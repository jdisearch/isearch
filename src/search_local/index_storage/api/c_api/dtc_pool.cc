/*
 * =====================================================================================
 *
 *       Filename:  dtc_pool.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <linux/sockios.h>

#include "unix_socket.h"
#include <fcntl.h>
#include <sys/poll.h>
#include <errno.h>
#include <netinet/in.h>
#include "myepoll.h"

#include "dtc_pool.h"

// must be bitwise
enum
{
	RS_IDLE = 0,
	RS_WAIT = 1,
	RS_SEND = 2,
	RS_RECV = 4,
	RS_DONE = 8,
};

enum
{
	SS_CONNECTED = 0,
	SS_LINGER = 1,
	SS_CLOSED = 1,
	SS_CONNECTING = 2,
};

#define MAXREQID 2147483646L

NCPool::NCPool(int ms, int mr)
	: PollerUnit(1024 + 16)
{
	int i;

	if (ms > (1 << 20))
		ms = 1 << 20;
	if (mr > (1 << 20))
		mr = 1 << 20;

	maxServers = ms;
	maxRequests = mr;
	maxRequestId = MAXREQID / maxRequests;

	serverList = new NCServerInfo[maxServers];
	// needn't zero init
	numServers = 0;

	transList = new NCTransation[maxRequests];
	for (i = 0; i < maxRequests; i++)
	{
		transList[i].ListAdd(&freeList);
		;
	}
	numRequests = 0;

	doneRequests = 0;

	initFlag = -1;
	buf = NULL;
}

NCPool::~NCPool()
{
	int i;

	DELETE_ARRAY(serverList);
	if (transList)
	{
		for (i = 0; i < maxRequests; i++)
		{
		}

		DELETE_ARRAY(transList);
	}
	DELETE_ARRAY(buf);
}

/* add server to pool */
int NCPool::add_server(NCServer *server, int mReq, int mConn)
{
	if (server->ownerPool != NULL)
		return -EINVAL;

	if (numServers >= maxServers)
		return -E2BIG;

	if (server->is_dgram())
	{
		if (mConn == 0)
			mConn = 1;
		else if (mConn != 1)
			return -EINVAL;
	}
	else
	{
		if (mConn == 0)
			mConn = mReq;
		else if (mConn > mReq)
			return -EINVAL;
	}

	server->set_owner(this, numServers);
	serverList[numServers].Init(server, mReq, mConn);
	numServers++;
	return 0;
}

/* switch basic server to async mode */
void NCServer::set_owner(NCPool *owner, int id)
{
	Close();
	ownerId = id;
	;
	ownerPool = owner;
	INC();
}

/* async connect, high level */
int NCServer::async_connect(int &netfd)
{
	int err = -EC_NOT_INITIALIZED;
	netfd = -1;

	if (addr.socket_family() != 0)
	{
		netfd = addr.create_socket();
		if (netfd < 0)
		{
			err = -errno;
		}
		else if (addr.socket_family() == AF_UNIX && is_dgram() && bind_temp_unix_socket() < 0)
		{
			err = -errno;
			close(netfd);
			netfd = -1;
		}
		else
		{
			fcntl(netfd, F_SETFL, O_RDWR | O_NONBLOCK);
			if (addr.connect_socket(netfd) == 0)
				return 0;
			err = -errno;
			if (err != -EINPROGRESS)
			{
				close(netfd);
				netfd = -1;
			}
		}
	}
	return err;
}

NCConnection::NCConnection(NCPool *owner, NCServerInfo *si)
	: PollerObject(owner),
	  timer(this)
{
	serverInfo = si;
	sreq = NULL;
	state = SS_CLOSED;
	result = NULL;
}

NCConnection::~NCConnection(void)
{
	if (state == SS_CONNECTING)
		/* decrease connecting count */
		serverInfo->connecting_done();
	/* remove partial result */
	DELETE(result);
	/* remove from all connection queue */
	list_del();
	/* abort all associated requests */
	abort_requests(-EC_REQUEST_ABORTED);
	serverInfo->more_closed_connection_and_ready();
}

