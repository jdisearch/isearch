/*
 * =====================================================================================
 *
 *       Filename:  hb_feature.h
 *
 *    Description:  hotbackup method release.
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
#ifndef __DTC_HB_FEATURE_H
#define __DTC_HB_FEATURE_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "namespace.h"
#include "singleton.h"
#include "global.h"

struct hb_feature_info
{
	int64_t	master_up_time;
	int64_t slave_up_time;
};
typedef struct hb_feature_info HB_FEATURE_INFO_T;

class HBFeature
{
public:
	HBFeature();
	~HBFeature();
		
	static HBFeature* Instance(){return Singleton<HBFeature>::Instance();}
	static void Destroy() { Singleton<HBFeature>::Destroy();}
		
	int Init(time_t tMasterUptime);
	int Attach(MEM_HANDLE_T handle);
	void Detach(void);
	
	const char *Error() const {return _errmsg;}
	
	MEM_HANDLE_T Handle() const { return _handle; }
	
	int64_t& master_uptime() { return _hb_info->master_up_time; }
	int64_t& slave_uptime() { return _hb_info->slave_up_time; }
	
private:
	HB_FEATURE_INFO_T* _hb_info;	
	MEM_HANDLE_T _handle;
	char _errmsg[256];
};

#endif

