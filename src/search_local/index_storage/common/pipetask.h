/*
 * =====================================================================================
 *
 *       Filename:  pipetask.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  10/05/2014 17:40:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming (prudence), linjinming@jd.com
 *        Company:  JD, China
 *
 * =====================================================================================
 */
#ifndef __H_DTC_PIPETASK_TEMP_H__
#define __H_DTC_PIPETASK_TEMP_H__

#include "log.h"
#include "pipequeue.h"
#include "compiler.h"

template <typename T>
class TaskDispatcher;
template <typename T>
class ReplyDispatcher;
template <typename T>
class RequestOutput;

template <typename T>
class TaskIncomingPipe : public PipeQueue<T, TaskIncomingPipe<T> >
{
public:
	TaskIncomingPipe(void) {}
	virtual ~TaskIncomingPipe() {}
	inline void task_notify(T p)
	{
		proc->task_notify(p);
	}

public:
	TaskOneWayDispatcher<T> *proc;
};

template <typename T>
class TaskReturnPipe : public PipeQueue<T *, TaskReturnPipe<T> >
{
public:
	TaskReturnPipe(){};
	virtual ~TaskReturnPipe(){};
	inline void task_notify(T *p)
	{
		p->reply_notify();
	}
};

template <typename T>
class TaskPipe : public TaskDispatcher<T>,
				 public ReplyDispatcher<T>
{
private:
	static LinkQueue<TaskPipe<T> *> pipelist;

public:
	inline TaskPipe() { pipelist.Push(this); }
	~TaskPipe() {}

	virtual void task_notify(T *p)
	{
		p->push_reply_dispatcher(this);
		incQueue.Push(p);
	}
	virtual void reply_notify(T *p)
	{
		retQueue.Push(p);
	}
	inline int bind_dispatcher(RequestOutput<T> *fr, TaskDispatcher<T> *to)
	{
#if 0
		log_debug("Bind taskpipe from thread %s to thread %s",
				fr->owner_thread()->Name(), to->owner_thread()->Name());
#endif
		TaskDispatcher<T>::owner = fr->owner_thread();

		incQueue.attach_poller(fr->owner_thread(), to->owner_thread());
		retQueue.attach_poller(to->owner_thread(), fr->owner_thread());

		fr->bind_dispatcher(this);
		incQueue.proc = to;
		fr->disable_queue();
		return 0;
	}

	static inline void destroy_all(void)
	{
		TaskPipe *p;
		while ((p = pipelist.Pop()) != NULL)
		{
			delete p;
		}
	}

private:
	TaskIncomingPipe<T *> incQueue;
	TaskReturnPipe<T> retQueue;
};

template <typename T>
LinkQueue<TaskPipe<T> *> TaskPipe<T>::pipelist;

#endif
