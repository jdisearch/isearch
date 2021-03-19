/*
 * =====================================================================================
 *
 *       Filename:  client_dgram.h
 *
 *    Description:  client dgram.
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
#ifndef __CLIENT__DGRAM_H__
#define __CLIENT__DGRAM_H__

#include <sys/socket.h>
#include "poller.h"
#include "task_request.h"
#include "packet.h"
#include "timer_list.h"

class DTCDecoderUnit;
class ClientDgram;

struct DgramInfo
{
	ClientDgram *cli;
	socklen_t len;
	char addr[0];
};

class ClientDgram : public PollerObject
{
public:
	DTCDecoderUnit *owner;

	ClientDgram(DTCDecoderUnit *, int fd);
	virtual ~ClientDgram();

	virtual int Attach(void);
	int send_result(TaskRequest *, void *, int);

protected:
	// recv on empty&no packets
	int recv_request(int noempty);

private:
	int hastrunc;
	int mru;
	int mtu;
	int alen;		 // address length
	DgramInfo *abuf; // current packet address

	virtual void input_notify(void);
	int allocate_dgram_info(void);
	int init_socket_info(void);
};

#endif
