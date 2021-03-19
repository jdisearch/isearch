#include <stdlib.h>
#include <stdint.h>
#include <new>

#include "dtcapi.h"
#include "dtcint.h"
#include <string.h>
#include <protocol.h>
#include "compiler.h"


using namespace DTC;

#define CAST(type, var) type *var = (type *)addr
#define CAST2(type, var, src) type *var = (type *)src->addr
#define CAST2Z(type, var, src) type *var = (type *)((src)?(src)->addr:0)
#define CAST3(type, var, src) type *var = (type *)src.addr

//WRAPPER for log
__EXPORT
void DTC::init_log (const char *app, const char *dir)
{
	::_init_log_(app, dir);
}

__EXPORT
void DTC::set_log_level(int n)
{
	::_set_log_level_(n);
}

__EXPORT
int DTC::set_key_value_max(unsigned int count)
{
	if(count >= 8 && count <= 10000) {
		NCKeyValueList::KeyValueMax = count;
	} else  {
		return -EINVAL;
	}
	return 0;
};

// WRAPPER for Server
__EXPORT
Server::Server(void) {
	NCServer *srv = new NCServer;
	srv->INC();
	addr = srv;
}

__EXPORT
Server::Server(const Server &that) {
	CAST3(NCServer, old, that);
	NCServer *srv = new NCServer(*old);
	srv->INC();
	addr = srv;
}

__EXPORT
Server::~Server(void) {
	CAST(NCServer, srv);
	DEC_DELETE(srv);
}

__EXPORT
int Server::SetAddress(const char *host, const char *port) {
	CAST(NCServer, srv);
	return srv->SetAddress(host, port);
}

__EXPORT
int Server::SetTableName(const char *name) {
	CAST(NCServer, srv);
	return srv->SetTableName(name);
}

__EXPORT
void Server::SetCompressLevel(int level) {
	CAST(NCServer, srv);
	srv->SetCompressLevel(level);
}

__EXPORT
const char * Server::GetAddress(void) const {
	CAST(NCServer, srv);
	return srv ? srv->GetAddress() : NULL;
}

__EXPORT
const char * Server::GetTableName(void) const {
	CAST(NCServer, srv);
	return srv ? srv->GetTableName() : NULL;
}

__EXPORT
const char * Server::GetServerAddress(void) const {
	CAST(NCServer, srv);
	return srv ? srv->GetServerAddress() : NULL;
}

__EXPORT
const char * Server::GetServerTableName(void) const {
	CAST(NCServer, srv);
	return srv ? srv->GetServerTableName() : NULL;
}
__EXPORT
void Server::CloneTabDef(const Server& source)
{
	CAST(NCServer, srv);
	CAST3(NCServer, ssrv, source);
	srv->CloneTabDef(*ssrv);
}

__EXPORT
int Server::IntKey(void) {
	CAST(NCServer, srv);
	return srv->IntKey();
}

__EXPORT
int Server::StringKey(void) {
	CAST(NCServer, srv);
	return srv->StringKey();
}

__EXPORT
int Server::BinaryKey(void) {
	CAST(NCServer, srv);
	return srv->StringKey();
}

__EXPORT
int Server::AddKey(const char* name, int type) {
	CAST(NCServer, srv);
	return srv->AddKey(name, (uint8_t)type);
}

__EXPORT
int Server::FieldType(const char* name) {
	CAST(NCServer, srv);
	return srv->FieldType(name);
}

__EXPORT
const char * Server::ErrorMessage(void) const {
	CAST(NCServer, srv);
	return srv->ErrorMessage();
}

//fixed, sec ---> msec
__EXPORT
void Server::SetMTimeout(int n) {
	CAST(NCServer, srv);
	return srv->SetMTimeout(n);
}

__EXPORT
void Server::SetTimeout(int n) {
	CAST(NCServer, srv);
	return srv->SetMTimeout(n*1000);
}

__EXPORT
int Server::Connect(void) {
	CAST(NCServer, srv);
	return srv->Connect();
}

__EXPORT
void Server::Close(void) {
	CAST(NCServer, srv);
	return srv->Close();
}

__EXPORT
int Server::Ping(void) {
	CAST(NCServer, srv);
	return srv->Ping();
}

__EXPORT
void Server::AutoPing(void) {
	CAST(NCServer, srv);
	return srv->AutoPing();
}

