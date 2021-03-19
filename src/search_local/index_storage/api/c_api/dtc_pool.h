/*
 * =====================================================================================
 *
 *       Filename:  dtc_pool.h
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
#ifndef __CHC_CLI_POOL_H
#define __CHC_CLI_POOL_H

#include <sys/poll.h>

#include "list.h"
#include "poller.h"
#include "timer_list.h"
#include "dtc_int.h"
#include "buffer_error.h"

class NCServerInfo;
class NCPool;
class NCConnection;

// transation is a internal async request
class NCTransation : public ListObject<NCTransation>
{
public:
	// constructor/destructor equiv
	NCTransation(void);
	~NCTransation(void);
	/* clear transation state */
	void Clear(void);
	/* abort current transation, zero means succ */
	void Abort(int);
	/* transation succ with result */
	void Done(NCResult *res)
	{
		result = res;
		Abort(0);
	}
	/* get transation result */
	NCResult *get_result(void);
	/* adjust generation id */
	void round_gen_id(int m)
	{
		if (genId >= m)
			genId = 0;
	}

	/* state & info management */
	int State(void) const { return state; }
	int gen_id(void) const { return genId; }
	int MatchSN(NCResult *res) const { return SN == res->versionInfo.serial_nr(); }
	/* send packet management */
	int Send(int fd) { return packet->Send(fd); }
	int attach_request(NCServerInfo *s, long long tag, NCRequest *req, DTCValue *key);
	void attach_connection(NCConnection *c);
	void SendOK(ListObject<NCTransation> *);
	void RecvOK(ListObject<NCTransation> *);

public: // constant member declare as static
	// owner info, associated server
	NCServerInfo *server;
	// attached connection, SEND, RECV
	NCConnection *conn;

private: // transient members is private
	// current transation state, WAIT, SEND, RECV, DONE
	int state;
	// internal transation generation id
	int genId;
	// associated request tag
	long long reqTag;
	// associated request SN, state SEND, RECV
	uint64_t SN;

	// sending packet
	Packet *packet;

	// execute result
	NCResult *result;
};

class NCConnection : public ListObject<NCConnection>,
					 public PollerObject
{
public:
	NCConnection(NCPool *, NCServerInfo *);
	~NCConnection(void);

	int is_dgram(void) const;
	int is_async(void) const;
	/* starting state machine, by NCServerInfo::Connect */
	int Connect(void);
	/* attach transation to connection */
	void process_request(NCTransation *);
	/* flush send channel */
	void send_request(void);
	/* flush recv channel */
	int RecvResult(void);
	/* check connection hangup, recv zero bytes */
	int check_hangup(void);

	/* abort all associated request */
	void abort_requests(int err);
	/* close connection */
	void Close(int err);
	/* abort current sending request, linger recv channel */
	void abort_send_side(int err);
	/* a valid result received */
	void done_result(void);
	/* connection is idle, usable for transation */
	void switch_to_idle(void);
	/* connection is async connecting */
	void switch_to_connecting(void);

	virtual void input_notify(void);
	virtual void output_notify(void);
	virtual void hangup_notify(void);
	virtual void timer_notify(void);

private:
	/* associated server */
	NCServerInfo *serverInfo;
	/* queued transation in RECV state */
	ListObject<NCTransation> reqList;
	/* sending transation */
	NCTransation *sreq;
	/* decoding/decoded result */
	NCResult *result;
	/* connection state */
	int state;

	int NETFD(void) const { return netfd; }

private:
	TimerMember<NCConnection> timer;
	SimpleReceiver receiver;
};

typedef ListObject<NCConnection> NCConnectionList;