/* connection is idle again */
void NCConnection::switch_to_idle(void)
{
	if (state == SS_CONNECTING)
		/* decrease connecting count */
		serverInfo->connecting_done();
	ListMove(&serverInfo->idleList);
	serverInfo->connection_idle_and_ready();
	state = SS_CONNECTED;
	disable_output();
	enable_input();
	timer.disable_timer();
	/* ApplyEvnets by caller */
}

/* prepare connecting state, wait for EPOLLOUT */
void NCConnection::switch_to_connecting(void)
{
	state = SS_CONNECTING;
	timer.attach_timer(serverInfo->timerList);
	ListMove(&serverInfo->busyList);
	DisableInput();
	enable_output();
	// no apply events, following attach_poller will do it */
}

/* try async connecting */
int NCConnection::Connect(void)
{
	int err = serverInfo->info->async_connect(netfd);
	if (err == 0)
	{
		switch_to_idle();
		/* treat epollsize overflow as fd overflow */
		if (attach_poller() < 0)
			return -EMFILE;
		/* attach_poller will apply_events automatically */
	}
	else if (err == -EINPROGRESS)
	{
		switch_to_connecting();
		/* treat epollsize overflow as fd overflow */
		if (attach_poller() < 0)
			return -EMFILE;
		/* attach_poller will apply_events automatically */
	}

	return err;
}

/* Link connection & transation */
void NCConnection::process_request(NCTransation *r)
{
	/* linkage between connection & transation */
	sreq = r;
	sreq->attach_connection(this);

	/* adjust server connection statistics */
	list_move_tail(&serverInfo->busyList);
	serverInfo->request_scheduled();

	/* initial timing and flushing */
	timer.attach_timer(serverInfo->timerList);
	send_request();
}

/* abort all requests associated transation */
void NCConnection::abort_requests(int err)
{
	if (sreq)
	{
		sreq->Abort(err);
		sreq = NULL;
	}
	while (reqList.ListEmpty() == 0)
	{
		NCTransation *req = reqList.NextOwner();
		req->Abort(err);
	}
}

/* abort sending transation, linger connection for RECV request */
void NCConnection::abort_send_side(int err)
{
	if (sreq)
	{
		sreq->Abort(err);
		sreq = NULL;
	}
	if (!is_async() || reqList.ListEmpty())
		Close(err);
	else
	{
		// async connection, lingering
		ListMove(&serverInfo->busyList);
		state = SS_LINGER;
		timer.attach_timer(serverInfo->timerList);
	}
}

/* close a connection */
void NCConnection::Close(int err)
{
	abort_requests(err);
	if (is_dgram() == 0)
		delete this;
	/* UDP has not connection close */
}

/* a valid result received */
void NCConnection::done_result(void)
{
	if (result)
	{
		/* searching SN */
		ListObject<NCTransation> *reqPtr = reqList.ListNext();
		while (reqPtr != &reqList)
		{
			NCTransation *req = reqPtr->ListOwner();
			if (req->MatchSN(result))
			{
				/* SN matched, transation is dont */
				req->Done(result);
				result = NULL;
				break;
			}
			reqPtr = reqPtr->ListNext();
		}
		DELETE(result);
	}
	if (is_async() == 0)
	{
		/*
		 * SYNC server, switch to idle state,
		 * ASYNC server always idle, no switch needed
		 */
		switch_to_idle();
		apply_events();
	}
	else if (state == SS_LINGER)
	{
		/* close LINGER connection if all result done */
		if (reqList.ListEmpty())
			delete this;
	}
	/*
	// disabled because async server should trigger by req->Done
	else
		serverInfo->mark_as_ready();
		*/
}

/* hangup by recv zero bytes */
int NCConnection::check_hangup(void)
{
	if (is_async())
		return 0;
	char buf[1];
	int n = recv(netfd, buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK);
	return n >= 0;
}

/*
 * sending result, may called by other component
 * apply events is required
 */
