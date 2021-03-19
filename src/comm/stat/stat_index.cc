#include "stat_index.h"

#include <unistd.h>
#include <stdlib.h>
#include "version.h"
#include "log.h"

CStatThread statmgr;
extern CStatItemU32 *__statLogCount;
extern unsigned int __localStatLogCnt[8];

void _init_log_stat_(void)
{
	__statLogCount = new CStatItemU32[8];
	for(unsigned i=0; i<8; i++)
	{
		__statLogCount[i] = statmgr.GetItemU32(LOG_COUNT_0+i);
		__statLogCount[i].set(__localStatLogCnt[i]);
	}
}

// prevent memory leak if module unloaded
// unused-yet because this file never link into module
__attribute__((__constructor__))
static void clean_logstat(void) {
	if(__statLogCount != NULL) {
		delete __statLogCount;
		__statLogCount = NULL;
	}
}

int InitStat(const char *name)
{
	int ret;
	ret = statmgr.InitStatInfo(name, STATIDX);
	// -1, recreate, -2, failed
	if(ret == -1)
	{
		unlink(STATIDX);
		char buf[64];
		ret = statmgr.CreateStatIndex(name, STATIDX, StatDefinition, buf, sizeof(buf));
		if(ret != 0)
		{
			log_crit("CreateStatIndex failed: %s", statmgr.ErrorMessage());
			exit(ret);
		}
		ret = statmgr.InitStatInfo(name, STATIDX);
	}
	if(ret==0)
	{
		int v1, v2, v3;
		sscanf(TTC_VERSION, "%d.%d.%d", &v1, &v2, &v3);
		statmgr.GetItem(S_VERSION) = v1*10000 + v2*100 + v3;
		statmgr.GetItem(C_TIME) = compile_time;
		_init_log_stat_();
	} else {
		log_crit("InitStatInfo failed: %s", statmgr.ErrorMessage());
		exit(ret);
	}
	return ret;
}

