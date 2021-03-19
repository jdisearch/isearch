#ifndef _STAT_INDEX_H__
#define _STAT_INDEX_H__

#include "StatThread.h"
#include "StatTTCDef.h"

#define STATIDX "../stat/index.stat.idx"

extern CStatThread statmgr;
extern int InitStat(const char *name);

#endif