void NCConnection::send_request(void)
{
	int ret = sreq->Send(netfd);
	timer.disable_timer();
	switch (ret)
	{
	case SendResultMoreData:
		/* more data to send, enable EPOLLOUT and timer */
		enable_output();
		apply_events();
		timer.attach_timer(serverInfo->timerList);
		break;

	case SendResultDone:
		/* send OK, disable output and enable receiving */
		disable_output();
		enable_input();
		sreq->SendOK(&reqList);
		sreq = NULL;
		serverInfo->request_sent();
		/* fire up receiving timer */
		timer.attach_timer(serverInfo->timerList);
		if (is_async())
		{
			list_move_tail(&serverInfo->idleList);
			serverInfo->connection_idle_and_ready();
		}
#if 0
			else {
				/* fire up receiving logic */
				RecvResult();
			}
#endif
		apply_events();
		break;

	default:
		abort_send_side(-ECONNRESET);
		break;
	}
}

int NCConnection::RecvResult(void)
{
	int ret;
	if (result == NULL)
	{
		result = new NCResult(serverInfo->info->tdef);
		receiver.attach(netfd);
		receiver.erase();
	}
	if (is_dgram())
	{
		if (serverInfo->owner->buf == NULL)
			serverInfo->owner->buf = new char[65536];
		ret = recv(netfd, serverInfo->owner->buf, 65536, 0);
		if (ret <= 0)
			ret = DecodeFatalError;
		else
			ret = result->Decode(serverInfo->owner->buf, ret);
	}
	else
		ret = result->Decode(receiver);
	timer.disable_timer();
	switch (ret)
	{
	default:
	case DecodeFatalError:
		Close(-ECONNRESET);
		return -1; // connection bad
		break;

	case DecodeDataError:
		done_result();
		// more result maybe available
		ret = 1;
		break;

	case DecodeIdle:
	case DecodeWaitData:
		// partial or no result available yet
		ret = 0;
		break;

	case DecodeDone:
		serverInfo->info->save_definition(result);
		done_result();
		// more result maybe available
		ret = 1;
		break;
	}
	if (sreq || !reqList.ListEmpty())
		timer.attach_timer(serverInfo->timerList);
	return ret;
}

void NCConnection::input_notify(void)
{
	switch (state)
	{
	case SS_CONNECTED:
		while (1)
		{
			if (RecvResult() <= 0)
				break;
		}
		break;
	case SS_LINGER:
		while (reqList.ListEmpty() == 0)
		{
			if (RecvResult() <= 0)
				break;
		}
	default:
		break;
	}
}

void NCConnection::output_notify(void)
{
	switch (state)
	{
	case SS_CONNECTING:
		switch_to_idle();
		break;
	case SS_CONNECTED:
		send_request();
		break;
	default:
		disable_output();
		break;
	}
}

void NCConnection::hangup_notify(void)
{
	if (state == SS_CONNECTING)
	{
		serverInfo->connecting_failed();
	}
	abort_requests(-ECONNRESET);
	delete this;
}

void NCConnection::timer_notify(void)
{
	if (sreq || !reqList.ListEmpty())
	{
		Close(-ETIMEDOUT);
	}
}

NCServerInfo::NCServerInfo(void)
{
	info = NULL;
}

NCServerInfo::~NCServerInfo(void)
{
	while (!idleList.ListEmpty())
	{
		NCConnection *conn = idleList.NextOwner();
		delete conn;
	}

	while (!busyList.ListEmpty())
	{
		NCConnection *conn = busyList.NextOwner();
		delete conn;
	}

	while (!reqList.ListEmpty())
	{
		NCTransation *trans = reqList.NextOwner();
		trans->Abort(-EC_REQUEST_ABORTED);
	}

	if (info && info->DEC() == 0)
		delete info;
}

