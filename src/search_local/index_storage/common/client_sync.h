/*
 * =====================================================================================
 *
 *       Filename:  client_sync.h
 *
 *    Description:  client sync.
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
#ifndef __CLIENT_SYNC_H__
#define __CLIENT_SYNC_H__

#include "poller.h"
#include "timer_list.h"
#include "receiver.h"

class DTCDecoderUnit;
class TaskRequest;
class Packet;
class ClientResourceSlot;

class ClientSync : public PollerObject, private TimerObject
{
public:
    DTCDecoderUnit *owner;
    void *addr;
    int addrLen;
    unsigned int resourceId;
    ClientResourceSlot *resource;
    uint32_t resourceSeq;
    enum RscStatus
    {
        RscClean,
        RscDirty,
    };
    RscStatus rscStatus;

    ClientSync(DTCDecoderUnit *, int fd, void *, int);
    virtual ~ClientSync();

    virtual int Attach(void);
    int send_result(void);

    virtual void input_notify(void);

private:
    virtual void output_notify(void);

    int get_resource();
    void free_resource();
    void clean_resource();

protected:
    enum ClientState
    {
        IdleState,
        RecvReqState, //wait for recv request, server side
        SendRepState, //wait for send response, server side
        ProcReqState, // IN processing
    };

    SimpleReceiver receiver;
    ClientState stage;
    TaskRequest *task;
    Packet *reply;

    int recv_request(void);
    int Response(void);
    void adjust_events(void);
};

#endif
