/*
 * =====================================================================================
 *
 *       Filename:  proc_title.h
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
#ifndef __DTC_PROCTITLE_H
#define __DTC_PROCTITLE_H

extern "C"
{
	extern void init_proc_title(int, char **);
	extern void set_proc_title(const char *title);
	extern void set_proc_name(const char *);
	extern void get_proc_name(char *);
}

#endif