void NCServerInfo::Init(NCServer *server, int maxReq, int maxConn)
{
	info = server;
	reqList.ResetList();
	idleList.ResetList();
	busyList.ResetList();

	reqWait = 0;
	reqSend = 0;
	reqRecv = 0;
	reqRemain = maxReq;

	connRemain = maxConn;
	connTotal = maxConn;
	connConnecting = 0;
	connError = 0;
	owner = server->ownerPool;
	int to = server->get_timeout();
	if (to <= 0)
		to = 3600 * 1000; // 1 hour
	timerList = owner->get_timer_list_by_m_seconds(to);

	mode = server->is_dgram() ? 2 : maxReq == maxConn ? 0 : 1;
}

int NCServerInfo::Connect(void)
{
	NCConnection *conn = new NCConnection(owner, this);
	connRemain--;
	int err = conn->Connect();
	if (err == 0)
	{
		/* pass */;
	}
	else if (err == -EINPROGRESS)
	{
		connConnecting++;
		err = 0; /* NOT a ERROR */
	}
	else
	{
		connRemain++;
		delete conn;
		connError++;
	}
	return err;
}

void NCServerInfo::abort_wait_queue(int err)
{
	while (!reqList.ListEmpty())
	{
		NCTransation *trans = reqList.NextOwner();
		trans->Abort(err);
	}
}

void NCServerInfo::timer_notify()
{
	while (reqRemain > 0 && !reqList.ListEmpty())
	{
		if (!idleList.ListEmpty())
		{
			// has something to do
			NCConnection *conn = idleList.NextOwner();

			conn->process_request(get_request_from_queue());
		}
		else
		{
			// NO connection available
			if (connRemain == 0)
				break;
			// need more connection to process
			if (connConnecting >= reqWait)
				break;

			int err;
			// connect error, abort processing
			if ((err = Connect()) != 0)
			{
				if (connRemain >= connTotal)
					abort_wait_queue(err);
				break;
			}
		}
	}
	/* no more work, clear bogus ready timer */
	TimerObject::disable_timer();
	/* reset connect error count */
	connError = 0;
}

void NCServerInfo::request_done_and_ready(int state)
{
	switch (state)
	{
	case RS_WAIT:
		reqWait--; // Abort a queued not really ready
		break;
	case RS_SEND:
		reqSend--;
		reqRemain++;
		mark_as_ready();
		break;
	case RS_RECV:
		reqRecv--;
		reqRemain++;
		mark_as_ready();
		break;
	}
}

NCTransation::NCTransation(void)
{
}

NCTransation::~NCTransation(void)
{
	DELETE(packet);
	DELETE(result);
}

/* clear transation state */
void NCTransation::Clear(void)
{
	// detach from list, mostly freeList
	ListObject<NCTransation>::ResetList();

	// increase reqId
	genId++;

	// reset local state
	state = RS_IDLE;
	reqTag = 0;
	;
	server = NULL;
	packet = NULL;
	result = NULL;
}

/* get transation result */
NCResult *NCTransation::get_result(void)
{
	NCResult *ret = result;
	ret->set_api_tag(reqTag);
	DELETE(packet);
	Clear();
	return ret;
}

/* attach new request to transation slot */
int NCTransation::attach_request(NCServerInfo *info, long long tag, NCRequest *req, DTCValue *key)
{
	state = RS_WAIT;
	reqTag = tag;
	server = info;
	packet = new Packet;
	int ret = req->Encode(key, packet);
	if (ret < 0)
	{
		/* encode error */
		result = new NCResult(ret, "API::encoding", "client encode packet error");
	}
	else
	{
		SN = server->info->LastSerialNr();
	}
	return ret;
}

extern inline void NCTransation::attach_connection(NCConnection *c)
{
	state = RS_SEND;
	conn = c;
}

extern inline void NCTransation::SendOK(ListObject<NCTransation> *queue)
{
	state = RS_RECV;
	ListAddTail(queue);
}

extern inline void NCTransation::RecvOK(ListObject<NCTransation> *queue)
{
	state = RS_DONE;
	ListAddTail(queue);
}

