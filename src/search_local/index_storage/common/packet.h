/*
 * =====================================================================================
 *
 *       Filename:  packet.h
 *
 *    Description:  packet operation.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __CH_PACKET_H__
#define __CH_PACKET_H__

#include <sys/uio.h>

#include "protocol.h"
#include "section.h"
#include "net_addr.h"
#include "log.h"

class NCRequest;
union DTCValue;
class DTCTask;
class TaskRequest;
class DTCTableDefinition;

enum DTCSendResult
{
	SendResultError,
	SendResultMoreData,
	SendResultDone
};

/* just one buff malloced */
class Packet
{
private:
	struct iovec *v;
	int nv;
	int bytes;
	BufferChain *buf;
	int sendedVecCount;

	Packet(const Packet &);

public:
	Packet() : v(NULL), nv(0), bytes(0), buf(NULL), sendedVecCount(0){};
	~Packet()
	{
		/* free buffer chain buffer in several place, not freed all here */
		FREE_IF(buf);
	}

	inline void Clean()
	{
		v = NULL;
		nv = 0;
		bytes = 0;
		if (buf)
			buf->Clean();
	}
	int Send(int fd);
	int send_to(int fd, void *name, int namelen);
	int send_to(int fd, SocketAddress *addr)
	{
		return addr == NULL ? Send(fd) : send_to(fd, (void *)addr->addr, (int)addr->alen);
	}

	/* for agent_sender */
	int Bytes(void);
	void free_result_buff();
	const struct iovec *IOVec() { return v; }
	int vec_count() { return nv; }
	void send_done_one_vec() { sendedVecCount++; }
	bool is_send_done() { return sendedVecCount == nv; }

	static int encode_header(PacketHeader &header);
#if __BYTE_ORDER == __LITTLE_ENDIAN
	static int encode_header(const PacketHeader &header);
#endif
	int encode_forward_request(TaskRequest &);
	int encode_pass_thru(DTCTask &);
	int encode_fetch_data(TaskRequest &);

	// encode result, for helper/server reply
	// side effect:
	// 	if error code set(result_code()<0), ignore DTCResultSet
	// 	if no result/error code set, no result_code() to zero
	// 	if no result key set, set result key to request key
	int encode_result(DTCTask &, int mtu = 0, uint32_t ts = 0);
	int encode_result(TaskRequest &, int mtu = 0);
	int encode_detect(const DTCTableDefinition *tdef, int sn = 1);
	int encode_request(NCRequest &r, const DTCValue *k = NULL);
	static int encode_simple_request(NCRequest &rq, const DTCValue *kptr, char *&ptr, int &len);
	char *allocate_simple(int size);

	int encode_result(DTCTask *task, int mtu = 0, uint32_t ts = 0)
	{
		return encode_result(*task, mtu, ts);
	}
	int encode_result(TaskRequest *task, int mtu = 0)
	{
		return encode_result(*task, mtu);
	}
	int encode_forward_request(TaskRequest *task)
	{
		return encode_forward_request(*task);
	}

	/* for agent */
	int encode_agent_request(NCRequest &rq, const DTCValue *kptr);

	int encode_reload_config(const DTCTableDefinition *tdef, int sn = 1);
};

#endif