__EXPORT
void Server::SetFD(int fd) {
	CAST(NCServer, srv);
	srv->SetFD(fd);
}

__EXPORT
void Server::SetAutoUpdateTab(bool autoUpdate)
{
	CAST(NCServer, srv);
	srv->SetAutoUpdateTab(autoUpdate);
}

__EXPORT
void Server::SetAutoReconnect(int autoReconnect)
{
	CAST(NCServer, srv);
	srv->SetAutoReconnect(autoReconnect);
}

__EXPORT
void Server::SetAccessKey(const char *token)
{
	CAST(NCServer, srv);
	srv->SetAccessKey(token);
}

//QOS ADD BY NEOLV
__EXPORT
void Server::IncErrCount()
{
	CAST(NCServer, srv);
	srv->IncErrCount();
}
__EXPORT
uint64_t Server::GetErrCount()
{
	CAST(NCServer, srv);
	return srv->GetErrCount();
}
__EXPORT
void Server::ClearErrCount()
{
	CAST(NCServer, srv);
	srv->ClearErrCount();
}

__EXPORT
void Server::IncRemoveCount()
{
	CAST(NCServer, srv);
	srv->IncRemoveCount();
}
__EXPORT
int Server::GetRemoveCount()
{
	CAST(NCServer, srv);
	return srv->GetRemoveCount();
}
__EXPORT
void Server::ClearRemoveCount()
{
	CAST(NCServer, srv);
	srv->ClearRemoveCount();
}
__EXPORT
void Server::IncTotalReq()
{
	CAST(NCServer, srv);
	srv->IncTotalReq();
}
__EXPORT
uint64_t Server::GetTotalReq()
{
	CAST(NCServer, srv);
	return srv->GetTotalReq();
}
__EXPORT
void Server::ClearTotalReq()
{
	CAST(NCServer, srv);
	srv->ClearTotalReq();
}

__EXPORT
void Server::AddTotalElaps(uint64_t iElaps)
{
	CAST(NCServer, srv);
	srv->AddTotalElaps(iElaps);
}
__EXPORT
uint64_t Server::GetTotalElaps()
{
	CAST(NCServer, srv);
	return srv->GetTotalElaps();
}
__EXPORT
void Server::ClearTotalElaps()
{
	CAST(NCServer, srv);
	srv->ClearTotalElaps();
}


// WRAPER for Request

__EXPORT
Request::Request(Server *srv0, int op) {
	CAST2Z(NCServer, srv, srv0);
	NCRequest *req = new NCRequest(srv, op);
	addr = (void *)req;
}

Request::Request()
{
	NCRequest *req = new NCRequest(NULL, 0);
	addr = (void *)req;
}

__EXPORT
Request::~Request(void) {
	CAST(NCRequest, req);
	delete req;
}

__EXPORT
int Request::AttachServer(Server *srv0) {
	CAST2Z(NCServer, srv, srv0);
	CAST(NCRequest, r);
	return r->AttachServer(srv);
}

__EXPORT
void Request::Reset(void) {
	CAST(NCRequest, old);
	if(old) {
		int op = old->cmd;
		NCServer *srv = old->server;
		srv->INC();
		old->~NCRequest();
		memset(old, 0, sizeof(NCRequest));
		new(addr) NCRequest(srv, op);
		srv->DEC();
	}
}

__EXPORT
void Request::Reset(int op)
{
	CAST(NCRequest, old);

	if (old)
	{
		NCServer *srv = old->server;
		srv->INC();
		old->~NCRequest();
		memset(old, 0, sizeof(NCRequest));
		new(addr) NCRequest(srv, op);
		srv->DEC();
	}
}

__EXPORT
void Request::SetAdminCode(int code) {
	CAST(NCRequest, r);
	r->SetAdminCode(code);
}

__EXPORT
void Request::SetHotBackupID(long long v) {
	CAST(NCRequest, r);
	r->SetHotBackupID(v);
}

__EXPORT
void Request::SetMasterHBTimestamp(long long t) {
	CAST(NCRequest, r);
	r->SetMasterHBTimestamp(t);
}

__EXPORT
void Request::SetSlaveHBTimestamp(long long t) {
	CAST(NCRequest, r);
	r->SetSlaveHBTimestamp(t);
}

