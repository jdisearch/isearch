/*
 * =====================================================================================
 *
 *       Filename:  timestamp.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __DTC_TIMESTAMP_H__
#define __DTC_TIMESTAMP_H__

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

static inline int64_t GET_TIMESTAMP(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

#define TIMESTAMP_PRECISION 1000000ULL

#endif
