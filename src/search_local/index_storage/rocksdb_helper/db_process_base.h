/*
 * =====================================================================================
 *
 *       Filename: db_process_base.h 
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

#ifndef __HELPER_PROCESS_BASE_H__
#define __HELPER_PROCESS_BASE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <task_base.h>
#include <dbconfig.h>
#include <buffer.h>
#include "helper_log_api.h"

class DirectRequestContext;
class DirectResponseContext;

class HelperProcessBase
{
public:
	HelperLogApi logapi;

public:
	virtual ~HelperProcessBase() {}

	virtual void use_matched_rows(void) = 0;
	virtual int Init(int GroupID, const DbConfig *Config, DTCTableDefinition *tdef, int slave) = 0;
	virtual void init_ping_timeout(void) = 0;
	virtual int check_table() = 0;

	virtual int process_task(DTCTask *Task) = 0;

	virtual void init_title(int m, int t) = 0;
	virtual void set_title(const char *status) = 0;
	virtual const char *Name(void) = 0;
	virtual void set_proc_timeout(unsigned int Seconds) = 0;

	virtual int process_direct_query(
		DirectRequestContext *reqCxt,
		DirectResponseContext *respCxt)
	{
		return -1;
	}
};

#endif