__EXPORT
void Request::NoCache(void) {
	CAST(NCRequest, r); return r->EnableNoCache();
}

__EXPORT
void Request::NoNextServer(void) {
	CAST(NCRequest, r); return r->EnableNoNextServer();
}

__EXPORT
int Request::Need(const char *name) {
	CAST(NCRequest, r); return r->Need(name, 0);
}

__EXPORT
int Request::Need(const char *name, int vid) {
	CAST(NCRequest, r); return r->Need(name, vid);
}

__EXPORT
int Request::FieldType(const char *name) {
	CAST(NCRequest, r); return r->FieldType(name);
}

__EXPORT
void Request::Limit(unsigned int st, unsigned int cnt) {
	CAST(NCRequest, r); return r->Limit(st, cnt);
}

__EXPORT
void Request::UnsetKey(void) {
	CAST(NCRequest, r);
	r->UnsetKey();
	r->UnsetKeyValue();
}

__EXPORT
int Request::SetKey(long long key) {
	CAST(NCRequest, r); return r->SetKey((int64_t)key);
}

__EXPORT
int Request::SetKey(const char *key) {
	CAST(NCRequest, r); return r->SetKey(key, strlen(key));
}

__EXPORT
int Request::SetKey(const char *key, int len) {
	CAST(NCRequest, r); return r->SetKey(key, len);
}

__EXPORT
int Request::AddKeyValue(const char* name, long long v) {
	CAST(NCRequest, r);
	DTCValue key(v);
	return r->AddKeyValue(name, key, KeyTypeInt);
}

__EXPORT
int Request::AddKeyValue(const char* name, const char *v) {
	CAST(NCRequest, r);
	DTCValue key(v);
	return r->AddKeyValue(name, key, KeyTypeString);
}

__EXPORT
int Request::AddKeyValue(const char* name, const char *v, int len) {
	CAST(NCRequest, r);
	DTCValue key(v, len);
	return r->AddKeyValue(name, key, KeyTypeString);
}

__EXPORT
int Request::SetCacheID(long long id){
    CAST(NCRequest, r); return r->SetCacheID(id);
}

__EXPORT
Result *Request::Execute(void) {
	Result *s = new Result();
	CAST(NCRequest, r);
	s->addr = r->Execute();
	return s;
}

__EXPORT
Result *Request::Execute(long long k) {
	Result *s = new Result();
	CAST(NCRequest, r);
	s->addr = r->Execute((int64_t)k);
	return s;
}

__EXPORT
Result *Request::Execute(const char *k) {
	Result *s = new Result();
	CAST(NCRequest, r);
	s->addr = r->Execute(k, k?strlen(k):0);
	return s;
}

__EXPORT
Result *Request::Execute(const char *k, int l) {
	Result *s = new Result();
	CAST(NCRequest, r);
	s->addr = r->Execute(k, l);
	return s;
}

__EXPORT
int Request::Execute(Result &s) {
	CAST(NCRequest, r);
	s.Reset();
	s.addr = r->Execute();
	return s.ResultCode();
}

__EXPORT
int Request::Execute(Result &s, long long k) {
	CAST(NCRequest, r);
	s.Reset();
	s.addr = r->Execute((int64_t)k);
	return s.ResultCode();
}

__EXPORT
int Request::Execute(Result &s, const char *k) {
	CAST(NCRequest, r);
	s.Reset();
	s.addr = r->Execute(k, k?strlen(k):0);
	return s.ResultCode();
}

__EXPORT
int Request::Execute(Result &s, const char *k, int l) {
	CAST(NCRequest, r);
	s.Reset();
	s.addr = r->Execute(k, l);
	return s.ResultCode();
}
//增加错误信息，方便以后问题定位. add by foryzhou
__EXPORT
const char * Request::ErrorMessage(void) const {
	CAST(NCRequest, r);
	if(r==NULL) return NULL;
	return r->ErrorMessage();
}

#define _DECLACT_(f0,t0,f1,f2,t1) \
    __EXPORT \
    int Request::f0(const char *n, t0 v) { \
	CAST(NCRequest, r); \
	return r->f1(n, DField::f2, DField::t1, DTCValue::Make(v)); \
    }
