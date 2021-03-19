/*
 * =====================================================================================
 *
 *       Filename:  black_hole.cc
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
#include <black_hole.h>

BlackHole::~BlackHole(void)
{
}

void BlackHole::task_notify(TaskRequest *cur)
{
#if 0
	switch(cur->request_code()){
		case DRequest::Get:
			break;
			
		case DRequest::Insert: // TableDef->has_auto_increment() must be false
			cur->resultInfo.set_affected_rows(1);
			break;
		
		case DRequest::Update:
		case DRequest::Delete:
		case DRequest::Purge:
		case DRequest::Replace:
		default:
			cur->resultInfo.set_affected_rows(1);
			break;
	}

#else
	// preset affected_rows==0 is obsoleted
	// use BlackHole flag instead
	cur->mark_as_black_hole();
#endif
	cur->reply_notify();
}
