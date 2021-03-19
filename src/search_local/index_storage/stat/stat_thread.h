/*
 * =====================================================================================
 *
 *       Filename:  stat_thread.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __H_STAT_THREAD___
#define __H_STAT_THREAD___

#include "stat_manager.h"

class StatThread : public StatManager
{
public: // public method
	StatThread(void);
	~StatThread(void);

public: // background access
	int start_background_thread(void);
	int stop_background_thread(void);
	int init_stat_info(const char *name, const char *fn) { return StatManager::init_stat_info(name, fn, 0); }

private:
	pthread_t threadid;
	;
	pthread_mutex_t wakeLock;
	;
	static void *__threadentry(void *);
	void thread_loop(void);
};

#endif