#define _DECLACT2_(f0,t0,f1,f2,t1) \
    __EXPORT \
    int Request::f0(const char *n, t0 v, int l) { \
	CAST(NCRequest, r); \
	return r->f1(n, DField::f2, DField::t1, DTCValue::Make(v,l)); \
    }



_DECLACT_(EQ, long long, AddCondition, EQ, Signed);
_DECLACT_(NE, long long, AddCondition, NE, Signed);
_DECLACT_(GT, long long, AddCondition, GT, Signed);
_DECLACT_(GE, long long, AddCondition, GE, Signed);
_DECLACT_(LT, long long, AddCondition, LT, Signed);
_DECLACT_(LE, long long, AddCondition, LE, Signed);
_DECLACT_(EQ, const char *, AddCondition, EQ, String);
_DECLACT_(NE, const char *, AddCondition, NE, String);
_DECLACT2_(EQ, const char *, AddCondition, EQ, String);
_DECLACT2_(NE, const char *, AddCondition, NE, String);
_DECLACT_(Set, long long, AddOperation, Set, Signed);
_DECLACT_(OR, long long, AddOperation, OR, Signed);
_DECLACT_(Add, long long, AddOperation, Add, Signed);

__EXPORT
int Request::Sub(const char *n, long long v) {
    CAST(NCRequest, r);
    return r->AddOperation(n, DField::Add, DField::Signed, DTCValue::Make(-(int64_t)v));
}

_DECLACT_(Set, double, AddOperation, Set, Float);
_DECLACT_(Add, double, AddOperation, Add, Float);

__EXPORT
int Request::Sub(const char *n, double v) {
    CAST(NCRequest, r);
    return r->AddOperation(n, DField::Add, DField::Signed, DTCValue::Make(-v));
}

_DECLACT_(Set, const char *, AddOperation, Set, String);
_DECLACT2_(Set, const char *, AddOperation, Set, String);
#undef _DECLACT_
#undef _DECLACT2_

__EXPORT
int Request::CompressSet(const char *n,const char * v,int len)
{
    CAST(NCRequest, r);
    return r->CompressSet(n,v,len);
}

__EXPORT
int Request::CompressSetForce(const char *n,const char * v,int len)
{
    CAST(NCRequest, r);
    return r->CompressSetForce(n,v,len);
}

__EXPORT
int Request::SetMultiBits(const char *n, int o, int s, unsigned int v) {
    if(s <=0 || o >= (1 << 24) || o < 0)
	return 0;
    if(s >= 32)
	s = 32;

    CAST(NCRequest, r);
    // 1 byte 3 byte 4 byte
    //   s     o      v
    uint64_t t = ((uint64_t)s << 56)+ ((uint64_t)o << 32) + v;
    return r->AddOperation(n, DField::SetBits, DField::Unsigned, DTCValue::Make(t));
}

__EXPORT
int Request::SetExpireTime(const char* key, int time){
	CAST(NCRequest, r);
	return r->SetExpireTime(key, time);
}

__EXPORT
int Request::GetExpireTime(const char* key){
	CAST(NCRequest, r);
	return r->GetExpireTime(key);
}


// WRAPER for Result
__EXPORT
Result::Result(void) {
	addr = NULL;
}

__EXPORT
Result::~Result(void) {
	Reset();
}

__EXPORT
void Result::SetError(int err, const char *via, const char *msg) {
	Reset();
	addr = new NCResult(err, via, msg);
}

__EXPORT
void Result::Reset(void) {
	if(addr != NULL) {
		CAST(NCResult, task);
		CAST(NCResultInternal, itask);
		if(task->Role()==TaskRoleClient)
			delete task;
		else
			delete itask;
	}
	addr = NULL;
}

__EXPORT
int Result::FetchRow(void) {
	CAST(NCResult, r);
	if(r==NULL) return -EC_NO_MORE_DATA;
	if(r->ResultCode()<0) return r->ResultCode();
	if(r->result==NULL) return -EC_NO_MORE_DATA;
	return r->result->DecodeRow();
}

__EXPORT
int Result::Rewind(void) {
	CAST(NCResult, r);
	if(r==NULL) return -EC_NO_MORE_DATA;
	if(r->ResultCode()<0) return r->ResultCode();
	if(r->result==NULL)  return -EC_NO_MORE_DATA;
	return r->result->Rewind();
}

