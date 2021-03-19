/*
 * =====================================================================================
 *
 *       Filename:  packet.h
 *
 *    Description:  packet class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __CH_PACKET_H__
#define __CH_PACKET_H__

#include <sys/uio.h>

#include "protocol.h"
#include "value.h"
#include "sockaddr.h"
#include "log.h"
#include "memcheck.h"

class NCRequest;
union CValue;
class CTask;
class CTaskRequest;
class CTableDefinition;

enum CSendResult {
	SendResultError,
	SendResultMoreData,
	SendResultDone
};

/* just one buff malloced */
class CPacket {
private:
	struct iovec *v;
	int nv;
	int bytes;
	CBufferChain *buf;
	int sendedVecCount;
	
	CPacket(const CPacket &);
public:
	CPacket() : v(NULL), nv(0), bytes(0), buf(NULL), sendedVecCount(0) { };
	~CPacket() {
		/* free buffer chain buffer in several place, not freed all here */
            FREE_IF(buf);
	}

	inline void Clean()
	{
	    v = NULL;
	    nv = 0;
	    bytes = 0;
	    if(buf)
		buf->Clean();
	}
//	int Send(int fd);
//	int SendTo(int fd, void *name, int namelen);
//	int SendTo(int fd, CSocketAddress *addr) {
//		return addr==NULL ? Send(fd) : SendTo(fd, (void *)addr->addr, (int)addr->alen);
//	}

	/* for agent_sender */
	int Bytes(void);
	void FreeResultBuff();
	const struct iovec * IOVec() { return v; }
        int VecCount() { return nv; }
        void SendDoneOneVec() { sendedVecCount++; }
        bool IsSendDone() { return sendedVecCount == nv; }


	static int EncodeHeader(const CPacketHeader &header);
//	int EncodeForwardRequest(CTaskRequest &);
//	int EncodePassThru(CTask &);
//	int EncodeFetchData(CTaskRequest &);

	// encode result, for helper/server reply
	// side effect:
	// 	if error code set(ResultCode()<0), ignore ResultSet
	// 	if no result/error code set, no ResultCode() to zero
	// 	if no result key set, set result key to request key
	int EncodeResult(CTask &, int mtu=0, uint32_t ts=0);
	int EncodeResult(CTaskRequest &, int mtu = 0);
	int EncodeDetect(const CTableDefinition * tdef, int sn=1);
	int EncodeRequest(NCRequest &r, const CValue *k=NULL);
	static int EncodeSimpleRequest(NCRequest &rq, const CValue *kptr, char * &ptr, int &len);
	char *AllocateSimple(int size);

	int EncodeResult(CTask *task, int mtu=0, uint32_t ts=0) {
		return EncodeResult(*task, mtu, ts);
	}
	int EncodeResult(CTaskRequest *task, int mtu=0) {
		return EncodeResult(*task, mtu);
	}
//	int EncodeForwardRequest(CTaskRequest* task) {
//		return EncodeForwardRequest(*task);
//	}

	/* for agent */
	int EncodeAgentRequest(NCRequest & rq, const CValue * kptr);
};

#endif
