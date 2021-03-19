/*
 * =====================================================================================
 *
 *       Filename:  pipequeue.h
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
#include <unistd.h>
#include "mem_check.h"
#include "poll_thread.h"

// typename T must be simple data type with 1,2,4,8,16 bytes
template <typename T, typename C>
class PipeQueueReader : PollerObject
{
private:
	T buf[32];
	int i; // first pending
	int n; // last pending

private:
	virtual void hangup_notify(void) {}
	virtual void input_notify(void)
	{
		T p;
		int n = 0;
		while (++n <= 64 && (p = Pop()) != NULL)
			static_cast<C *>(this)->task_notify(p);
	}
	inline T Pop(void)
	{
		if (i >= n)
		{
			i = 0;
			n = read(netfd, buf, sizeof(buf));
			if (n <= 0)
				return NULL;
			n /= sizeof(T);
		}
		return buf[i++];
	}

public:
	inline PipeQueueReader() : i(0), n(0) {}
	virtual ~PipeQueueReader() {}
	inline int attach_poller(PollerUnit *thread, int fd)
	{
		netfd = fd;
		enable_input();
		return PollerObject::attach_poller(thread);
	}
};

template <class T>
class PipeQueueWriter : PollerObject, TimerObject
{
private:
	PollThread *owner;
	T *buf;
	int i; // first unsent
	int n; // last unsent
	int m; // buf size

private:
	virtual void hangup_notify(void) {}
	virtual void output_notify(void)
	{
		if (n > 0)
		{
			// i always less than n
			int r = write(netfd, buf + i, (n - i) * sizeof(T));
			if (r > 0)
			{
				i += r / sizeof(T);
				if (i >= n)
					i = n = 0;
			}
		}
		if (n == 0)
		{
			disable_output();
			disable_timer();
		}
		else
			enable_output();
		delay_apply_events();
	}
	virtual void timer_notify(void)
	{
		output_notify();
	}

public:
	inline PipeQueueWriter() : i(0), n(0), m(16) { buf = (T *)MALLOC(m * sizeof(T)); }
	virtual ~PipeQueueWriter() { FREE_IF(buf); }
	inline int attach_poller(PollThread *thread, int fd)
	{
		owner = thread;
		netfd = fd;
		return PollerObject::attach_poller(thread);
	}
	inline int Push(T p)
	{
		if (n >= m)
		{
			if (i > 0)
			{
				memmove(buf, buf + i, (n - i) * sizeof(T));
				n -= i;
			}
			else
			{
				T *newbuf = (T *)REALLOC(buf, 2 * m * sizeof(T));
				if (newbuf == NULL)
					return -1;
				buf = newbuf;
				m *= 2;
			}
			i = 0;
		}
		buf[n++] = p;

		// force flush every 16 pending task
		if (n - i >= 16 && n % 16 == 0)
			output_notify();
		// some task pending, trigger ready timer
		if (n > 0)
			attach_ready_timer(owner);
		return 0;
	}
};

template <typename T, typename C>
class PipeQueue : public PipeQueueReader<T, C>
{
private:
	PipeQueueWriter<T> writer;

public:
	inline PipeQueue() {}
	inline ~PipeQueue() {}
	inline int attach_poller(PollThread *fr, PollThread *to, int fd[2])
	{
		PipeQueueReader<T, C>::attach_poller(to, fd[0]);
		writer.attach_poller(fr, fd[1]);
		return 0;
	}
	inline int attach_poller(PollThread *fr, PollThread *to)
	{
		int fd[2];
		int ret = pipe(fd);
		if (ret != 0)
			return ret;

		PipeQueueReader<T, C>::attach_poller(to, fd[0]);
		writer.attach_poller(fr, fd[1]);
		return 0;
	}
	inline int Push(T p) { return writer.Push(p); }
};