__EXPORT
int Result::ResultCode(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->ResultCode();
}

__EXPORT
const char * Result::ErrorMessage(void) const {
	CAST(NCResult, r);
	if(r==NULL) return NULL;
	int code = r->ResultCode();
	const char *msg = r->resultInfo.ErrorMessage();
	if(msg==NULL && code < 0) {
		char a[1];
		msg = strerror_r(-code, a, 1);
		if(msg==a) msg = NULL;
	}
	return msg;
}

__EXPORT
const char * Result::ErrorFrom(void) const {
	CAST(NCResult, r);
	if(r==NULL) return NULL;
	return r->resultInfo.ErrorFrom();
}

__EXPORT
long long Result::HotBackupID() const {
    CAST(NCResult, r);
    return r->versionInfo.HotBackupID();
}

__EXPORT
long long Result::MasterHBTimestamp() const {
    CAST(NCResult, r);
    return r->versionInfo.MasterHBTimestamp();
}

__EXPORT
long long Result::SlaveHBTimestamp() const {
    CAST(NCResult, r);
    return r->versionInfo.SlaveHBTimestamp();
}

__EXPORT
char* Result::ServerInfo() const {
    CAST(NCResult, r);
    return r->resultInfo.ServerInfo();
}

__EXPORT
long long Result::BinlogID() const {
	CServerInfo *p = (CServerInfo *)ServerInfo();
	return p ? p->binlog_id : 0;
}

__EXPORT
long long Result::BinlogOffset() const {
	CServerInfo *p = (CServerInfo *)ServerInfo();
	return p ? p->binlog_off : 0;
}
__EXPORT
long long Result::MemSize() const
{
	CServerInfo *p = (CServerInfo *)ServerInfo();
	return p ? p->memsize : 0;
}
__EXPORT
long long Result::DataSize() const
{
	CServerInfo *p = (CServerInfo *)ServerInfo();
	return p ? p->datasize : 0;
}

__EXPORT
int Result::NumRows(void) const {
	CAST(NCResult, r);
	if(r==NULL || r->result==NULL) return 0;
	return r->result->TotalRows();
}

__EXPORT
int Result::TotalRows(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->resultInfo.TotalRows();
}

__EXPORT
long long Result::InsertID(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->resultInfo.InsertID();
}

__EXPORT
int Result::NumFields(void) const {
	CAST(NCResult, r);
	if(r==NULL || r->result==NULL) return 0;
	return r->result->NumFields();
}

__EXPORT
const char* Result::FieldName(int n) const {
	CAST(NCResult, r);
	if(r==NULL || r->result==NULL) return NULL;
	if(n<0 || n>=r->result->NumFields()) return NULL;
	return r->FieldName(r->result->FieldId(n));
}

__EXPORT
int Result::FieldPresent(const char* name) const {
	CAST(NCResult, r);
	if(r==NULL || r->result==NULL) return 0;
	int id = r->FieldId(name);
	if(id < 0) return 0;
	return r->result->FieldPresent(id);
}

__EXPORT
int Result::FieldType(int n) const {
	CAST(NCResult, r);
	if(r==NULL || r->result==NULL) return FieldTypeNone;
	if(n<0 || n>=r->result->NumFields()) return FieldTypeNone;
	return r->FieldType(r->result->FieldId(n));
}

__EXPORT
int Result::AffectedRows(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->resultInfo.AffectedRows();
}

__EXPORT
long long Result::IntKey(void) const {
	CAST(NCResult, r);
	const DTCValue *v = NULL;
	if(r && r->FlagMultiKeyResult() && r->result){
		v = r->result->FieldValue(0);
	}
	else if(r && r->ResultKey()){
		v = r->ResultKey();
	}
	if(v != NULL){
		switch(r->FieldType(0)) {
		case DField::Signed:
		case DField::Unsigned:
		    return v->s64;
	    }
	}
/*	
	if(r && r->ResultKey()) {
	    switch(r->FieldType(0)) {
		case DField::Signed:
		case DField::Unsigned:
		    return r->ResultKey()->s64;
	    }
	}
*/
	return 0;
}

