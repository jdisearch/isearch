/*
 * =====================================================================================
 *
 *       Filename:  agent_receiver.h
 *
 *    Description:  agent_receiver class definition.
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

#ifndef __AGENT_RECEIVER_H__
#define __AGENT_RECEIVER_H__

#include <stdint.h>

#include "value.h"
#include "protocol.h"

#define AGENT_INIT_RECV_BUFF_SIZE 4096

typedef struct{
    char * buff;
    int len;
    int pktCnt;
    int err;
}RecvedPacket;

class CAgentReceiver
{
    public:
	CAgentReceiver(int f);
	virtual ~CAgentReceiver();

	int Init();
        RecvedPacket Recv();

    private:
	int fd;
	char * buffer;
	uint32_t offset;
	uint32_t buffSize;
	uint32_t pktTail;
	int pktCnt;

    private:
        int EnlargeBuffer();
	bool IsNeedEnlargeBuffer();
	int RecvOnce();
	int RealRecv();	
	int RecvAgain();
	int DecodeHeader(CPacketHeader * header);
	void SetRecvedInfo(RecvedPacket & packet);
	int CountPacket();
};

#endif
