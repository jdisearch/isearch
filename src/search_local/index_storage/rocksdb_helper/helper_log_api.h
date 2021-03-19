
/*
 * =====================================================================================
 *
 *       Filename:  helper_log_api.h 
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
#ifndef __HELPER_LOG_API_H__
#define __HELPER_LOG_API_H__

#if HAS_LOGAPI

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "msglogapi.h"
#include "logfmt.h"
struct HelperLogApi
{
	HelperLogApi();
	~HelperLogApi();

	TMsgLog log;
	unsigned int msec;
	unsigned int iplocal, iptarget;
	int id[4];

	void Init(int id0, int id1, int id2, int id3);
	void init_target(const char *host);
	void Start(void);
	void Done(const char *fn, int ln, const char *op, int ret, int err);
};

inline HelperLogApi::CHelperLogApi(void) : log(6, "dtc")
{
}

inline HelperLogApi::~CHelperLogApi(void)
{
}

extern "C"
{
	extern unsigned int get_local_ip();
}
inline void HelperLogApi::Init(int id0, int id1, int id2, int id3)
{
	log_debug("LogApi: id %d caller %d target %d interface %d", id0, id1, id2, id3);
	id[0] = id0;
	id[1] = id1;
	id[2] = id2;
	id[3] = id3;
	iplocal = get_local_ip();
	log_debug("local ip is %x\n", iplocal);
}

inline void HelperLogApi::init_target(const char *host)
{
	if (host == NULL || host[0] == '\0' || host[0] == '/' || !strcasecmp(host, "localhost"))
		host = "127.0.0.1";

	struct in_addr a;
	a.s_addr = inet_addr(host);
	iptarget = *(unsigned int *)&a;

	log_debug("remote ip is %x\n", iptarget);
}

inline void HelperLogApi::Start(void)
{
	INIT_MSEC(msec);
}

inline void HelperLogApi::Done(const char *fn, int ln, const char *op, int ret, int err)
{
	CALC_MSEC(msec);
	if (id[0] == 0)
		return;
	log.msgprintf((unsigned int)6, (unsigned long)id[0],
				  MAINTENANCE_MONITOR_MODULE_INTERFACE_LOG_FORMAT,
				  id[1], id[2], id[3], // 0: ID
				  iplocal, iptarget,   // 3: IP
				  0, 0,				   // 5: PORT
				  fn, ln,			   // 7: source position
				  "",				   // 9: file modification time
				  op,				   // 10: operation
				  err,				   // 11: MySQL ErrNo
				  ret ? 1 : 0,		   // 12: status
				  msec, -1,			   // 13: timing
				  0, 0, 0, 0, "", "", "", "", "");
}

#else

struct HelperLogApi
{
	void Init(int id0, int id1, int id2, int id3) {}
	void init_target(const char *host) {}
	void Start(void) {}
	void Done(const char *fn, int ln, const char *op, int ret, int err) {}
};

#endif

#endif
