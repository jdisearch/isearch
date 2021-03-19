/*
 * =====================================================================================
 *
 *       Filename:  thread.h
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
#ifndef __GENERICTHREAD_H__
#define __GENERICTHREAD_H__

#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/unistd.h>

#include "timestamp.h"
#include "poller.h"
#include "timer_list.h"
#include "config.h"

class Thread
{
public:
	enum
	{
		ThreadTypeWorld = -1,
		ThreadTypeProcess = 0,
		ThreadTypeAsync = 1,
		ThreadTypeSync = 2
	};

public:
	static AutoConfig *g_autoconf;
	static void set_auto_config_instance(AutoConfig *ac) { g_autoconf = ac; };

public:
	Thread(const char *name, int type = 0);
	virtual ~Thread();

	int initialize_thread();
	void running_thread();
	const char *Name(void) { return taskname; }
	int Pid(void) const { return stopped ? 0 : pid; }
	void set_stack_size(int);
	int Stopping(void) { return *stopPtr; }
	virtual void interrupt(void);
	virtual void crash_hook(int);

	Thread *the_world(void) { return &TheWorldThread; };

protected:
	char *taskname;
	pthread_t tid;
	pthread_mutex_t runlock;
	int stopped;
	volatile int *stopPtr;
	int tasktype; // 0-->main, 1--> epoll, 2-->sync, -1, world
	int stacksize;
	uint64_t cpumask;
	int pid;

protected:
	virtual int Initialize(void);
	virtual void Prepare(void);
	virtual void *Process(void);
	virtual void Cleanup(void);

protected:
	struct cmp
	{
		bool operator()(const char *const &a, const char *const &b) const
		{
			return strcmp(a, b) < 0;
		}
	};
	typedef std::map<const char *, Thread *, cmp> thread_map_t;
	static thread_map_t _thread_map;
	static pthread_mutex_t _thread_map_lock;

	static void LOCK_THREAD_MAP() { pthread_mutex_lock(&_thread_map_lock); }
	static void UNLOCK_THREAD_MAP() { pthread_mutex_unlock(&_thread_map_lock); }

public:
	static Thread *FindThreadByName(const char *name);

private:
	static Thread TheWorldThread;
	static void *Entry(void *thread);
	void prepare_internal(void);
	void auto_config(void);
	void auto_config_stack_size(void);
	void auto_config_cpu_mask(void);
};

#endif
