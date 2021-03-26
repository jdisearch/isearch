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
#include <stdlib.h>
#include <linux/sockios.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "unix_socket.h"
#include "dtcint.h"
#include <timestamp.h>
#include <container.h>

using namespace std;

const uint32_t ACCESS_KEY_LEN = 40;
#define CLIENT_CURVE 5
#define AGENT_CURVE 8

const char* dtc_api_ver="dtc-api-ver:4.3.4"; 
const char* dtc_api_complite="dtc-api-complite-date:"__DATE__" "__TIME__;

int NCServer::_network_mode = 0;
char * NCServer::_server_address = NULL;
char * NCServer::_server_tablename = NULL;
extern "C" void set_network_mode(void)
{
    NCServer::_network_mode = 1;
}

extern "C" void set_server_address(const char * address)
{
    NCServer::_server_address = strdup(address);
}

extern "C" void set_server_tablename(const char * tablename)
{
    NCServer::_server_tablename = strdup(tablename);
}

NCServer::NCServer(void)
{
	keytype = DField::None;

	tablename = NULL;
	appname = NULL;

	autoUpdateTable = 1;
	autoReconnect = 1;
	tdef = NULL;
	admin_tdef = NULL;
	
	unsigned int a = time(NULL);
	a = rand_r(&a) + (long)this;
	lastSN = rand_r(&a);

	completed = 0;
	badkey = 0;
	badname = 0;
	autoping = 0;
	errstr = NULL;

	timeout = 5000;
	realtmo = 0;

	iservice = NULL;
	executor = NULL;
	netfd = -1;
	lastAct = 0;
	pingReq = NULL;

	ownerPool = NULL;
	ownerId = 0;
	dc = DataConnector::getInstance();

	m_iErrCount = 0;
	m_iTotalCount = 0;
	m_iTotalElaps = 0;
	m_RemoveCount = 0;
}


// copy most members, zero connection state
// copy address if use generic address
NCServer::NCServer(const NCServer &that) :
	addr(that.addr),
	keyinfo(that.keyinfo)
{
#define _MCOPY(x) this->x = that.x
#define _MZERO(x) this->x = 0
#define _MNEG(x) this->x = -1
#define _MDUP(x)	this->x = that.x ? STRDUP(that.x) : NULL

	_MDUP(tablename);
	_MDUP(appname);
	_MCOPY(keytype);
	_MCOPY(autoUpdateTable);
	_MCOPY(autoReconnect);

	_MCOPY(completed);
	_MCOPY(badkey);
	_MCOPY(badname);
	_MCOPY(autoping);
	// errstr always compile time const string
	_MCOPY(errstr);

	_MCOPY(tdef);
	_MCOPY(admin_tdef);
	if(tdef) tdef->INC();
	if(admin_tdef) admin_tdef->INC();

	lastAct = time(NULL);
	_MCOPY(timeout);
	_MZERO(realtmo); // this is sockopt set to real netfd
	_MCOPY(iservice);
	_MCOPY(executor);
	_MNEG(netfd);	// real socket fd, cleared
	_MZERO(pingReq); // cached ping request object

	unsigned int a = lastAct;
	a = rand_r(&a) + (long)this;
	lastSN = rand_r(&a);


	_MZERO(ownerPool); // serverpool linkage
	_MZERO(ownerId); // serverpool linkage

#undef _MCOPY
#undef _MZERO
#undef _MNEG
#undef _MDUP
}

NCServer::~NCServer(void) {
	DELETE(pingReq);
	Close();
	FREE_IF(tablename);
	FREE_IF(appname);
	DEC_DELETE(tdef);
	DEC_DELETE(admin_tdef);
	DataConnector::getInstance()->SendData();		// 防止10s 间隔时间未到，程序先结束了少报一次
	DataConnector::getInstance()->SendTopPercentileData();
}

DataConnector*  NCServer::dc = NULL;//DataConnector::getInstance();

void NCServer::CloneTabDef(const NCServer& source)
{
	DEC_DELETE(tdef);
	tdef = source.tdef;
	tdef->INC();
	if(tablename != NULL)
		FREE(tablename);
	tablename = STRDUP(source.tablename);
	keytype = source.keytype;
	completed = 1;
}

int NCServer::SetAddress(const char *h, const char *p) {
	if(h==NULL) {
		errstr = "No Host Specified";
		return -EC_BAD_HOST_STRING;
	}

	char *oldhost = NULL;
	if(addr.Name() != NULL) {
		// dup string put on stack
		oldhost = strdupa(addr.Name());
	}

	const char *err = addr.set_address(h, p);
	if(err) {
		this->errstr = err;
		return -EC_BAD_HOST_STRING;
	}

	// un-changed
	if(oldhost!=NULL && !strcmp(addr.Name(), oldhost)) 
		return 0;

	Close();

    //set network model
	executor = NULL;
    if(!_network_mode&&iservice && iservice->match_listening_ports(addr.Name(), NULL)) {
        executor = iservice->query_task_executor();
        DTCTableDefinition *t1 = iservice->query_table_definition();
        if(t1 != this->tdef) {
            DEC_DELETE(this->tdef);
            this->tdef = t1;
        }
        DTCTableDefinition *t2 = iservice->query_admin_table_definition();
        if(t2 != this->tdef) {
            DEC_DELETE(this->admin_tdef);
            this->admin_tdef = t2;
        }
    }

	return 0;
}