/* abort current transation, zero means succ */
void NCTransation::Abort(int err)
{
	if (server == NULL) // empty slot or done
		return;

	//Don't reverse abort connection, just clear linkage
	//NCConnection *c = conn;
	//DELETE(c);
	conn = NULL;

	NCPool *owner = server->owner;
	list_del();

	server->request_done_and_ready(state);
	server = NULL;

	if (err)
	{
		result = new NCResult(err, "API::aborted", "client abort request");
	}

	owner->transation_finished(this);
}

NCResult *NCPool::get_transation_result(NCTransation *req)
{
	NCResult *ret = req->get_result();
	doneRequests--;
	numRequests--;
	req->ListAdd(&freeList);
	return ret;
}

/* get transation slot for new request */
NCTransation *NCPool::get_transation_slot(void)
{
	NCTransation *trans = freeList.NextOwner();
	trans->Clear();
	trans->round_gen_id(maxRequestId);
	numRequests++;
	return trans;
}

void NCPool::transation_finished(NCTransation *trans)
{
	trans->RecvOK(&doneList);
	doneRequests++;
}

int NCPool::add_request(NCRequest *req, long long tag, DTCValue *key)
{
	int ret;
	if (numRequests >= maxRequests)
	{
		return -E2BIG;
	}

	if (req->server == NULL)
	{
		return -EC_NOT_INITIALIZED;
	}

	NCServer *server = req->server;
	if (server->ownerPool == NULL)
	{
		ret = add_server(server, 1);
		if (ret < 0)
			return ret;
	}

	if (server->ownerPool != this)
	{
		return -EINVAL;
	}

	if (key == NULL && req->haskey)
		key = &req->key;

	NCTransation *trans = get_transation_slot();
	NCServerInfo *info = &serverList[server->ownerId];

	if (trans->attach_request(info, tag, req, key) < 0)
	{
		// attach error, result already prepared */
		transation_finished(trans);
	}
	else
	{
		info->queue_request(trans);
	}

	return (trans - transList) + trans->gen_id() * maxRequests + 1;
}

void NCPool::execute_one_loop(int timeout)
{
	wait_poller_events(timeout);
	uint64_t now = GET_TIMESTAMP();
	process_poller_events();
	check_expired(now);
	check_ready();
}

int NCPool::execute(int timeout)
{
	if (timeout > 3600000)
		timeout = 3600000;
	initialize_poller_unit();
	execute_one_loop(0);
	if (doneRequests > 0)
		return doneRequests;
	int64_t till = GET_TIMESTAMP() + timeout * TIMESTAMP_PRECISION / 1000;
	while (doneRequests <= 0)
	{
		int64_t exp = till - GET_TIMESTAMP();
		if (exp < 0)
			break;
		int msec;
#if TIMESTAMP_PRECISION > 1000
		msec = exp / (TIMESTAMP_PRECISION / 1000);
#else
		msec = exp * 1000 / TIMESTAMP_PRECISION;
#endif
		execute_one_loop(expire_micro_seconds(msec, 1));
	}
	return doneRequests;
}

int NCPool::execute_all(int timeout)
{
	if (timeout > 3600000)
		timeout = 3600000;
	initialize_poller_unit();
	execute_one_loop(0);
	if (numRequests <= doneRequests)
		return doneRequests;
	int64_t till = GET_TIMESTAMP() + timeout * TIMESTAMP_PRECISION / 1000;
	while (numRequests > doneRequests)
	{
		int64_t exp = till - GET_TIMESTAMP();
		if (exp < 0)
			break;
		int msec;
#if TIMESTAMP_PRECISION > 1000
		msec = exp / (TIMESTAMP_PRECISION / 1000);
#else
		msec = exp * 1000 / TIMESTAMP_PRECISION;
#endif
		execute_one_loop(expire_micro_seconds(msec, 1));
	}
	return doneRequests;
}

int NCPool::initialize_poller_unit(void)
{
	if (initFlag == -1)
	{
		initFlag = PollerUnit::initialize_poller_unit() >= 0;
	}
	return initFlag;
}

