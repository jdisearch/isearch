/*
 * =====================================================================================
 *
 *       Filename:  hb_feature.cc
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "hb_feature.h"
#include "global.h"

DTC_USING_NAMESPACE

HBFeature::HBFeature() : _hb_info(NULL), _handle(INVALID_HANDLE)
{
	memset(_errmsg, 0, sizeof(_errmsg));
}

HBFeature::~HBFeature()
{
}

int HBFeature::Init(time_t tMasterUptime)
{
	_handle = M_CALLOC(sizeof(HB_FEATURE_INFO_T));
	if (INVALID_HANDLE == _handle)
	{
		snprintf(_errmsg, sizeof(_errmsg), "init hb_feature fail, %s", M_ERROR());
		return -ENOMEM;
	}

	_hb_info = M_POINTER(HB_FEATURE_INFO_T, _handle);
	_hb_info->master_up_time = tMasterUptime;
	_hb_info->slave_up_time = 0;

	return 0;
}

int HBFeature::Attach(MEM_HANDLE_T handle)
{
	if (INVALID_HANDLE == handle)
	{
		snprintf(_errmsg, sizeof(_errmsg), "attach hb feature failed, memory handle = 0");
		return -1;
	}

	_handle = handle;
	_hb_info = M_POINTER(HB_FEATURE_INFO_T, _handle);

	return 0;
}

void HBFeature::Detach(void)
{
	_hb_info = NULL;
	_handle = INVALID_HANDLE;
}