int NCServer::IntKey(void) {
	if(keytype != DField::None && keytype != DField::Signed)
	    return -EC_BAD_KEY_TYPE;
	keytype = DField::Signed;
	if(tablename != NULL) completed = 1;
	return 0;
}

int NCServer::StringKey(void) {
	if(keytype != DField::None && keytype != DField::String)
	    return -EC_BAD_KEY_TYPE;
	keytype = DField::String;
	if(tablename != NULL) completed = 1;
	return 0;
}

int NCServer::AddKey(const char* name, uint8_t type)
{
	int ret = keyinfo.AddKey(name, type);
	// TODO verify tabledef
	if(ret==0) {
		if(tablename != NULL) completed = 1;
		if(keytype == DField::None)
			keytype = type;
	}
	return 0;
}

int NCServer::SetTableName(const char *name) {
	if(name==NULL) return -EC_BAD_TABLE_NAME;

	if(tablename)
		return mystrcmp(name, tablename, 256)==0 ? 0 : -EC_BAD_TABLE_NAME;

	tablename = STRDUP(name);

    if(&NCServer::CheckInternalService != 0 && !_network_mode) {
        CheckInternalService();
    }

	if(keytype != DField::None) completed = 1;

	return 0;
}

int NCServer::FieldType(const char *name) {
	if(tdef == NULL)
		Ping();

	if(tdef != NULL){
		int id = tdef->field_id(name);
		if(id >= 0)
			return tdef->field_type(id);
	}
	
	return DField::None;
}

//fixed, sec --> msec
void NCServer::SetMTimeout(int n) {
	timeout = n<=0 ? 5000 : n;
	UpdateTimeout();
}

