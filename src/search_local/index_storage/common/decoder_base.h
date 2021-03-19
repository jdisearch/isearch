/*
 * =====================================================================================
 *
 *       Filename:  decoder_base.h
 *
 *    Description:  decode unit.
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
#ifndef __H_DTC_DECODER_UNIT_H__
#define __H_DTC_DECODER_UNIT_H__

class PollThread;
class TimerList;

class DecoderUnit
{
public:
	DecoderUnit(PollThread *, int idletimeout);
	virtual ~DecoderUnit();

	TimerList *idle_list(void) const { return idleList; }
	int idle_time(void) const { return idleTime; }
	PollThread *owner_thread(void) const { return owner; }

	virtual int process_stream(int fd, int req, void *, int) = 0;
	virtual int process_dgram(int fd) = 0;

protected:
	PollThread *owner;
	int idleTime;
	TimerList *idleList;
};

#endif
