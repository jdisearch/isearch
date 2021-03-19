/*
 * =====================================================================================
 *
 *       Filename:  version.h
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
#ifndef __DTC_VERSION_H
#define __DTC_VERSION_H

#define DTC_MAJOR_VERSION 4
#define DTC_MINOR_VERSION 7
#define DTC_BETA_VERSION 1

/*major.minor.beta*/
#define DTC_VERSION "4.7.1"
/* the following show line should be line 11 as line number is used in Makefile */
#define DTC_GIT_VERSION "7b21244"
#define DTC_VERSION_DETAIL DTC_VERSION "." DTC_GIT_VERSION

extern const char compdatestr[];
extern const char comptimestr[];
extern const char version[];
extern const char version_detail[];
extern long compile_time;

#endif