__EXPORT
const char *Result::StringKey(void) const {
	CAST(NCResult, r);
	const DTCValue *v = NULL;
	if(r && r->FlagMultiKeyResult() && r->result){
		v = r->result->FieldValue(0);
	}
	else if(r && r->ResultKey()){
		v = r->ResultKey();
	}
	if(v != NULL){
		  switch(r->FieldType(0)) {
			case DField::Binary:
			case DField::String:
				return v->bin.ptr;
		}
	}

	return NULL;
}

__EXPORT
const char *Result::StringKey(int *lp) const {
	CAST(NCResult, r);
	const DTCValue *v = NULL;
	if(r && r->FlagMultiKeyResult() && r->result){
		v = r->result->FieldValue(0);
	}
	else if(r && r->ResultKey()){
		v = r->ResultKey();
	}
	if(v != NULL){
		switch(r->FieldType(0)) {
		case DField::Binary:
		case DField::String:
			 if(lp) *lp = v->bin.len;
		    return v->bin.ptr;
	    }
	}	

	return NULL;
}

const char *Result::StringKey(int &l) const {
	CAST(NCResult, r);
	const DTCValue *v = NULL;
	if(r && r->FlagMultiKeyResult() && r->result){
		v = r->result->FieldValue(0);
	}
	else if(r && r->ResultKey()){
		v = r->ResultKey();
	}
	if(v != NULL){
		switch(r->FieldType(0)) {
		case DField::Binary:
		case DField::String:
			 l = v->bin.len;
		    return v->bin.ptr;
	    }
	}		

	return NULL;
}

__EXPORT
const char *Result::BinaryKey(void) const {
	return StringKey();
}

__EXPORT
const char *Result::BinaryKey(int *lp) const {
	return StringKey(lp);
}

__EXPORT
const char *Result::BinaryKey(int &l) const {
	return StringKey(l);
}

static inline int64_t GetIntValue(NCResult *r, int id) {
	if(id >= 0) {
		const DTCValue *v;
		if(id==0 && !(r->result->FieldPresent(0)))
			v = r->ResultKey();
		else
			v = r->result->FieldValue(id);

		if(v) {
			switch(r->FieldType(id)) {
				case DField::Signed:
				case DField::Unsigned:
					return v->s64;
				case DField::Float:
					return llround(v->flt);
					//return (int64_t)(v->flt);
				case DField::String:
					if(v->str.ptr)
						return atoll(v->str.ptr);
			}
		}
	}
	return 0;
}

static inline double GetFloatValue(NCResult *r, int id) {
	if(id >= 0) {
		const DTCValue *v;
		if(id==0 && !(r->result->FieldPresent(0)))
			v = r->ResultKey();
		else
			v = r->result->FieldValue(id);

		if(v) {
			switch(r->FieldType(id)) {
				case DField::Signed:
					return (double)v->s64;
				case DField::Unsigned:
					return (double)v->u64;
				case DField::Float:
					return v->flt;
				case DField::String:
					if(v->str.ptr)
						return atof(v->str.ptr);
			}
		}
	}
	return NAN;
}

static inline const char *GetStringValue(NCResult *r, int id, int *lenp) {
	if(id >= 0) {
		const DTCValue *v;
		if(id==0 && !(r->result->FieldPresent(0)))
			v = r->ResultKey();
		else
			v = r->result->FieldValue(id);

		if(v) {
			switch(r->FieldType(id)) {
				case DField::String:
					if(lenp) *lenp = v->bin.len;
					return v->str.ptr;
			}
		}
	}
	return NULL;
}

static inline const char *GetBinaryValue(NCResult *r, int id, int *lenp) {
	if(id >= 0) {
		const DTCValue *v;
		if(id==0 && !(r->result->FieldPresent(0)))
			v = r->ResultKey();
		else
			v = r->result->FieldValue(id);

		if(v) {
			switch(r->FieldType(id)) {
				case DField::String:
				case DField::Binary:
					if(lenp) *lenp = v->bin.len;
					return v->str.ptr;
			}
		}
	}
	return NULL;
}

__EXPORT
long long Result::IntValue(const char *name) const {
	CAST(NCResult, r);
	if(r && r->result)
	    return GetIntValue(r, r->FieldId(name));
	return 0;
}

__EXPORT
long long Result::IntValue(int id) const {
	CAST(NCResult, r);
	if(r && r->result)
	    return GetIntValue(r, r->FieldIdVirtual(id));
	return 0;
}

