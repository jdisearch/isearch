#include <inttypes.h>
#include <value.h>
#include <compiler.h>

#include "dtcapi.h"
#include "dtcpool.h"

using namespace DTC;

#define CAST(type, var) type *var = (type *)addr
#define CAST2(type, var, src) type *var = (type *)src->addr
#define CAST3(type, var, src) type *var = (type *)src.addr

// WRAPPER for Server
__EXPORT
ServerPool::ServerPool(int ms, int mr) {
	NCPool *pl = new NCPool(ms, mr);
	addr = pl;
}

__EXPORT
ServerPool::~ServerPool(void) {
	CAST(NCPool, pl);
	delete pl;
}

__EXPORT
int ServerPool::AddServer(Server *srv, int mreq, int mconn)
{
	CAST(NCPool, pl);
	CAST2(NCServer, s, srv);

	return pl->AddServer(s, mreq, mconn);
}

__EXPORT
int ServerPool::AddRequest(Request *req, long long tag)
{
	CAST(NCPool, pl);
	CAST2(NCRequest, r, req);

	if(r->server == NULL)
	    return -EC_NOT_INITIALIZED;

	return pl->AddRequest(r, tag, NULL);
}

__EXPORT
int ServerPool::AddRequest(Request *req, void *tag)
{
	return AddRequest(req, (long long)(long)tag);
}

__EXPORT
int ServerPool::AddRequest(Request *req, long long tag, long long k)
{
	CAST(NCPool, pl);
	CAST2(NCRequest, r, req);

	if(r->server == NULL)
	    return -EC_NOT_INITIALIZED;
	if(r->server->keytype != DField::Signed)
	    return -EC_BAD_KEY_TYPE;

	DTCValue v(k);
	return pl->AddRequest(r, tag, &v);
}

__EXPORT
int ServerPool::AddRequest(Request *req, void *tag, long long k)
{
	return AddRequest(req, (long long)(long)tag, k);
}

__EXPORT
int ServerPool::AddRequest(Request *req, long long tag, const char *k)
{
	CAST(NCPool, pl);
	CAST2(NCRequest, r, req);

	if(r->server == NULL)
	    return -EC_NOT_INITIALIZED;
	if(r->server->keytype != DField::String)
	    return -EC_BAD_KEY_TYPE;

	DTCValue v(k);
	return pl->AddRequest(r, tag, &v);
}

__EXPORT
int ServerPool::AddRequest(Request *req, void *tag, const char *k)
{
	return AddRequest(req, (long long)(long)tag, k);
}

__EXPORT
int ServerPool::AddRequest(Request *req, long long tag, const char *k, int l)
{
	CAST(NCPool, pl);
	CAST2(NCRequest, r, req);

	if(r->server == NULL)
	    return -EC_NOT_INITIALIZED;
	if(r->server->keytype != DField::String)
	    return -EC_BAD_KEY_TYPE;

	DTCValue v(k, l);
	return pl->AddRequest(r, tag, &v);
}

__EXPORT
int ServerPool::AddRequest(Request *req, void *tag, const char *k, int l)
{
	return AddRequest(req, (long long)(long)tag, k, l);
}

__EXPORT
int ServerPool::Execute(int msec)
{
	CAST(NCPool, pl);
	return pl->Execute(msec);
}

__EXPORT
int ServerPool::ExecuteAll(int msec)
{
	CAST(NCPool, pl);
	return pl->ExecuteAll(msec);
}

__EXPORT
int ServerPool::CancelRequest(int id)
{
	CAST(NCPool, pl);
	return pl->CancelRequest(id);
}

__EXPORT
int ServerPool::CancelAllRequest(int type)
{
	CAST(NCPool, pl);
	return pl->CancelAllRequest(type);
}

__EXPORT
int ServerPool::AbortRequest(int id)
{
	CAST(NCPool, pl);
	return pl->AbortRequest(id);
}

__EXPORT
int ServerPool::AbortAllRequest(int type)
{
	CAST(NCPool, pl);
	return pl->AbortAllRequest(type);
}

__EXPORT
Result * ServerPool::GetResult(void)
{
	return GetResult(0);
}

__EXPORT
Result * ServerPool::GetResult(int id)
{
	CAST(NCPool, pl);
	NCResult *a = pl->GetResult(id);
	if( (long)a  < 4096 && (long)a > -4095)
		return NULL;
	Result *s = new Result();
	s->addr = (void *)a;
	return s;
}

__EXPORT
int ServerPool::GetResult(Result &s)
{
	return GetResult(s, 0);
}

__EXPORT
int ServerPool::GetResult(Result &s, int id)
{
	CAST(NCPool, pl);
	s.Reset();
	NCResult *a = pl->GetResult(id);
	long iv = (long)a;
	if( iv  < 4096 && iv > -4095)
	{
		s.addr = (void *)(new NCResult(iv, "API::executing", iv>0 ? "Request not completed" : id ? "Invalid request" : "No more result"));
		return iv;
	}
	s.addr = (void *)a;
	return 0;
}

__EXPORT
int ServerPool::ServerCount(void) const
{
	CAST(NCPool, pl);
	return pl->ServerCount();
}

__EXPORT
int ServerPool::RequestCount(int type) const
{
	CAST(NCPool, pl);
	if(type == DONE)
		return pl->DoneRequestCount();
	if(type == (WAIT|SEND|RECV|DONE))
		return pl->RequestCount();
	return pl->CountRequestState(type);
}

__EXPORT
int ServerPool::RequestState(int reqId) const
{
	CAST(NCPool, pl);
	return pl->RequestState(reqId);
}

__EXPORT
int ServerPool::GetEpollFD(int size)
{
    CAST(NCPool, pl);
    return pl->GetEpollFD(size);
}

const int ALL_STATE = WAIT|SEND|RECV|DONE;