int NCPool::get_epoll_fd(int mp)
{
	if (initFlag == -1 && mp > PollerUnit::get_max_pollers())
		PollerUnit::set_max_pollers(mp);
	initialize_poller_unit();
	return PollerUnit::get_fd();
}

NCTransation *NCPool::Id2Req(int reqId) const
{
	if (reqId <= 0 || reqId > MAXREQID)
		return NULL;
	reqId--;
	NCTransation *req = &transList[reqId % maxRequests];
	if (reqId / maxRequests != req->gen_id())
		return NULL;
	return req;
}

int NCPool::abort_request(NCTransation *req)
{
	if (req->conn)
	{
		/* send or recv state */
		if (req->server->mode == 0)
		{ // SYNC, always close connecction
			req->conn->Close(-EC_REQUEST_ABORTED);
		}
		else if (req->server->mode == 1)
		{ // ASYNC, linger connection for other result, abort send side
			if (req->State() == RS_SEND)
				req->conn->abort_send_side(-EC_REQUEST_ABORTED);
			else
				req->Abort(-EC_REQUEST_ABORTED);
		}
		else
		{ // UDP, abort request only
			req->Abort(-EC_REQUEST_ABORTED);
		}
		return 1;
	}
	else if (req->server)
	{
		/* waiting state, abort request only */
		req->Abort(-EC_REQUEST_ABORTED);
		return 1;
	}
	return 0;
}

int NCPool::cancel_request(int reqId)
{
	NCTransation *req = Id2Req(reqId);
	if (req == NULL)
		return -EINVAL;

	abort_request(req);
	NCResult *res = get_transation_result(req);
	DELETE(res);
	return 1;
}

int NCPool::cancel_all_request(int type)
{
	int n = 0;
	if ((type & ~0xf) != 0)
		return -EINVAL;
	for (int i = 0; i < maxRequests; i++)
	{
		NCTransation *req = &transList[i];
		if ((type & req->State()))
		{
			abort_request(req);
			NCResult *res = get_transation_result(req);
			DELETE(res);
			n++;
		}
	}
	return n;
}

int NCPool::abort_request(int reqId)
{
	NCTransation *req = Id2Req(reqId);
	if (req == NULL)
		return -EINVAL;

	return abort_request(req);
}

int NCPool::abort_all_request(int type)
{
	int n = 0;
	if ((type & ~0xf) != 0)
		return -EINVAL;
	type &= ~RS_DONE;
	for (int i = 0; i < maxRequests; i++)
	{
		NCTransation *req = &transList[i];
		if ((type & req->State()))
		{
			abort_request(req);
			n++;
		}
	}
	return n;
}

NCResult *NCPool::get_result(int reqId)
{
	NCTransation *req;
	if (reqId == 0)
	{
		if (doneList.ListEmpty())
			return (NCResult *)-EAGAIN;
		req = doneList.NextOwner();
	}
	else
	{
		req = Id2Req(reqId);
		if (req == NULL)
			return (NCResult *)-EINVAL;

		if (req->State() != RS_DONE)
			return (NCResult *)req->State();
	}
	return get_transation_result(req);
}

int NCServerInfo::count_request_state(int type) const
{
	int n = 0;
	if ((type & RS_WAIT))
		n += reqWait;
	if ((type & RS_SEND))
		n += reqSend;
	if ((type & RS_RECV))
		n += reqRecv;
	return n;
}

int NCPool::count_request_state(int type) const
{
	int n = 0;
#if 0
	for(int i=0; i<maxRequests; i++)
	{
		if((type & transList[i].State()))
			n++;
	}
#else
	for (int i = 0; i < maxServers; i++)
	{
		if (serverList[i].info == NULL)
			continue;
		n += serverList[i].count_request_state(type);
	}
	if ((type & RS_DONE))
		n += doneRequests;
#endif
	return n;
}

int NCPool::request_state(int reqId) const
{
	NCTransation *req = Id2Req(reqId);
	if (req == NULL)
		return -EINVAL;

	return req->State();
}