__EXPORT
double Result::FloatValue(const char *name) const {
	CAST(NCResult, r);
	if(r && r->result)
	    return GetFloatValue(r, r->FieldId(name));
	return NAN;
}

__EXPORT
double Result::FloatValue(int id) const {
	CAST(NCResult, r);
	if(r && r->result)
	    return GetFloatValue(r, r->FieldIdVirtual(id));
	return NAN;
}

__EXPORT
const char *Result::StringValue(const char *name, int *lenp) const {
	if(lenp) *lenp = 0;
	CAST(NCResult, r);
	if(r && r->result)
	    return GetStringValue(r, r->FieldId(name), lenp);
	return NULL;
}

__EXPORT
const char *Result::StringValue(int id, int *lenp) const {
	if(lenp) *lenp = 0;
	CAST(NCResult, r);
	if(r && r->result)
	    return GetStringValue(r, r->FieldIdVirtual(id), lenp);
	return NULL;
}

__EXPORT
const char *Result::StringValue(const char *name, int &len) const {
	return StringValue(name, &len);
}

__EXPORT
const char *Result::StringValue(int id, int &len) const {
	return StringValue(id, &len);
}

__EXPORT
const char *Result::StringValue(const char *name) const {
	return StringValue(name, NULL);
}

__EXPORT
const char *Result::StringValue(int id) const {
	return StringValue(id, NULL);
}

__EXPORT
const char *Result::BinaryValue(const char *name, int *lenp) const {
	if(lenp) *lenp = 0;
	CAST(NCResult, r);
	if(r && r->result)
	    return GetBinaryValue(r, r->FieldId(name), lenp);
	return NULL;
}

//所以UnCompressBinaryValue接口的调用均会复制一份buffer，用户得到数据后需要手动释放
__EXPORT
int Result::UnCompressBinaryValue(const char *name,char **buf,int *lenp){
    if(buf==NULL||lenp==NULL)
        return -EC_UNCOMPRESS_ERROR;

    int iret = 0;
	*lenp = 0;
    *buf = NULL;
    const char * buf_dup = NULL;
    uint64_t compressflag = 0;

	CAST(NCResult, r);
	if(r && r->result)
	{
		iret = r->initCompress();
		if (iret) return iret;
		if (name == NULL)
		{
			snprintf(r->gzip->_errmsg, sizeof(r->gzip->_errmsg), "field name is null");
			return -EC_UNCOMPRESS_ERROR;
		}
		int fieldId = r->FieldId(name);
		if (fieldId >63)//fieldid must less than 64,because compressflag is 8 bytes uint
		{
			snprintf(r->gzip->_errmsg, sizeof(r->gzip->_errmsg), "fieldid must less than 64,because compressflag is 8 bytes uint");
			return -EC_UNCOMPRESS_ERROR;
		}
		buf_dup = GetBinaryValue(r, fieldId, lenp);
		if (buf_dup==NULL)
		{
			snprintf(r->gzip->_errmsg, sizeof(r->gzip->_errmsg), "BinaryValue of this field is NULL,please check fieldname and fieldtype is binary or not");
			return -EC_UNCOMPRESS_ERROR;
		}

		if (r->CompressId() < 0)//没启用压缩
		{
			*buf = (char *)MALLOC(*lenp);
			if (*buf==NULL)
				return -ENOMEM;
			memcpy(*buf,buf_dup,*lenp);
			return 0;
		}
		compressflag = GetIntValue(r,r->CompressId());
		//启用了压缩，但是该字段没有被压缩
		if (!(compressflag&(0x1<<fieldId)))
		{
			*buf = (char *)MALLOC(*lenp);
			if (*buf==NULL)
				return -ENOMEM;
			memcpy(*buf,buf_dup,*lenp);
			return 0;
        }

        //需要解压
        iret = r->gzip->UnCompress(buf,lenp,buf_dup,*lenp);
        if (iret == -111111)
            return -EC_UNCOMPRESS_ERROR;
        return iret;
    }
    return  -EC_UNCOMPRESS_ERROR;
}

