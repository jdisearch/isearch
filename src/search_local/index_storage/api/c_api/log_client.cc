/*
 * =====================================================================================
 *
 *       Filename:  log_client.cc
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
#include "dtc_api.h"
#include <log.cc>

__EXPORT
void DistributedTableCache::write_log(int level,
									  const char *file, const char *func, int lineno,
									  const char *fmt, ...)
	__attribute__((format(printf, 5, 6)))
	__attribute__((__alias__("_write_log_")));
