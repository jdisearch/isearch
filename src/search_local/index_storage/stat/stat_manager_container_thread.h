/*
 * =====================================================================================
 *
 *       Filename:  StatManagerContainerThread.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  27/05/2014 08:50:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming (prudence), linjinming@jd.com
 *        Company:  JD, China
 *
 * =====================================================================================
 */
#ifndef __H_STAT_MANAGER_CONTAINER_THREAD___
#define __H_STAT_MANAGER_CONTAINER_THREAD___

#include "stat_manager.h"
#include "lock.h"

class StatManagerContainerThread : protected StatLock
{
public: // public method
	StatManagerContainerThread(void);
	~StatManagerContainerThread(void);
	static StatManagerContainerThread *getInstance();

public: // background access
	int start_background_thread(void);
	int stop_background_thread(void);
	int add_stat_manager(uint32_t moudleId, StatManager *stat);
	int delete_stat_manager(uint32_t moudleId);
	StatItemU32 operator()(int moduleId, int cId);

	//int init_stat_info(const char *name, const char *fn) { return StatManager::init_stat_info(name, fn, 0); }
private:
	pthread_t threadid;
	;
	pthread_mutex_t wakeLock;
	;
	static void *__threadentry(void *);
	void thread_loop(void);
	std::map<uint32_t, StatManager *> m_statManagers;
	Mutex mLock;
};

#endif
