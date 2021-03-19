/*
 * =====================================================================================
 *
 *       Filename:  daemon.h
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
#ifndef __DTC_DAEMON__H__
#define __DTC_DAEMON__H__

#define MAXLISTENERS 10

class DTCConfig;
struct DbConfig;
class DTCTableDefinition;

extern DTCConfig *gConfig;
extern DbConfig *dbConfig;
extern DTCTableDefinition *gTableDef[];

extern volatile int stop;
extern int background;

extern const char progname[];
extern const char version[];
extern const char usage_argv[];
extern char cacheFile[256];
extern char tableFile[256];
extern void show_version(void);
extern void show_usage(void);
extern int dtc_daemon_init(int argc, char **argv);
extern int bmp_daemon_init(int argc, char **argv);
extern int daemon_start(void);
extern void daemon_wait(void);
extern void daemon_cleanup(void);
extern int daemon_get_fd_limit(void);
extern int daemon_set_fd_limit(int maxfd);
extern int daemon_enable_core_dump(void);
extern unsigned int scan_process_openning_fd(void);
#endif
