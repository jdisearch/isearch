/*
 * =====================================================================================
 *
 *       Filename:  thread_cpu_stat.h
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
#ifndef THREAD_CPU_STAT_H
#define THREAD_CPU_STAT_H

#include "log.h"
#include "poll_thread.h"
#include "stat_dtc.h"
#include <string>
#define THREAD_CPU_STAT_MAX_ERR 6

#define CPU_STAT_INVALID 0
#define CPU_STAT_NOT_INIT 1
#define CPU_STAT_INIT 2
#define CPU_STAT_FINE 3
#define CPU_STAT_ILL 4

typedef struct one_thread_cpu_stat
{
	int _fd;  //proc fd thread hold
	int _pid; //thread pid
	const char *_thread_name;
	double _last_time;
	double _this_time;
	unsigned long long _last_cpu_time; //previous cpu time of this thread
	unsigned long long _this_cpu_time; //this cpu time of this thread
	StatItemU32 _pcpu;				   //cpu percentage
	unsigned int _err_times;		   //max error calculate time
	unsigned int _status;			   //status
	struct one_thread_cpu_stat *_next;

public:
	int init(const char *thread_name, int thread_idx, int pid);
	int read_cpu_time(void);
	void cal_pcpu(void);
	void do_stat();
} one_thread_cpu_stat_t;

class thread_cpu_stat
{
private:
	one_thread_cpu_stat_t *_stat_list_head;

public:
	thread_cpu_stat()
	{
		_stat_list_head = NULL;
	}
	~thread_cpu_stat()
	{
	}
	int init();
	void do_stat();

private:
	int add_cpu_stat_object(const char *thread_name, int thread_idx);
};

#endif
