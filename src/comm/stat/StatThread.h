#ifndef __H_STAT_THREAD___
#define __H_STAT_THREAD___

#include "StatMgr.h"

class CStatThread: public CStatManager
{
public: // public method
	CStatThread(void);
	~CStatThread(void);

public: // background access
	int StartBackgroundThread(void);
	int StopBackgroundThread(void);
	int InitStatInfo(const char *name, const char *fn) { return CStatManager::InitStatInfo(name, fn, 0); }
private:
	pthread_t threadid;;
	pthread_mutex_t wakeLock;;
	static void *__threadentry(void *);
	void ThreadLoop(void);
};

#endif
