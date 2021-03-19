/*
 * =====================================================================================
 *
 *       Filename:  request_threading.h
 *
 *    Description:  
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
#ifndef __H_DTC_REQUEST_THREAD_H__
#define __H_DTC_REQUEST_THREAD_H__

#include "mtpqueue.h"
#include "wait_queue.h"
#include "request_base_all.h"

template <typename T>
class ThreadingOutputDispatcher
{
private: // internal implementation
	class InternalTaskDispatcher : public ThreadingPipeQueue<T *, InternalTaskDispatcher>
	{
	public:
		TaskDispatcher<T> *proc;

	public:
		InternalTaskDispatcher() : proc(0) {}
		virtual ~InternalTaskDispatcher() {}
		void task_notify(T *p)
		{
			proc->task_notify(p);
		};
	};

	class InternalReplyDispatcher : public ReplyDispatcher<T>, public threading_wait_queue<T *>
	{
	public:
		InternalReplyDispatcher *freenext;
		InternalReplyDispatcher *allnext;

	public:
		InternalReplyDispatcher() : freenext(0), allnext(0) {}
		virtual ~InternalReplyDispatcher() {}
		virtual void reply_notify(T *p)
		{
			Push(p);
		};
	};

private:
	InternalTaskDispatcher incQueue;
	pthread_mutex_t lock;
	// lock management, protect freeList and allList
	inline void Lock(void) { pthread_mutex_lock(&lock); }
	inline void Unlock(void) { pthread_mutex_unlock(&lock); }
	InternalReplyDispatcher *volatile freeList;
	InternalReplyDispatcher *volatile allList;
	volatile int stopping;

public:
	ThreadingOutputDispatcher() : freeList(0), allList(0), stopping(0)
	{
		pthread_mutex_init(&lock, NULL);
	}
	~ThreadingOutputDispatcher()
	{
		InternalReplyDispatcher *q;
		while (allList)
		{
			q = allList;
			allList = q->allnext;
			delete q;
		}
		pthread_mutex_destroy(&lock);
	}

	void Stop(void)
	{
		stopping = 1;
		InternalReplyDispatcher *p;
		for (p = allList; p; p = p->allnext)
			p->Stop(NULL);
	}

	int Stopping(void) { return stopping; }

	int execute(T *p)
	{
		InternalReplyDispatcher *q;

		// aborted without side-effect
		if (Stopping())
			return -1;

		/* freelist被别的线程在lock锁住的时候被别的线程置成了NULL */
		Lock();
		if (freeList)
		{
			q = freeList;
			freeList = q->freenext;
			q->Clear();
			q->freenext = NULL;
		}
		else
		{
			q = new InternalReplyDispatcher();
			q->allnext = allList;
			allList = q;
		}
		Unlock();

		p->push_reply_dispatcher(q);
		incQueue.Push(p);
		// has side effect
		if (q->Pop() == NULL)
		{
			// q leaked by purpose
			// because an pending reply didn't Popped
			return -2;
		}

		Lock();
		q->freenext = freeList;
		freeList = q;
		Unlock();
		return 0;
	}

	int bind_dispatcher(TaskDispatcher<T> *to)
	{
		log_debug("Create ThreadingOutputDispatcher to thread %s",
				  to->owner_thread()->Name());
		incQueue.proc = to;
		return incQueue.attach_poller(to->owner_thread());
	}
};

#endif
