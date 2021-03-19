/*
 * =====================================================================================
 *
 *       Filename:  proxy_sender.h
 *
 *    Description:  agent sender.
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
#ifndef __AGENT_SENDER_H__
#define __AGENT_SENDER_H__

#include <stdint.h>

#define SENDER_MAX_VEC 1024

class Packet;
class AgentSender
{
private:
	int fd;
	struct iovec *vec;
	uint32_t totalVec;
	uint32_t currVec;
	Packet **packet;
	uint32_t totalPacket;
	uint32_t currPacket;

	uint32_t totalLen;
	uint32_t sended;
	uint32_t leftLen;

	int broken;

public:
	AgentSender(int f);
	virtual ~AgentSender();

	int Init();
	int is_broken() { return broken; }
	int add_packet(Packet *pkt);
	int Send();
	inline uint32_t total_len() { return totalLen; }
	inline uint32_t Sended() { return sended; }
	inline uint32_t left_len() { return leftLen; }
	inline uint32_t vec_count() { return currVec; }
};

#endif
