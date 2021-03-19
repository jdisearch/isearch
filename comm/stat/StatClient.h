#ifndef __STAT_MGR_H
#define __STAT_MGR_H

#include "StatMgr.h"

class CStatClient: public CStatManager
{
public:
	int InitStatInfo(const char *name, const char *fn);

	typedef CStatInfo *iterator;
	int CheckPoint(void);
	inline iterator begin(void) { return info; }
	inline iterator end(void) { return info+numinfo; }

	int64_t ReadCounterValue(iterator, unsigned int cat);
	int64_t ReadSampleCounter(iterator, unsigned int cat, unsigned int idx=0);
	int64_t ReadSampleAverage(iterator, unsigned int cat);
	iterator operator[](unsigned int id) { return idmap[id]; }

public:	// client/tools access
	CStatClient();
	~CStatClient();

private:
	int lastSN;
};

#endif
