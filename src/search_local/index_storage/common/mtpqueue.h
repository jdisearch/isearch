#ifndef __PIPE_MTQUEUE_H__
#define __PIPE_MTQUEUE_H__

#include <unistd.h>
#include <pthread.h>
#include "poller.h"
#include "lqueue.h"

// typename T must be simple data type with 1,2,4,8,16 bytes
template <typename T, typename C>
class ThreadingPipeQueue : PollerObject
{
private:
	typename LinkQueue<T>::allocator alloc;
	LinkQueue<T> queue;
	pthread_mutex_t lock;
	int wakefd;

private:
	// lock management
	inline void Lock(void) { pthread_mutex_lock(&lock); }
	inline void Unlock(void) { pthread_mutex_unlock(&lock); }

	// pipe management
	inline void Wake()
	{
		char c = 0;
		write(wakefd, &c, 1);
	}
	inline void Discard()
	{
		char buf[256];
		int n;
		do
		{
			n = read(netfd, buf, sizeof(buf));
		} while (n > 0);
	}

	// reader implementation
	virtual void hangup_notify(void) {}
	virtual void input_notify(void)
	{
		T p;
		int n = 0;
		Lock();
		while (++n <= 64 && queue.Count() > 0)
		{
			p = queue.Pop();
			Unlock();
			// running task in unlocked mode
			static_cast<C *>(this)->task_notify(p);
			Lock();
		}
		if (queue.Count() <= 0)
			Discard();
		Unlock();
	}

public:
	ThreadingPipeQueue() : queue(&alloc), wakefd(-1)
	{
		pthread_mutex_init(&lock, NULL);
	}
	~ThreadingPipeQueue()
	{
		pthread_mutex_destroy(&lock);
	}
	inline int attach_poller(PollerUnit *thread)
	{
		int fd[2];
		int ret = pipe(fd);
		if (ret != 0)
			return ret;

		wakefd = fd[1];
		netfd = fd[0];
		enable_input();
		return PollerObject::attach_poller(thread);
	}
	inline int Push(T p)
	{
		int qsz;
		int ret;

		Lock();
		qsz = queue.Count();
		ret = queue.Push(p);
		Unlock();
		if (qsz == 0)
			Wake();
		return ret;
	}
	inline int Unshift(T p)
	{
		int qsz;
		int ret;

		Lock();
		qsz = queue.Count();
		ret = queue.Unshift(p);
		Unlock();
		if (qsz == 0)
			Wake();
		return ret;
	}
	inline int Count(void)
	{
		int qsz;

		Lock();
		qsz = queue.Count();
		Unlock();
		return qsz;
	}
	inline int queue_empty(void)
	{
		return Count() == 0;
	}
};

#endif