void NCServer::UpdateTimeout(void) {
	if(netfd >= 0 && realtmo != timeout) {
		struct timeval tv = { timeout/1000, (timeout%1000)*1000 };
		setsockopt(netfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		if(timeout) {
			if(tv.tv_usec > 1000000)
			{
				tv.tv_usec += 100000; // add more 100ms
			}else {
				tv.tv_usec += 100; // add more 100us
			}
			if(tv.tv_usec >= 1000000)
			{
				tv.tv_usec -= 1000000;
				tv.tv_sec  +=1;
			}
		}
		setsockopt(netfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		realtmo = timeout;
	}
}

void NCServer::UpdateTimeoutAnyway(void){
	if(netfd >= 0) {
		struct timeval tv = { timeout/1000, (timeout%1000)*1000 };
		setsockopt(netfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		if(timeout) {
			if(tv.tv_usec > 1000000)
			{
				tv.tv_usec += 100000; // add more 100ms
			}else {
				tv.tv_usec += 100; // add more 100us
			}
			if(tv.tv_usec >= 1000000)
			{
				tv.tv_usec -= 1000000;
				tv.tv_sec  +=1;
			}
		}
		setsockopt(netfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		realtmo = timeout;
	}
}

void NCServer::Close(void){
	if(netfd>=0) {
		close(netfd);
		netfd = -1;
		realtmo=0;
	}
}

int NCServer::BindTempUnixSocket(void) {
	struct sockaddr_un tempaddr;
	tempaddr.sun_family = AF_UNIX;
	snprintf(tempaddr.sun_path, sizeof(tempaddr.sun_path),
			"@dtcapi-%d-%d-%d", getpid(), netfd, (int)time(NULL));
	socklen_t len = SUN_LEN(&tempaddr);
	tempaddr.sun_path[0] = 0;
	return bind(netfd, (const sockaddr *)&tempaddr, len);
}

int NCServer::Connect(void) {
	if(ownerPool != NULL)
		return -EC_PARALLEL_MODE;
	if(executor != NULL)
		return 0;
	if(netfd >= 0)
		return 0;

	int err = -EC_NOT_INITIALIZED;

	if(addr.socket_family() != 0) {
		netfd = addr.create_socket();
		if(netfd < 0) {
			log_error("create socket error: %d, %m", errno);
			err = -errno;
		} else if(addr.socket_family()==AF_UNIX && IsDgram() && BindTempUnixSocket() < 0) {
			log_error("bind unix socket error: %d, %m", errno);
			err = -errno;
			Close();
		} else {
			int iRes = -1;
			//先将netfd设置为非阻塞模式，然后进行connect，返回0，则建立连接成功；返回其它失败
			fcntl(netfd, F_SETFL, fcntl(netfd, F_GETFL, 0) | O_NONBLOCK);
			if(addr.connect_socket(netfd)==0) {
				iRes = 0;
			}
			else{
				int iTimeout = timeout<=0 ? 5000 : timeout;
				//struct pollfd *pollfds = (struct pollfd *) calloc(1, sizeof(struct pollfd));
				struct pollfd pollfds[1];
						
				pollfds[0].fd = netfd;
				pollfds[0].events = POLLIN | POLLOUT | POLLERR;
				int ready_num = poll(pollfds, 1, iTimeout);
				if (ready_num < 0) {
					log_error("network error in connect, errno [%d], errmsg [%s]", errno, strerror(errno));
				}
				else if (ready_num == 0) {
					log_error("connect time out, errno [%d], errmsg [%s]", errno, strerror(errno));
				}
				else {
					if (pollfds[0].revents & POLLERR) {
						log_error("network error in connect, errno [%d], errmsg [%s]", errno, strerror(errno));
					}
					else if (pollfds[0].revents & POLLOUT || pollfds[0].revents & POLLIN) {
						iRes = 0;
					}

				}
			}
			//将netfd恢复为阻塞模式
			fcntl(netfd, F_SETFL, fcntl(netfd, F_GETFL, 0) & ~O_NONBLOCK);
			int keepalive = 1; //开启keepalive属性
			int keepidle = 60; //如该连接在60秒内没有任何数据往来,则进行探测
			int keepinterval = 5; //探测时发包的时间间隔为5 秒
			int keepcount = 3; //探测尝试的次数。如果第1次探测包就收到响应了,则后2次的不再发。
			setsockopt(netfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
			setsockopt(netfd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle ));
			setsockopt(netfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
			setsockopt(netfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));
			UpdateTimeoutAnyway();
			
			if(0 == iRes)
				return 0;
			log_error("connect dtc server error: %d,%m", errno);
			err = -errno;
			if(err==-EINPROGRESS) err = -ETIMEDOUT;
			Close();
		}
	}
	return err;
}

int NCServer::Reconnect(void) {
	if(ownerPool != NULL)
		return -EC_PARALLEL_MODE;
	if(executor != NULL)
		return 0;
	if(netfd < 0)
		return -ENOTCONN;

	if(addr.connect_socket(netfd)==0) {
		return 0;
	}
	log_error("connect dtc server error: %d,%m", errno);
	int err = -errno;
	if(err==-EINPROGRESS) err = -ETIMEDOUT;
	Close();
	return err;
}

int NCServer::SendPacketStream(Packet &pk) {
	int err;
#ifndef SIOCOUTQ
	if(1) //if(!IsDgram())
	{
		char tmp[1];
		err = recv(netfd, tmp, sizeof(tmp), MSG_PEEK|MSG_DONTWAIT);
		if(err==0 || (err<0 && errno != EAGAIN && errno != EWOULDBLOCK))
		{
			if(!autoReconnect)
			{
				log_error("dtc server close connection: %d,%m", err);
				return err;
			}
			else
			{
				log_debug("%s", "dtc server close connection, re-connect now.");
				Close();
				err = Connect();
				if(err) return err;
			}
		}
	}
#endif

	while((err = pk.Send(netfd)) == SendResultMoreData)
	{
	    if(errno==EINPROGRESS)
	    {
			log_error("socket[%d] send data timeout: %d,%m", netfd, errno);
	    	// timeout
			if(1) // if(!IsDgram())
				Close();
			return -ETIMEDOUT;
	    }
	}
	if(err != SendResultDone)
	{
		log_error("socket[%d] send data error: %d,%m", netfd, err);
	    err = -errno;
	    if(1) // if(!IsDgram())
		    Close();
	    return err;
	}
	return 0;
}

// always return network error code
// EAGAIN must re-sent
int NCServer::DecodeResultStream(NCResult &tk) 
{
	if(netfd < 0) 
	{
		tk.set_error(-ENOTCONN, "API::recving", "connection is closed");
		return -ENOTCONN;
	}

	int err;
	SimpleReceiver receiver(netfd);

	uint64_t beginTime = 0;
	//log_debug("wait response time stamp = %lld", (long long)(beginTime = GET_TIMESTAMP()));
	
	while(1)
	{
		errno = 0;
		err = tk.Decode(receiver);

		if(err == DecodeFatalError) 
		{
			log_error("socket[%d] decode data error: %d, %m", netfd, errno);
			
			err = -errno;
			if(err==0)
				err = -ECONNRESET;
#ifdef SIOCOUTQ
			int value = 0;
			ioctl(netfd, SIOCOUTQ, &value);
			// unsent bytes
			if(value > 0) {
				err = -EAGAIN;
				tk.set_error(err, "API::sending", "client send packet error");
				Close();
				return err;
			}
#endif
			Close();
			// EAGAIN never return FatalError, should be DecodeWaitData
			tk.set_error(err, "API::recving", "client recv packet error");
			return err;
		}

		if(err == DecodeDataError)
		{
			log_error("socket[%d] decode data error: %d, %m", netfd, errno);
			
			// packet is valid
			return 0;
		}

		if(err == DecodeDone)
		{
			break;
		}

		if(errno == EINPROGRESS || errno == EAGAIN)
		{
			log_error("socket[%d] decode data error: %d, %m", netfd, errno);
			log_debug("use time %ldms", (long)((GET_TIMESTAMP()-beginTime)/1000));
			
			Close();
			tk.set_error(-ETIMEDOUT, "API::recving", "client recv packet timedout");
			return -ETIMEDOUT;
		}
	}

	//log_debug("decode result use time: %ldms", (long)((GET_TIMESTAMP()-beginTime)/1000));
	
	SaveDefinition(&tk);
	if(autoping) {
		time(&lastAct);
		err = tk.versionInfo.keep_alive_timeout();
		if(err<15) err = 15;
		lastAct += err - 1;
	}
	return 0;
}

int NCServer::SendPacketDgram(SocketAddress *peer, Packet &pk)
{
	int err = 0;

	while ((err = pk.send_to(netfd, peer)) == SendResultMoreData)
	{
		if (errno == EINPROGRESS)
		{
			log_error("socket[%d] send data timeout: %d,%m", netfd, errno);
			// timeout
			return -ETIMEDOUT;
		}
	}

	if (err != SendResultDone)
	{
		log_error("socket[%d] send data error: %d,%m", netfd, err);
		err = -errno;
		return err;
	}
	return 0;
}

static inline char *RecvFromFixedSize(int fd, int len, int &err, struct sockaddr *peeraddr, socklen_t *addrlen)
{
	int blen = len <= 0 ? 1 : len;
	char *buf = (char *)MALLOC(blen);
	blen = recvfrom(fd, buf, blen, 0, peeraddr, addrlen);
	if(blen < 0) {
		err = errno;
		free(buf);
		buf = NULL;
	} else if(blen != len) {
		err = EIO;
		free(buf);
		buf = NULL;
	} else {
		err = 0;
	}

	return buf;
}

static char *RecvPacketPeek(int fd, int &len, int &err)
{
	// MSG_PEEK + MSG_TRUNC get next packet size
	char dummy[1];
	len = recv(fd, dummy, 1, MSG_PEEK|MSG_TRUNC);
	if(len < 0) {
		err = errno;
		return NULL;
	}

	return RecvFromFixedSize(fd, len, err, NULL, NULL);
}

static char *RecvPacketPeekPeer(int fd, SocketAddress *peer, int &len, int &err)
{
	struct sockaddr peeraddr;
	socklen_t sock_len;

	for (int n=0; n<10; n++) {
		char dummy[1];

		sock_len = sizeof(peeraddr);
		len = recvfrom(fd, dummy, 1, MSG_PEEK|MSG_TRUNC, &peeraddr, &sock_len);
		if(len < 0) {
			err = errno;
			return NULL;
		}

		if(peer->Equal(&peeraddr, sock_len, SOCK_DGRAM))
			break;
	}

	return RecvFromFixedSize(fd, len, err, &peeraddr, &sock_len);
}

static char *RecvPacketPoll(int fd, int msec, int &len, int &err)
{
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	int n;
	
	while( (n = poll(&pfd, 1, msec)) < 0 && errno == EINTR)
		/* LOOP IF EINTR */;

	if(n == 0) {
		err = EAGAIN;
		return NULL;
	}
	if(n < 0) {
		err = errno;
		return NULL;
	}

	len = 0;
	ioctl(fd, SIOCINQ, &len);

	return RecvFromFixedSize(fd, len, err, NULL, NULL);
}

// always return network error code
// EAGAIN must re-sent
int NCServer::DecodeResultDgram(SocketAddress *peer, NCResult &tk) 
{
	if(netfd < 0) 
	{
		tk.set_error(-ENOTCONN, "API::recving", "connection is closed");
		return -ENOTCONN;
	}

	int saved_errno; // saved errno
	int len; // recv len
	char *buf; // packet buffer
	uint64_t beginTime = 0;

	log_debug("wait response time stamp = %lld", (long long)(beginTime = GET_TIMESTAMP()));
	
	if(addr.socket_family() == AF_UNIX) {
		if(peer == NULL) {
			buf = RecvPacketPoll(netfd, timeout,  len, saved_errno);
		} else {
			if(peer->connect_socket(netfd) < 0) {
				saved_errno = errno;
				log_error("connect dtc server error: %d,%m", saved_errno);
				if(saved_errno == EAGAIN) {
					// unix socket return EAGAIN if listen queue overflow
					saved_errno = EC_SERVER_BUSY;
				}
				tk.set_error(-saved_errno, "API::connecting", "client api connect server error");
				return -saved_errno;
			}

			buf = RecvPacketPoll(netfd, timeout, len, saved_errno);

			// Un-connect
			struct sockaddr addr;
			addr.sa_family = AF_UNSPEC;
			connect(netfd, &addr, sizeof(addr));
		}
	} else {
		if(peer == NULL) {
			buf = RecvPacketPeek(netfd, len, saved_errno);
		} else {
			buf = RecvPacketPeekPeer(netfd, peer, len, saved_errno);
		}
	}
	log_debug("receive result use time: %ldms", (long)((GET_TIMESTAMP()-beginTime)/1000));

	if(buf == NULL)
	{
		errno = saved_errno;
		log_error("socket[%d] recv udp data error: %d,%m", netfd, saved_errno);

		if(saved_errno == EAGAIN) {
			// change errcode to TIMEDOUT;
			saved_errno = ETIMEDOUT;
			tk.set_error(-ETIMEDOUT, "API::recving", "client recv packet timedout");
		} else {
			if(saved_errno==0)
				saved_errno = ECONNRESET;
			tk.set_error(-saved_errno, "API::recving", "connection reset by peer");
		}
		return -saved_errno;
	}

	int err = tk.Decode(buf, len, 1 /*eat buf*/);

	switch(err) {
	case DecodeFatalError:
		log_error("socket[%d] decode udp data error: invalid packet content", netfd);
		tk.set_error(-ECONNRESET, "API::recving", "invalid packet content");
		FREE_IF(buf); // buf not eaten
		return -ECONNRESET;

	case DecodeWaitData:
	default:
		// UNREACHABLE, DecodePacket never return these code
		log_error("socket[%d] decode udp data error: %d, %m", netfd, errno);
		tk.set_error(-ETIMEDOUT, "API::recving", "client recv packet timedout");
		return -ETIMEDOUT;

	case DecodeDataError:
		// packet maybe valid
		break;

	case DecodeDone:
		SaveDefinition(&tk);
		break;

	}

	return 0;
}

//Get udpport form udppool
NCUdpPort *NCServer::GetGlobalPort()
{
    NCUdpPort *udpport = NCUdpPort::Get(addr.socket_family());

    if (udpport)
    {
	    netfd = udpport->fd;
	    lastSN = udpport->sn;
	    realtmo = udpport->timeout; 
	    UpdateTimeout();
    }

    return udpport;
}

//Put the udpport to the udppool
void NCServer::PutGlobalPort(NCUdpPort *udpport)
{
	if (udpport)
	{
		if (netfd > 0)
		{
			udpport->sn = lastSN;
			udpport->timeout = realtmo;
			netfd = -1;
			udpport->Put();
		}
		else
		{
			//if fd error, delete this udpport object
			udpport->Eat();
		}
	}
}

void NCServer::SaveDefinition(NCResult *tk)
{
	if(strcmp("*", tablename)!=0 && tk->result_code()==-EC_TABLE_MISMATCH) {
		badname = 1;
		errstr = "Table Name Mismatch";
		return;
	}
	if(strcmp("*", tablename)!=0 && tk->result_code()==-EC_BAD_KEY_TYPE) {
		badkey = 1;
		errstr = "Key Type Mismatch";
		return;
	}
	
	DTCTableDefinition *t = tk->remote_table_definition();
	if(t == NULL) return;

	if(t->is_admin_table()){
		if(admin_tdef)
		{
			if(admin_tdef->is_same_table(t)) return;
			if(!autoUpdateTable){
				badname = 1;
				errstr = "Table Mismatch";
				tk->set_error(-EC_TABLE_MISMATCH, "API::executed", "AdminTable Mismatch");
				return;
			}
			DEC_DELETE(admin_tdef);
		}
		admin_tdef = t;
		admin_tdef->INC();
	}
	else{
		if(tdef)
		{
			if(tdef->is_same_table(t)) return;
			if(!autoUpdateTable){
				badname = 1;
				errstr = "Table Mismatch";
				tk->set_error(-EC_TABLE_MISMATCH, "API::executed", "Table Mismatch");
				return;
			}
			DEC_DELETE(tdef);
		}
		tdef = t;
		tdef->INC();
		
		FREE(tablename);
		tablename = STRDUP(tdef->table_name());
		keytype = tdef->key_type();

	    //bugfix， by ada
	    if(keytype == DField::Unsigned)
	        keytype = DField::Signed;

	    if(keytype == DField::Binary)
	        keytype = DField::String;
	}
}

void NCServer::TryPing(void) {
	if(autoping && netfd >= 0) {
		time_t now;
		time(&now);
		if(now >= lastAct)
			Ping();
	}
}

int NCServer::Ping(void) {
	if(ownerPool != NULL)
		return -EC_PARALLEL_MODE;
	if(executor != NULL)
		return 0;
		NCRequest r(this, DRequest::Nop);
	NCResult *t = r.ExecuteNetwork();
	int ret = t->result_code();
	delete t;
	return ret;
}

NCResult *NCServer::DecodeBuffer(const char *buf, int len)
{
	NCResult *res = new NCResult(tdef);

	switch(len <= (int)sizeof(PacketHeader) ? DecodeFatalError : res->Decode(buf, len))
	{
		default:
			res->set_error(-EINVAL, "API::decoding", "invalid packet content");
			break;
		case DecodeDataError:
		case DecodeDone:
			break;
	}
	return res;
}

int NCServer::CheckPacketSize(const char *buf, int len)
{
	return NCResult::check_packet_size(buf, len);
}

/*date:2014/06/04, author:xuxinxin*/
int NCServer::SetAccessKey(const char *token)
{
	int ret = 0;
	std::string str;
	if(token == NULL)
		return -EC_BAD_ACCESS_KEY;
	else
		str = token;
	if(str.length() != ACCESS_KEY_LEN)
	{
		log_error("Invalid accessKey!");
		accessToken = "";
		return -EC_BAD_ACCESS_KEY;
	}
	else
		accessToken = str;

	/* 截取  AccessKey中前32位以后的bid， 以便上报至数据采集 dtchttpd */
	ret = dc->SetBussinessId(accessToken);
	//ret = dc->SetTopPercentileConfig(accessToken, this->GetAddress());
	return 0;
}

DataConnector* DataConnector::pDataConnector = NULL;

pthread_mutex_t wakeLock;
static void *Process(void *p)
{
	sigset_t sset;
	sigfillset(&sset);
	sigdelset(&sset, SIGSEGV);
	sigdelset(&sset, SIGBUS);
	sigdelset(&sset, SIGABRT);
	sigdelset(&sset, SIGILL);
	sigdelset(&sset, SIGCHLD);
	sigdelset(&sset, SIGFPE);
	pthread_sigmask(SIG_BLOCK, &sset, &sset);

	int ret = 0;
	time_t next = 0;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int offset = random() % 10;
	next = (tv.tv_sec / 10) * 10 + 10 + offset;
	struct timespec ts;
	ts.tv_sec = next;
	ts.tv_nsec = 0;

	while(pthread_mutex_timedlock(&wakeLock, &ts)!=0)
	{
		gettimeofday(&tv, NULL);
		if(tv.tv_sec >= next)
		{
			ret = DataConnector::getInstance()->SendData();
			ret = DataConnector::getInstance()->SendTopPercentileData();
			gettimeofday(&tv, NULL);
			next = (tv.tv_sec / 10) * 10 + 10 + offset;
		}
		ts.tv_sec = next;
		ts.tv_nsec = 0;
	}
	pthread_mutex_unlock(&wakeLock);
	return NULL;
}

/* date:2014/06/09, author:xuxinxin */
DataConnector::DataConnector()
{
}

DataConnector::~DataConnector()
{
}

int DataConnector::SendData()
{
	return 0;
}

int DataConnector::SetReportInfo(const std::string str, const uint32_t curve, const uint64_t t)
{
	uint32_t bid = 0;
	std::string stemp = str.substr(0,8);
	sscanf(stemp.c_str(), "%u", &bid);
	struct bidCurve bc;
	bc.bid = bid;
	bc.curve = curve;

	if(mapBi.find(bc) != mapBi.end())
	{
		do
		{
			ScopedLock lock(_lock);
			mapBi[bc].TotalTime += t;
			mapBi[bc].TotalRequests += 1;
		}while(0);
	}
	return 0;
}

void DataConnector::GetReportInfo(std::map<bidCurve, businessStatistics> &map)
{
	ScopedLock lock(_lock);
	for(std::map<bidCurve, businessStatistics>::iterator it = mapBi.begin(); it != mapBi.end(); ++it)
	{
		struct businessStatistics bs;
		map[it->first] = it->second;
		/*  mapBi 取出来之后，不管后面是否上报成功，该bid对应的数据都丢弃，重置为0  */
		mapBi[it->first] = bs;
	}
}

int DataConnector::SetBussinessId(std::string str)
{
	ScopedLock lock(_lock);
	if(mapBi.size() == 0)
	{
		pthread_mutex_init(&wakeLock, NULL);
		if(pthread_mutex_trylock(&wakeLock) == 0)
		{
			int ret = pthread_create(&threadid, 0, Process, (void *)NULL);
			if(ret != 0)
			{
				errno = ret;
				return -1;
			}
		}
	}
	std::string stemp = str.substr(0, 8);
	uint32_t bid = 0;
	sscanf(stemp.c_str(), "%u", &bid);

	struct businessStatistics bs;
	struct bidCurve bclient, bagent;
	bclient.bid = bid;
	bclient.curve = CLIENT_CURVE;

	bagent.bid = bid;
	bagent.curve = AGENT_CURVE;
	mapBi.insert(std::make_pair(bclient, bs));
	mapBi.insert(std::make_pair(bagent, bs));
	return 0;
}

int DataConnector::SendTopPercentileData()
{
	return 0;
}

int DataConnector::SetTopPercentileData(const std::string strAccessKey, const std::string strAgentAddr, const uint64_t elapse, const int status)
{
	if(strAccessKey.empty())
		return -1;
	if(strAgentAddr.empty())
		return -1;
	if(!isdigit(strAgentAddr[0]))
		return -1;

	uint32_t uiBid = 0;
	std::string stemp = strAccessKey.substr(0,8);
	sscanf(stemp.c_str(), "%u", &uiBid);
	
	uint64_t uiIP;
	uint64_t uiPort;
	if(ParseAddr(strAgentAddr, &uiIP, &uiPort) < 0)
		return -1;

	int16_t iRealSite = CTopPercentileSection::GetTPSection(elapse);
	if(iRealSite < 0 || iRealSite >= sizeof(kpi_sample_count) / sizeof(kpi_sample_count[0]))
		return -1;

	uint64_t uiKey = (uiIP << 32) | (uiPort << 16) | (uint64_t)uiBid;
	std::map<uint64_t, DataConnector::top_percentile_statistics>::iterator it;
	ScopedLock lock(m_tp_lock);
	if((it = mapTPStat.find(uiKey)) == mapTPStat.end())
	{
		DataConnector::top_percentile_statistics tpStat;
		tpStat.uiBid = uiBid;
		tpStat.uiAgentIP = uiIP;
		tpStat.uiAgentPort = uiPort;
		std::pair<std::map<uint64_t, DataConnector::top_percentile_statistics>::iterator, bool> pairRet;
		pairRet = mapTPStat.insert(std::make_pair(uiKey, tpStat));
		if(!pairRet.second)
			return -1;
		it = pairRet.first;
	}

	if(it->second.uiMaxTime < elapse)
		it->second.uiMaxTime = elapse;
	if(0 == it->second.uiMinTime)
		it->second.uiMinTime = elapse;
	else if(it->second.uiMinTime > elapse)
		it->second.uiMinTime = elapse;
	if(0 != status)
		it->second.uiFailCount ++;
	it->second.uiTotalRequests ++;
	it->second.uiTotalTime += elapse;
	it->second.statArr[iRealSite] ++;
	
	return 0;
}

void DataConnector::GetTopPercentileData(std::map<uint64_t, top_percentile_statistics> &mapStat)
{
	ScopedLock lock(m_tp_lock);
	std::swap(mapTPStat, mapStat);
}

/*
 *@strAccessKey:00001460ccea307caad065107c936c88bc59b1a4
 *@strAgentAddr:192.168.145.135:20024/tcp
 */
int DataConnector::SetTopPercentileConfig(const std::string strAccessKey, const std::string strAgentAddr)
{
	if(strAccessKey.empty())
		return -1;
	if(strAgentAddr.empty())
		return -1;
	if(!isdigit(strAgentAddr[0]))
		return -1;
	uint64_t uiIP = 0;
	uint64_t uiPort = 0;
	if(ParseAddr(strAgentAddr, &uiIP, &uiPort) < 0)
		return -1;
	uint32_t uiBid = 0;
	std::string strTemp = strAccessKey.substr(0,8);
	sscanf(strTemp.c_str(), "%u", &uiBid);
	struct top_percentile_statistics tpStat;
	tpStat.uiBid = uiBid;
	tpStat.uiAgentIP = (uint32_t)uiIP;
	tpStat.uiAgentPort = (uint16_t)uiPort;
	mapTPStat.insert(std::make_pair((uiIP << 32) | (uiPort << 16) | uiBid, tpStat));
	return 0;
}

/*
 *@strAddr:192.168.145.135:20024/tcp
 */
int DataConnector::ParseAddr(const std::string strAddr, uint64_t *uipIP /* = NULL */, uint64_t *uipPort /* = NULL */)
{
	if(strAddr.empty())
		return -1;
	if(!isdigit(strAddr[0]))
		return -1;
	const char *szAgentAddr = strAddr.c_str();
	const char *szPortProtocol = strchr(strAddr.c_str(), ':');
	if(NULL == szPortProtocol)
		return -1;
	//":"后面必然需要有端口，若是长度小于2，则数据有问题
	if(strlen((char *)szPortProtocol) < 2)
		return -1;
	if(!isdigit(szPortProtocol[1]))
		return -1;
	struct in_addr addr;
	uint32_t uiLen = szPortProtocol - szAgentAddr;
	char szIP[uiLen + 1];
	memcpy(szIP, szAgentAddr, uiLen);
	szIP[uiLen] = 0;
	if(1 != inet_pton(AF_INET, szIP, &addr))
		return -1;
	*uipIP = 0;
	*uipPort = 0;
	*uipIP = (uint64_t)ntohl(addr.s_addr);
	*uipPort = (uint64_t)atoi(szPortProtocol + 1);
	if(0 == *uipIP || 0 == *uipPort)
		return -1;
	return 0;
}

/*
 *以下数组中的值对应的为kpi_sample_count数组的下标，即跟随在其后注释中的数值所在的位置
 */
int16_t CTopPercentileSection::GetTPSection(uint64_t elapse)
{
	if(elapse < THOUSAND)
		return GetLTThousand(elapse);
	else if(THOUSAND <= elapse && elapse < TENTHOUSAND)
		return GetGTEThousandLTTenThousand(elapse);
	else if(TENTHOUSAND <= elapse && elapse < HUNDREDTHOUSAND)
		return GetGTETenThousandLTHundredThousand(elapse);
	else if(HUNDREDTHOUSAND <= elapse && elapse < MILLION)
		return GetGTEHundredThousandLTMillion(elapse);
	else
		return GetGTEMillion(elapse);

	return -1;
}

int16_t CTopPercentileSection::GetLTThousand(uint64_t elapse)
{
	if(elapse < THOUSAND)
	{
		static int16_t iSiteArr[] =
		{
			0,/*200us下标*/
			0,/*200us下标*/
			1,/*500us下标*/
			1,/*500us下标*/
			1,/*500us下标*/
			2,/*1000us下标*/
			2,/*1000us下标*/
			2,/*1000us下标*/
			2,/*1000us下标*/
			2/*1000us下标*/
		};
		int16_t iSite = elapse / HUNDRED;
		if(iSite < 0 || iSite >= int16_t(sizeof(iSiteArr) / sizeof(iSiteArr[0])))
			return -1;
		return iSiteArr[iSite];
	}
	return -1;
}

int16_t CTopPercentileSection::GetGTEThousandLTTenThousand(uint64_t elapse)
{
	if(THOUSAND <= elapse && elapse < TENTHOUSAND)
	{
		static int16_t iSiteArr[] =
		{
			3,/*2000us下标*/
			4,/*3000us下标*/
			5,/*4000us下标*/
			6,/*5000us下标*/
			7,/*6000us下标*/
			8,/*7000us下标*/
			9,/*8000us下标*/
			10,/*9000us下标*/
			11/*10000us下标*/
		};
		int16_t iSite = elapse / THOUSAND - 1;
		if(iSite < 0 || iSite >= int16_t(sizeof(iSiteArr) / sizeof(iSiteArr[0])))
			return -1;
		return iSiteArr[iSite];
	}
	return -1;
}

int16_t CTopPercentileSection::GetGTETenThousandLTHundredThousand(uint64_t elapse)
{
	if(TENTHOUSAND <= elapse && elapse < HUNDREDTHOUSAND)
	{
		static int16_t iSiteArr[] =
		{
			12,/*20000us下标*/
			13,/*30000us下标*/
			14,/*40000us下标*/
			15,/*50000us下标*/
			16,/*60000us下标*/
			17,/*70000us下标*/
			18,/*80000us下标*/
			19,/*90000us下标*/
			20/*100000us下标*/
		};
		int16_t iSite = elapse / TENTHOUSAND - 1;
		if(iSite < 0 || iSite >= int16_t(sizeof(iSiteArr) / sizeof(iSiteArr[0])))
			return -1;
		return iSiteArr[iSite];
	}
	return -1;
}

int16_t CTopPercentileSection::GetGTEHundredThousandLTMillion(uint64_t elapse)
{
	if(HUNDREDTHOUSAND <= elapse && elapse < MILLION)
	{
		static int16_t iSiteArr[] =
		{
			21,/*200000us下标*/
			22,/*300000us下标*/
			23,/*400000us下标*/
			24,/*500000us下标*/
			25,/*600000us下标*/
			26,/*700000us下标*/
			27,/*800000us下标*/
			28,/*900000us下标*/
			29/*1000000us下标*/
		};
		int16_t iSite = elapse / HUNDREDTHOUSAND - 1;
		if(iSite < 0 || iSite >= int16_t(sizeof(iSiteArr) / sizeof(iSiteArr[0])))
			return -1;
		return iSiteArr[iSite];
	}
	return -1;
}

int16_t CTopPercentileSection::GetGTEMillion(uint64_t elapse)
{
	if(elapse >= MILLION)
	{
		static int16_t iSiteArr[] =
		{
			30,/*2000000us下标*/
			31,/*5000000us下标*/
			31,/*5000000us下标*/
			31,/*5000000us下标*/
			32,/*10000000us下标*/
			32,/*10000000us下标*/
			32,/*10000000us下标*/
			32,/*10000000us下标*/
			32,/*10000000us下标*/
			32/*10000000us下标*/
		};
		int16_t iSite = elapse / MILLION - 1;
		int16_t iArrLen = int16_t(sizeof(iSiteArr) / sizeof(iSiteArr[0]));
		iSite = (iSite >= iArrLen) ? (iArrLen - 1) : iSite;
		if(iSite < 0)
			return -1;
		return iSiteArr[iSite];
	}
	return -1;
}