class NCPool : public PollerUnit,
			   public TimerUnit
{
public:
	NCPool(int maxServers, int maxRequests);
	~NCPool();

	int initialize_poller_unit(void);

	int get_epoll_fd(int maxpoller);
	int add_server(NCServer *srv, int maxReq = 1, int maxConn = 0);
	int add_request(NCRequest *req, long long tag, DTCValue *key = 0);

	void execute_one_loop(int timeout);
	int execute(int timeout);
	int execute_all(int timeout);

	NCTransation *Id2Req(int) const;
	int cancel_request(int);
	int cancel_all_request(int);
	int abort_request(NCTransation *);
	int abort_request(int);
	int abort_all_request(int);
	NCResult *get_result(void);
	NCResult *get_result(int);

	int count_request_state(int) const;
	int request_state(int) const;

public:
	NCTransation *get_transation_slot(void);
	void transation_finished(NCTransation *);
	NCResult *get_transation_result(NCTransation *);
	int get_transation_state(NCTransation *);

	int server_count(void) const { return numServers; }
	int request_count(void) const { return numRequests; }
	int done_request_count(void) const { return doneRequests; }

private:
	int initFlag;
	int maxServers;
	int maxRequests;
	int numServers;
	int numRequests;
	int maxRequestId;
	int doneRequests;
	ListObject<NCTransation> freeList;
	ListObject<NCTransation> doneList;
	NCServerInfo *serverList;
	NCTransation *transList;

public:
	char *buf;
};

class NCServerInfo : private TimerObject
{
public:
	NCServerInfo(void);
	~NCServerInfo(void);
	void Init(NCServer *, int, int);

	/* prepare wake timer_notify */
	void mark_as_ready() { attach_ready_timer(owner); }
	virtual void timer_notify(void);
	/* four reason server has more work to do */
	/* more transation attached */
	void more_request_and_ready(void)
	{
		reqWait++;
		mark_as_ready();
	}
	/* more close connection, should reconnecting */
	void more_closed_connection_and_ready(void)
	{
		connRemain++;
		mark_as_ready();
	}
	/* more idle connection available */
	void connection_idle_and_ready(void) { mark_as_ready(); }
	/* more request can assign to idle pool */
	void request_done_and_ready(int oldstate);

	/* one request scheduled to SEND state */
	void request_scheduled(void)
	{
		reqRemain--;
		reqSend++;
	}
	void request_sent(void)
	{
		reqSend--;
		reqRecv++;
	}

	/* queue transation to this server */
	void queue_request(NCTransation *req)
	{
		req->ListAddTail(&reqList);
		more_request_and_ready();
	}
	/* one connecting aborted */
	void connecting_failed(void) { connError++; }
	void connecting_done(void) { connConnecting--; }

	/* abort all waiting transations */
	void abort_wait_queue(int err);

	int count_request_state(int type) const;

	/* get a waiting transation */
	NCTransation *get_request_from_queue(void)
	{
		NCTransation *trans = reqList.NextOwner();
		trans->list_del();
		reqWait--;
		return trans;
	}

private:
	int Connect(void);
	ListObject<NCTransation> reqList;

public: // constant member declare as public
	/* associated ServerPool */
	NCPool *owner;
	/* basic server info */
	NCServer *info;
	TimerList *timerList;
	int mode; // 0--TCP 1--ASYNC 2--UDP
	/* total connection */
	int connTotal;

private: // transient member is private
	/* transation in state WAIT */
	int reqWait;
	/* transation in state SEND */
	int reqSend;
	/* transation in state RECV */
	int reqRecv;
	/* remain requests can assign to connection pool */
	int reqRemain;

	/* remain connections can connect */
	int connRemain;
	/* number connections in connecting state */
	int connConnecting;
	/* number of connect error this round */
	int connError;
	/* busy connection count */
	inline int conn_working(void) const { return connTotal - connConnecting - connRemain; }

public: //except idleList for code conventional
	/* idle connection list */
	NCConnectionList idleList;
	/* busy connection list SEND,RECV,LINGER */
	NCConnectionList busyList;
};

inline int NCConnection::is_dgram(void) const { return serverInfo->mode == 2; }
inline int NCConnection::is_async(void) const { return serverInfo->mode; }

#endif
