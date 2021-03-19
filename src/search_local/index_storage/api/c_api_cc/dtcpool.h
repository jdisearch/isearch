#ifndef __CHC_CLI_POOL_H
#define __CHC_CLI_POOL_H

#include <sys/poll.h>

#include "list.h"
#include "poller.h"
#include "timerlist.h"
#include "dtcint.h"
#include "cache_error.h"

class NCServerInfo;
class NCPool;
class NCConnection;

// transation is a internal async request
class NCTransation :
	public CListObject<NCTransation>
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
	void Done(NCResult *res) { result = res; Abort(0); }
	/* get transation result */
	NCResult * GetResult(void);
	/* adjust generation id */
	void RoundGenId(int m) { if(genId >= m) genId = 0; }

	/* state & info management */
	int State(void) const { return state; }
	int GenId(void) const { return genId; }
	int MatchSN(NCResult *res) const { return SN == res->versionInfo.SerialNr(); }
	/* send packet management */
	int Send(int fd) { return packet->Send(fd); }
	int AttachRequest(NCServerInfo *s, long long tag, NCRequest *req, DTCValue *key);
	void AttachConnection(NCConnection *c);
	void SendOK(CListObject<NCTransation>*);
	void RecvOK(CListObject<NCTransation>*);

public: // constant member declare as static
	// owner info, associated server
	NCServerInfo *server;
	// attached connection, SEND, RECV
	NCConnection *conn;

private:// transient members is private
	// current transation state, WAIT, SEND, RECV, DONE
	int state;
	// internal transation generation id
	int genId;
	// associated request tag
	long long reqTag;
	// associated request SN, state SEND, RECV
	uint64_t SN;

	// sending packet
	CPacket *packet;

	// execute result
	NCResult *result;
};

class NCConnection :
	public CListObject<NCConnection>,
	public CPollerObject
{
public:
	NCConnection(NCPool *, NCServerInfo *);
	~NCConnection(void);

	int IsDgram(void) const;
	int IsAsync(void) const;
	/* starting state machine, by NCServerInfo::Connect */
	int Connect(void);
	/* attach transation to connection */
	void ProcessRequest(NCTransation *);
	/* flush send channel */
	void SendRequest(void);
	/* flush recv channel */
	int RecvResult(void);
	/* check connection hangup, recv zero bytes */
	int CheckHangup(void);

	/* abort all associated request */
	void AbortRequests(int err);
	/* close connection */
	void Close(int err);
	/* abort current sending request, linger recv channel */
	void AbortSendSide(int err);
	/* a valid result received */
	void DoneResult(void);
	/* connection is idle, usable for transation */
	void SwitchToIdle(void);
	/* connection is async connecting */
	void SwitchToConnecting(void);

	virtual void InputNotify(void);
	virtual void OutputNotify(void);
	virtual void HangupNotify(void);
	virtual void TimerNotify(void);

private:
	/* associated server */
	NCServerInfo *serverInfo;
	/* queued transation in RECV state */
	CListObject<NCTransation> reqList;
	/* sending transation */
	NCTransation *sreq;
	/* decoding/decoded result */
	NCResult *result;
	/* connection state */
	int state;

	int NETFD(void) const { return netfd; }

private:
	CTimerMember<NCConnection> timer;
	CSimpleReceiver receiver;
};

typedef CListObject<NCConnection> NCConnectionList;

class NCPool :
	public CPollerUnit,
	public CTimerUnit
{
public:
	NCPool(int maxServers, int maxRequests);
	~NCPool();

	int InitializePollerUnit(void);

	int GetEpollFD(int maxpoller);
	int AddServer(NCServer *srv, int maxReq=1, int maxConn=0);
	int AddRequest(NCRequest *req, long long tag, DTCValue *key=0);

	void ExecuteOneLoop(int timeout);
	int Execute(int timeout);
	int ExecuteAll(int timeout);
	
	NCTransation *Id2Req(int) const;
	int CancelRequest(int);
	int CancelAllRequest(int);
	int AbortRequest(NCTransation *);
	int AbortRequest(int);
	int AbortAllRequest(int);
	NCResult *GetResult(void);
	NCResult *GetResult(int);

	int CountRequestState(int) const;
	int RequestState(int) const;
public:
	NCTransation * GetTransationSlot(void);
	void TransationFinished(NCTransation *);
	NCResult * GetTransationResult(NCTransation *);
	int GetTransationState(NCTransation *);

	int ServerCount(void) const { return numServers; }
	int RequestCount(void) const { return numRequests; }
	int DoneRequestCount(void) const { return doneRequests; }
private:
	int initFlag;
	int maxServers;
	int maxRequests;
	int numServers;
	int numRequests;
	int maxRequestId;
	int doneRequests;
	CListObject<NCTransation> freeList;
	CListObject<NCTransation> doneList;
	NCServerInfo *serverList;
	NCTransation *transList;
public:
	char *buf;
};

class NCServerInfo :
	private CTimerObject
{
public:
	NCServerInfo(void);
	~NCServerInfo(void);
	void Init(NCServer *, int, int);

	/* prepare wake TimerNotify */
	void MarkAsReady() { AttachReadyTimer(owner); }
	virtual void TimerNotify(void);
	/* four reason server has more work to do */
	/* more transation attached */
	void MoreRequestAndReady(void) { reqWait++; MarkAsReady(); }
	/* more close connection, should reconnecting */
	void MoreClosedConnectionAndReady(void) { connRemain++; MarkAsReady(); }
	/* more idle connection available */
	void ConnectionIdleAndReady(void) { MarkAsReady(); }
	/* more request can assign to idle pool */
	void RequestDoneAndReady(int oldstate);

	/* one request scheduled to SEND state */
	void RequestScheduled(void) { reqRemain--; reqSend++; }
	void RequestSent(void) { reqSend--; reqRecv++; }

	/* queue transation to this server */
	void QueueRequest(NCTransation *req)
	{
		req->ListAddTail(&reqList);
		MoreRequestAndReady();
	}
	/* one connecting aborted */
	void ConnectingFailed(void) { connError++; }
	void ConnectingDone(void) { connConnecting--; }

	/* abort all waiting transations */
	void AbortWaitQueue(int err);

	int CountRequestState(int type) const;

	/* get a waiting transation */
	NCTransation * GetRequestFromQueue(void)
	{
		NCTransation *trans = reqList.NextOwner();
		trans->ListDel();
		reqWait--;
		return trans;
	}

private:
	int Connect(void);
	CListObject<NCTransation> reqList;

public: // constant member declare as public
	/* associated ServerPool */
	NCPool *owner;
	/* basic server info */
	NCServer *info;
	CTimerList *timerList;
	int mode; // 0--TCP 1--ASYNC 2--UDP
	/* total connection */
	int connTotal;

private:// transient member is private

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
	inline int connWorking(void) const { return connTotal - connConnecting - connRemain; }
public: //except idleList for code conventional
	/* idle connection list */
	NCConnectionList idleList;
	/* busy connection list SEND,RECV,LINGER */
	NCConnectionList busyList;
};

inline int NCConnection::IsDgram(void) const { return serverInfo->mode==2; }
inline int NCConnection::IsAsync(void) const { return serverInfo->mode; }

#endif
