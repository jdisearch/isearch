/*
 * =====================================================================================
 *
 *       Filename:  request_base.h
 *
 *    Description:  request task class base.
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
#ifndef __H_DTC_REQUEST_TEMP_H__
#define __H_DTC_REQUEST_TEMP_H__

#include <endian.h>
#include <stdlib.h>

#include "lqueue.h"
#include "log.h"
#include "timer_list.h"

template <typename T>
class TaskPipe;

class PollThread;

template <typename T>
class TaskOneWayDispatcher
{
public:
	PollThread *owner;

public:
	inline TaskOneWayDispatcher(PollThread *p = NULL) : owner(p) {}
	virtual inline ~TaskOneWayDispatcher() {}
	virtual void task_notify(T) = 0;

	inline PollThread *owner_thread(void) { return owner; }
	inline void attach_thread(PollThread *thread)
	{
		if (owner == NULL)
			owner = thread;
	}
};

template <typename T>
class TaskDispatcher : public TaskOneWayDispatcher<T *>
{
public:
	inline TaskDispatcher(PollThread *p = NULL) : TaskOneWayDispatcher<T *>(p) {}
	virtual inline ~TaskDispatcher() {}
};

template <typename T>
class ReplyDispatcher
{
public:
	inline ReplyDispatcher() {}
	virtual inline ~ReplyDispatcher() {}
	virtual void reply_notify(T *) = 0;
};

template <typename T>
class RequestOutput : private TimerObject
{
private:
	PollThread *owner;
	TaskDispatcher<T> *proc;
	LinkQueue<T *> queue;
	typename LinkQueue<T *>::allocator allocator;

protected:
	int queueDisabled;

private:
	inline virtual void timer_notify(void)
	{
		T *p;
		while ((p = queue.Pop()) != NULL)
			task_notify(p);
	}

public:
	inline RequestOutput(PollThread *o) : owner(o),
										  proc(NULL),
										  queue(&allocator),
										  queueDisabled(0)
	//reply(NULL)
	{
	}
	inline ~RequestOutput() {}
	inline PollThread *owner_thread(void) { return owner; }
	inline void bind_dispatcher(TaskDispatcher<T> *out)
	{
		if (owner == out->owner_thread())
		{
			proc = out;
		}
		else
		{
			TaskPipe<T> *tp = new TaskPipe<T>();
			tp->bind_dispatcher(this, out);
		}
	}
	inline void task_notify(T *p)
	{
		proc->task_notify(p);
	}
	inline void indirect_notify(T *p)
	{
		if (queueDisabled)
			task_notify(p);
		else
		{
			queue.Push(p);
			attach_ready_timer(owner);
		}
	}
	inline TaskDispatcher<T> *get_dispatcher(void) { return proc; }
	inline void disable_queue(void) { queueDisabled = 1; }
};

template <typename T, int maxcount = 10>
class TaskReplyList
{
public:
	ReplyDispatcher<T> *proc[maxcount];
	int count;

public:
	inline TaskReplyList(void) { count = 0; };
	inline virtual ~TaskReplyList(void){};
	inline void Clean()
	{
		memset(proc, 0, sizeof(ReplyDispatcher<T> *) * maxcount);
		count = 0;
	}
	inline void copy_reply_path(TaskReplyList<T, 10> *org)
	{
		memcpy(proc, org->proc, sizeof(ReplyDispatcher<T> *) * maxcount);
		count = org->count;
	}
	inline int Push(ReplyDispatcher<T> *p)
	{
		if (count >= maxcount)
			return -1;
		proc[count++] = p;
		return 0;
	}
	inline ReplyDispatcher<T> *Pop(void)
	{
		return count == 0 ? NULL : proc[--count];
	}
	inline void Dump(void)
	{
		for (int i = 0; i < count; i++)
			log_debug("replyproc%d: %p", i, proc[i]);
	}
	inline void push_reply_dispatcher(ReplyDispatcher<T> *proc)
	{
		if (proc == NULL)
			static_cast<T *>(this)->Panic("push_reply_dispatcher: dispatcher is NULL, check your code");
		else if (Push(proc) != 0)
			static_cast<T *>(this)->Panic("push_reply_dispatcher: push queue failed, possible memory exhausted");
	}
	inline void reply_notify(void)
	{
		ReplyDispatcher<T> *proc = Pop();
		if (proc == NULL)
			static_cast<T *>(this)->Panic("reply_notify: no more dispatcher, possible double reply");
		else
			proc->reply_notify(static_cast<T *>(this));
	}
	void Panic(const char *msg);
};

template <typename T, int maxcount>
void TaskReplyList<T, maxcount>::Panic(const char *msg)
{
	log_crit("Internal Error Encountered: %s", msg);
}

#endif
