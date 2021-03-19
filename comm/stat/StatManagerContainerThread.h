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

#include "StatMgr.h"
#include "lock.h"

class CStatManagerContainerThread: protected CStatLock
{
public: // public method
	CStatManagerContainerThread(void);
	~CStatManagerContainerThread(void);
	static CStatManagerContainerThread *getInstance();

public: // background access
	int StartBackgroundThread(void);
	int StopBackgroundThread(void);
	int AddStatManager(uint32_t moudleId, CStatManager *stat);
	int DeleteStatManager(uint32_t moudleId);
	CStatItemU32 operator()(int moduleId,int cId);
	
	//int InitStatInfo(const char *name, const char *fn) { return CStatManager::InitStatInfo(name, fn, 0); }
private:
	pthread_t threadid;;
	pthread_mutex_t wakeLock;;
	static void *__threadentry(void *);
	void ThreadLoop(void);
	std::map<uint32_t, CStatManager *> 	m_statManagers;
	CMutex mLock;

};

#endif
