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

#include "dtcpool.h"

// must be bitwise
enum {
	RS_IDLE = 0,
	RS_WAIT = 1,
	RS_SEND = 2,
	RS_RECV = 4,
	RS_DONE = 8,
};

enum {
	SS_CONNECTED = 0,
	SS_LINGER = 1,
	SS_CLOSED = 1,
	SS_CONNECTING = 2,
};

#define MAXREQID 2147483646L

NCPool::NCPool(int ms, int mr)
	:
	PollerUnit(1024+16)
{
	int i;

	if(ms > (1<<20))
		ms = 1<<20;
	if(mr > (1<<20))
		mr = 1<<20;

	maxServers = ms;
	maxRequests = mr;
	maxRequestId = MAXREQID / maxRequests;

	serverList = new NCServerInfo[maxServers];
	// needn't zero init
	numServers = 0;

	transList = new NCTransation[maxRequests];
	for(i=0; i<maxRequests; i++)
	{
		transList[i].ListAdd(&freeList);;
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
	if(transList)
	{
		for(i=0; i<maxRequests; i++)
		{
		}

		DELETE_ARRAY(transList);
	}
	DELETE_ARRAY(buf);
}

/* add server to pool */
int NCPool::AddServer(NCServer *server, int mReq, int mConn)
{
	if(server->ownerPool != NULL)
		return -EINVAL;

	if(numServers >= maxServers)
		return -E2BIG;

	if(server->IsDgram())
	{
		if(mConn == 0)
			mConn = 1;
		else if(mConn != 1)
			return -EINVAL;
	} else {
		if(mConn == 0)
			mConn = mReq;
		else if(mConn > mReq)
			return -EINVAL;
	}

	server->SetOwner(this, numServers);
	serverList[numServers].Init(server, mReq, mConn);
	numServers++;
	return 0;
}

/* switch basic server to async mode */
void NCServer::SetOwner(NCPool *owner, int id)
{
	Close();
	ownerId = id;;
	ownerPool = owner;
	INC();
}

/* async connect, high level */
int NCServer::AsyncConnect(int &netfd)
{
	int err = -EC_NOT_INITIALIZED;
	netfd = -1;

	if(addr.socket_family() != 0) {
		netfd = addr.create_socket();
		if(netfd < 0) {
			err = -errno;
		} else if(addr.socket_family()==AF_UNIX && IsDgram() && BindTempUnixSocket() < 0) {
			err = -errno;
			close(netfd);
			netfd = -1;
		} else {
			fcntl(netfd, F_SETFL, O_RDWR|O_NONBLOCK);
			if(addr.connect_socket(netfd)==0)
				return 0;
			err = -errno;
			if(err!=-EINPROGRESS) 
			{
				close(netfd);
				netfd = -1;
			}
		}
	}
	return err;
}

NCConnection::NCConnection(NCPool *owner, NCServerInfo *si)
	:
	PollerObject(owner),
	timer(this)
{
	serverInfo = si;
	sreq = NULL;
	state = SS_CLOSED;
	result = NULL;
}

NCConnection::~NCConnection(void)
{
	if(state==SS_CONNECTING)
		/* decrease connecting count */
		serverInfo->ConnectingDone();
	/* remove partial result */
	DELETE(result);
	/* remove from all connection queue */
	list_del();
	/* abort all associated requests */
	AbortRequests(-EC_REQUEST_ABORTED);
	serverInfo->MoreClosedConnectionAndReady();
}

/* connection is idle again */
void NCConnection::SwitchToIdle(void)
{
	if(state==SS_CONNECTING)
		/* decrease connecting count */
		serverInfo->ConnectingDone();
	ListMove(&serverInfo->idleList);
	serverInfo->ConnectionIdleAndReady();
	state = SS_CONNECTED;
	disable_output();
	enable_input();
	timer.disable_timer();
	/* ApplyEvnets by caller */
}

/* prepare connecting state, wait for EPOLLOUT */
void NCConnection::SwitchToConnecting(void)
{
	state = SS_CONNECTING;
	timer.attach_timer(serverInfo->timerList);
	ListMove(&serverInfo->busyList);
	DisableInput();
	enable_output();
	// no apply events, following AttachPoller will do it */
}

/* try async connecting */
int NCConnection::Connect(void)
{
	int err = serverInfo->info->AsyncConnect(netfd);
	if(err == 0) {
		SwitchToIdle();
		/* treat epollsize overflow as fd overflow */
		if(attach_poller() < 0)
			return -EMFILE;
		/* AttachPoller will ApplyEvents automatically */
	} else if(err == -EINPROGRESS) {
		SwitchToConnecting();
		/* treat epollsize overflow as fd overflow */
		if(attach_poller() < 0)
			return -EMFILE;
		/* AttachPoller will ApplyEvents automatically */
	}

	return err;
}

/* Link connection & transation */
void NCConnection::ProcessRequest(NCTransation *r)
{
	/* linkage between connection & transation */
	sreq = r;
	sreq->AttachConnection(this);

	/* adjust server connection statistics */
	list_move_tail(&serverInfo->busyList);
	serverInfo->RequestScheduled();

	/* initial timing and flushing */
	timer.attach_timer(serverInfo->timerList);
	SendRequest();
}

/* abort all requests associated transation */
void NCConnection::AbortRequests(int err)
{
	if(sreq)
	{
		sreq->Abort(err);
		sreq = NULL;
	}
	while(reqList.ListEmpty()==0)
	{
		NCTransation *req = reqList.NextOwner();
		req->Abort(err);
	}
}

/* abort sending transation, linger connection for RECV request */
void NCConnection::AbortSendSide(int err)
{
	if(sreq)
	{
		sreq->Abort(err);
		sreq = NULL;
	}
	if(!IsAsync() || reqList.ListEmpty())
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
	AbortRequests(err);
	if(IsDgram()==0)
		delete this;
	/* UDP has not connection close */
}

/* a valid result received */
void NCConnection::DoneResult(void)
{
	if(result)
	{
		/* searching SN */
		ListObject<NCTransation> *reqPtr = reqList.ListNext();
		while(reqPtr != &reqList)
		{
			NCTransation *req = reqPtr->ListOwner();
			if(req->MatchSN(result))
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
	if(IsAsync()==0) {
		/*
		 * SYNC server, switch to idle state,
		 * ASYNC server always idle, no switch needed
		 */
		SwitchToIdle();
		apply_events();
	} else if(state == SS_LINGER) {
		/* close LINGER connection if all result done */
		if(reqList.ListEmpty())
			delete this;
	}
	/*
	// disabled because async server should trigger by req->Done
	else
		serverInfo->MarkAsReady();
		*/
}

/* hangup by recv zero bytes */
int NCConnection::CheckHangup(void)
{
	if(IsAsync())
		return 0;
	char buf[1];
	int n = recv(netfd, buf, sizeof(buf), MSG_DONTWAIT|MSG_PEEK);
	return n >= 0;
}

/*
 * sending result, may called by other component
 * apply events is required
 */
void NCConnection::SendRequest(void)
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
			serverInfo->RequestSent();
			/* fire up receiving timer */
			timer.attach_timer(serverInfo->timerList);
			if(IsAsync())
			{
				list_move_tail(&serverInfo->idleList); 
				serverInfo->ConnectionIdleAndReady();
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
			AbortSendSide(-ECONNRESET);
			break;
	}
}

int NCConnection::RecvResult(void)
{
	int ret;
	if(result==NULL)
	{
		result = new NCResult(serverInfo->info->tdef);
		receiver.attach(netfd);
		receiver.erase();
	}
	if(IsDgram())
	{
		if(serverInfo->owner->buf==NULL)
			serverInfo->owner->buf = new char[65536];
		ret = recv(netfd, serverInfo->owner->buf, 65536, 0);
		if(ret <= 0)
			ret = DecodeFatalError;
		else
			ret = result->Decode(serverInfo->owner->buf, ret);
	} else
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
			DoneResult();
			// more result maybe available
			ret = 1;
			break;

		case DecodeIdle:
		case DecodeWaitData:
			// partial or no result available yet
			ret = 0;
			break;
			
		case DecodeDone:
			serverInfo->info->SaveDefinition(result);
			DoneResult();
			// more result maybe available
			ret = 1;
			break;
	}
	if(sreq || !reqList.ListEmpty())
		timer.attach_timer(serverInfo->timerList);
	return ret;
}

void NCConnection::InputNotify(void)
{
	switch(state) {
		case SS_CONNECTED:
			while(1) {
				if(RecvResult() <= 0)
					break;
			}
			break;
		case SS_LINGER:
			while(reqList.ListEmpty()==0) {
				if(RecvResult() <= 0)
					break;
			}
		default:
			break;
	}
}

void NCConnection::OutputNotify(void)
{
	switch(state)
	{
	case SS_CONNECTING:
		SwitchToIdle();
		break;
	case SS_CONNECTED:
		SendRequest();
		break;
	default:
		disable_output();
		break;
	}
}

void NCConnection::HangupNotify(void)
{
	if(state==SS_CONNECTING) {
		serverInfo->ConnectingFailed();
	}
	AbortRequests(-ECONNRESET);
	delete this;
}

void NCConnection::timer_notify(void)
{
	if(sreq || !reqList.ListEmpty())
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
	while(!idleList.ListEmpty())
	{
		NCConnection *conn = idleList.NextOwner();
		delete conn;
	}

	while(!busyList.ListEmpty())
	{
		NCConnection *conn = busyList.NextOwner();
		delete conn;
	}

	while(!reqList.ListEmpty())
	{
		NCTransation *trans = reqList.NextOwner();
		trans->Abort(-EC_REQUEST_ABORTED);
	}

	if(info && info->DEC()==0)
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
	int to = server->GetTimeout();
	if(to <= 0) to = 3600 * 1000; // 1 hour
	timerList = owner->get_timer_list_by_m_seconds( to );

	mode = server->IsDgram() ? 2 : maxReq==maxConn ? 0 : 1;
}

int NCServerInfo::Connect(void)
{
	NCConnection *conn = new NCConnection(owner, this);
	connRemain--;
	int err = conn->Connect();
	if(err==0) {
		/* pass */;
	} else if(err==-EINPROGRESS) {
		connConnecting++;
		err = 0; /* NOT a ERROR */
	} else {
		connRemain++;
		delete conn;
		connError++;
	}
	return err;
}

void NCServerInfo::AbortWaitQueue(int err)
{
	while(!reqList.ListEmpty())
	{
		NCTransation *trans = reqList.NextOwner();
		trans->Abort(err);
	}
}

void NCServerInfo::timer_notify()
{
	while(reqRemain>0 && !reqList.ListEmpty())
	{
		if(!idleList.ListEmpty())
		{
			// has something to do
			NCConnection *conn = idleList.NextOwner();

			conn->ProcessRequest(GetRequestFromQueue());
		} else {
			// NO connection available
			if(connRemain == 0)
				break;
			// need more connection to process
			if(connConnecting >= reqWait)
				break;

			int err;
			// connect error, abort processing
			if((err=Connect()) != 0)
			{
				if(connRemain>=connTotal)
					AbortWaitQueue(err);
				break;
			}
		}
	}
	/* no more work, clear bogus ready timer */
	TimerObject::disable_timer();
	/* reset connect error count */
	connError = 0;
}

void NCServerInfo::RequestDoneAndReady(int state) {
	switch(state) {
		case RS_WAIT:
			reqWait--; // Abort a queued not really ready
			break;
		case RS_SEND:
			reqSend--;
			reqRemain++;
			MarkAsReady();
			break;
		case RS_RECV:
			reqRecv--;
			reqRemain++;
			MarkAsReady();
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
	reqTag = 0;;
	server = NULL;
	packet = NULL;
	result = NULL;
}

/* get transation result */
NCResult * NCTransation::GetResult(void) {
	NCResult *ret = result;
	ret->SetApiTag(reqTag);
	DELETE(packet);
	Clear();
	return ret;
}

/* attach new request to transation slot */
int NCTransation::AttachRequest(NCServerInfo *info, long long tag, NCRequest *req, DTCValue *key) {
	state = RS_WAIT;
	reqTag = tag;
	server = info;
	packet = new Packet;
	int ret = req->Encode(key, packet);
	if(ret < 0) {
		/* encode error */
		result = new NCResult(ret, "API::encoding", "client encode packet error");
	} else {
		SN = server->info->LastSerialNr();
	}
	return ret;
}

extern inline void NCTransation::AttachConnection(NCConnection *c) {
	state = RS_SEND;
	conn = c;
}

extern inline void NCTransation::SendOK(ListObject<NCTransation> *queue) {
	state = RS_RECV;
	ListAddTail(queue);
}

extern inline void NCTransation::RecvOK(ListObject<NCTransation> *queue) {
	state = RS_DONE;
	ListAddTail(queue);
}

/* abort current transation, zero means succ */
void NCTransation::Abort(int err)
{
	if(server == NULL) // empty slot or done
		return;

	//Don't reverse abort connection, just clear linkage
	//NCConnection *c = conn;
	//DELETE(c);
	conn = NULL;

	NCPool *owner = server->owner;
	list_del();

	server->RequestDoneAndReady(state);
	server = NULL;

	if(err)
	{
		result = new NCResult(err, "API::aborted", "client abort request");
	}
	
	owner->TransationFinished(this);
}

NCResult *NCPool::GetTransationResult(NCTransation *req)
{
	NCResult *ret = req->GetResult();
	doneRequests--;
	numRequests--;
	req->ListAdd(&freeList);
	return ret;
}

/* get transation slot for new request */
NCTransation * NCPool::GetTransationSlot(void)
{
	NCTransation *trans = freeList.NextOwner();
	trans->Clear();
	trans->RoundGenId(maxRequestId);
	numRequests ++;
	return trans;
}

void NCPool::TransationFinished(NCTransation *trans)
{
	trans->RecvOK(&doneList);
	doneRequests++;
}

int NCPool::AddRequest(NCRequest *req, long long tag, DTCValue *key)
{
	int ret;
	if(numRequests >= maxRequests)
	{
		return -E2BIG;
	}

	if(req->server == NULL)
	{
	    return -EC_NOT_INITIALIZED;
	}

	NCServer *server = req->server;
	if(server->ownerPool == NULL)
	{
		ret = AddServer(server, 1);
		if(ret < 0)
			return ret;
	}

	if(server->ownerPool != this)
	{
		return -EINVAL;
	}

	if(key==NULL && req->haskey) key = &req->key;

	NCTransation *trans = GetTransationSlot();
	NCServerInfo *info = &serverList[server->ownerId];

	if(trans->AttachRequest(info, tag, req, key) < 0)
	{
		// attach error, result already prepared */
		TransationFinished(trans);
	} else {
		info->QueueRequest(trans);
	}

	return (trans-transList) + trans->GenId() * maxRequests + 1;
}

void NCPool::ExecuteOneLoop(int timeout)
{
	wait_poller_events(timeout);
	uint64_t now = GET_TIMESTAMP();
	process_poller_events();
	check_expired(now);
	check_ready();
}

int NCPool::Execute(int timeout)
{
	if(timeout > 3600000) timeout = 3600000;
	InitializePollerUnit();
	ExecuteOneLoop(0);
	if(doneRequests > 0)
		return doneRequests;
	int64_t till = GET_TIMESTAMP() + timeout * TIMESTAMP_PRECISION / 1000;
	while(doneRequests <= 0)
	{
		int64_t exp = till - GET_TIMESTAMP();
		if(exp < 0)
			break;
		int msec;
#if TIMESTAMP_PRECISION > 1000
		msec = exp / (TIMESTAMP_PRECISION/1000);
#else
		msec = exp * 1000/TIMESTAMP_PRECISION;
#endif
		ExecuteOneLoop(expire_micro_seconds(msec, 1));
	}
	return doneRequests;
}

int NCPool::ExecuteAll(int timeout)
{
	if(timeout > 3600000) timeout = 3600000;
	InitializePollerUnit();
	ExecuteOneLoop(0);
	if(numRequests <= doneRequests)
		return doneRequests;
	int64_t till = GET_TIMESTAMP() + timeout * TIMESTAMP_PRECISION / 1000;
	while(numRequests > doneRequests)
	{
		int64_t exp = till - GET_TIMESTAMP();
		if(exp < 0)
			break;
		int msec;
#if TIMESTAMP_PRECISION > 1000
		msec = exp / (TIMESTAMP_PRECISION/1000);
#else
		msec = exp * 1000/TIMESTAMP_PRECISION;
#endif
		ExecuteOneLoop(expire_micro_seconds(msec, 1));
	}
	return doneRequests;
}

int NCPool::InitializePollerUnit(void)
{
	if(initFlag==-1)
	{
		initFlag = PollerUnit::initialize_poller_unit() >= 0;
	}
	return initFlag;
}

int NCPool::GetEpollFD(int mp)
{
	if(initFlag == -1 && mp > PollerUnit::get_max_pollers())
		PollerUnit::set_max_pollers(mp);
	InitializePollerUnit();
	return PollerUnit::get_fd();
}

NCTransation * NCPool::Id2Req(int reqId) const
{
	if(reqId <= 0 || reqId > MAXREQID)
		return NULL;
	reqId--;
	NCTransation *req = &transList[reqId % maxRequests];
	if(reqId / maxRequests != req->GenId())
		return NULL;
	return req;
}


int NCPool::AbortRequest(NCTransation *req)
{
	if(req->conn)
	{
		/* send or recv state */
		if(req->server->mode==0)
		{	// SYNC, always close connecction
			req->conn->Close(-EC_REQUEST_ABORTED);
		} else if(req->server->mode==1)
		{	// ASYNC, linger connection for other result, abort send side
			if(req->State()==RS_SEND)
				req->conn->AbortSendSide(-EC_REQUEST_ABORTED);
			else 
				req->Abort(-EC_REQUEST_ABORTED);
		} else { // UDP, abort request only
			req->Abort(-EC_REQUEST_ABORTED);
		}
		return 1;
	}
	else if(req->server)
	{
		/* waiting state, abort request only */
		req->Abort(-EC_REQUEST_ABORTED);
		return 1;
	}
	return 0;
}

int NCPool::CancelRequest(int reqId)
{
	NCTransation *req = Id2Req(reqId);
	if(req==NULL)
		return -EINVAL;

	AbortRequest(req);
	NCResult *res = GetTransationResult(req);
	DELETE(res);
	return 1;
}

int NCPool::CancelAllRequest(int type)
{
	int n = 0;
	if((type & ~0xf) != 0)
		return -EINVAL;
	for(int i=0; i<maxRequests; i++)
	{
		NCTransation *req = &transList[i];
		if((type & req->State()))
		{
			AbortRequest(req);
			NCResult *res = GetTransationResult(req);
			DELETE(res);
			n++;
		}
	}
	return n;
}

int NCPool::AbortRequest(int reqId)
{
	NCTransation *req = Id2Req(reqId);
	if(req==NULL)
		return -EINVAL;

	return AbortRequest(req);
}

int NCPool::AbortAllRequest(int type)
{
	int n = 0;
	if((type & ~0xf) != 0)
		return -EINVAL;
	type &= ~RS_DONE;
	for(int i=0; i<maxRequests; i++)
	{
		NCTransation *req = &transList[i];
		if((type & req->State()))
		{
			AbortRequest(req);
			n++;
		}
	}
	return n;
}

NCResult *NCPool::GetResult(int reqId)
{
	NCTransation *req;
	if(reqId==0)
	{
		if(doneList.ListEmpty())
			return (NCResult *)-EAGAIN;
		req = doneList.NextOwner();
	} else {
		req = Id2Req(reqId);
		if(req==NULL) return (NCResult *)-EINVAL;

		if(req->State() != RS_DONE)
			return (NCResult *)req->State();
	}
	return GetTransationResult(req);
}

int NCServerInfo::CountRequestState(int type) const
{
	int n = 0;
	if((type & RS_WAIT))
		n += reqWait;
	if((type & RS_SEND))
		n += reqSend;
	if((type & RS_RECV))
		n += reqRecv;
	return n;
}

int NCPool::CountRequestState(int type) const
{
	int n = 0;
#if 0
	for(int i=0; i<maxRequests; i++)
	{
		if((type & transList[i].State()))
			n++;
	}
#else
	for(int i=0; i<maxServers; i++)
	{
		if(serverList[i].info==NULL)
			continue;
		n += serverList[i].CountRequestState(type);
	}
	if((type & RS_DONE))
		n += doneRequests;
#endif
	return n;
}

int NCPool::RequestState(int reqId) const
{
	NCTransation *req = Id2Req(reqId);
	if(req==NULL)
		return -EINVAL;

	return req->State();
}