//所以UnCompressBinaryValue接口的调用均会复制一份buffer，用户得到数据后需要手动释放
//忽略compressflag
__EXPORT
int Result::UnCompressBinaryValueForce(const char *name,char **buf,int *lenp){
    if(buf==NULL||lenp==NULL)
        return -EC_UNCOMPRESS_ERROR;

    int iret = 0;
	*lenp = 0;
    *buf = NULL;
    const char * buf_dup = NULL;

	CAST(NCResult, r);
	if(r && r->result)
	{
		if (name == NULL)
		{
			snprintf(r->gzip->_errmsg, sizeof(r->gzip->_errmsg), "field name is null");
			return -EC_UNCOMPRESS_ERROR;
		}
		iret = r->initCompress();
		if (iret) return iret;
		int fieldId = r->FieldId(name);
		buf_dup = GetBinaryValue(r, fieldId, lenp);
		if (buf_dup==NULL)
		{
			snprintf(r->gzip->_errmsg, sizeof(r->gzip->_errmsg), "BinaryValue of this field is NULL,please check fieldname and fieldtype is binary or not");
			return -EC_UNCOMPRESS_ERROR;
		}
        //需要解压
        iret = r->gzip->UnCompress(buf,lenp,buf_dup,*lenp);
        if (iret == -111111)
            return -EC_UNCOMPRESS_ERROR;
        return iret;
    }
    return  -EC_UNCOMPRESS_ERROR;
}

__EXPORT
const char *Result::UnCompressErrorMessage() const
{
	CAST(NCResult, r);
	if(r && r->gzip)
        return  r->gzip->ErrorMessage();
    return NULL;
}

__EXPORT
const char *Result::BinaryValue(int id, int *lenp) const {
	if(lenp) *lenp = 0;
	CAST(NCResult, r);
	if(r && r->result)
	    return GetBinaryValue(r, r->FieldIdVirtual(id), lenp);
	return NULL;
}

__EXPORT
const char *Result::BinaryValue(const char *name, int &len) const {
	return BinaryValue(name, &len);
}

__EXPORT
const char *Result::BinaryValue(int id, int &len) const {
	return BinaryValue(id, &len);
}

__EXPORT
const char *Result::BinaryValue(const char *name) const {
	return BinaryValue(name, NULL);
}

__EXPORT
const char *Result::BinaryValue(int id) const {
	return BinaryValue(id, NULL);
}

__EXPORT
long long Result::Tag(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->GetApiTag();
};

__EXPORT
void *Result::TagPtr(void) const {
	CAST(NCResult, r);
	if(r==NULL) return NULL;
	return (void *)(long)r->GetApiTag();
};

__EXPORT
long long Result::Magic(void) const {
	CAST(NCResult, r);
	if(r==NULL) return 0;
	return r->versionInfo.SerialNr();
};

__EXPORT
long long Result::ServerTimestamp(void) const
{
	CAST(NCResult, r);

	if (r == NULL)
	{
		return 0;
	}

	return r->resultInfo.Timestamp();
};

__EXPORT
int Server::DecodePacket(Result &s, const char *buf, int len)
{
	CAST(NCServer, srv);
	s.Reset();
	s.addr = srv->DecodeBuffer(buf, len);
	return s.ResultCode();
}

__EXPORT
int Server::CheckPacketSize(const char *buf, int len)
{
	return NCServer::CheckPacketSize(buf, len);
}

__EXPORT
int Request::EncodePacket(char *&ptr, int &len, long long &m) {
	CAST(NCRequest, r);
	int64_t mm;
	int ret = r->EncodeBuffer(ptr, len, mm);
	m = mm;
	return ret;
}

__EXPORT
int Request::EncodePacket(char *&ptr, int &len, long long &m, long long k) {
	CAST(NCRequest, r);
	int64_t mm;
	int ret = r->EncodeBuffer(ptr, len, mm, (int64_t)k);
	m = mm;
	return ret;
}

__EXPORT
int Request::EncodePacket(char *&ptr, int &len, long long &m, const char *k) {
	CAST(NCRequest, r);
	int64_t mm;
	int ret = r->EncodeBuffer(ptr, len, mm, k, k?strlen(k):0);
	m = mm;
	return ret;
}

__EXPORT
int Request::EncodePacket(char *&ptr, int &len, long long &m, const char *k, int l) {
	CAST(NCRequest, r);
	int64_t mm;
	int ret = r->EncodeBuffer(ptr, len, mm, k, l);
	m = mm;
	return ret;
}

