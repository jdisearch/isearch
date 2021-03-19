/*
 * =====================================================================================
 *
 *       Filename:  stat_client.h
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
#ifndef __STAT_MGR_H
#define __STAT_MGR_H

#include "stat_manager.h"

class StatClient : public StatManager
{
public:
	int init_stat_info(const char *name, const char *fn);

	typedef StatInfo *iterator;
	int check_point(void);
	inline iterator begin(void) { return info; }
	inline iterator end(void) { return info + numinfo; }

	int64_t read_counter_value(iterator, unsigned int cat);
	int64_t read_sample_counter(iterator, unsigned int cat, unsigned int idx = 0);
	int64_t read_sample_average(iterator, unsigned int cat);
	iterator operator[](unsigned int id) { return idmap[id]; }

public: // client/tools access
	StatClient();
	~StatClient();

private:
	int lastSN;
};

#endif
