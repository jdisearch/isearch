/*
 * =====================================================================================
 *
 *       Filename:  log.h
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
#ifndef __DTCLOG_H__
#define __DTCLOG_H__

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <stdio.h>

extern int __log_level__;
extern int __business_id__;
#define log_bare(lvl, fmt, args...) _write_log_(lvl, NULL, NULL, 0, fmt, ##args)
#define log_generic(lvl, fmt, args...) _write_log_(lvl, __FILE__, __FUNCTION__, __LINE__, fmt, ##args)
#define log_emerg(fmt, args...) log_generic(0, fmt, ##args)
#define log_alert(fmt, args...) log_generic(1, fmt, ##args)
#define log_crit(fmt, args...) log_generic(2, fmt, ##args)
#define log_error(fmt, args...) log_generic(3, fmt, ##args)
#define log_warning(fmt, args...)        \
    do                                   \
    {                                    \
        if (__log_level__ >= 4)          \
            log_generic(4, fmt, ##args); \
    } while (0)
#define log_notice(fmt, args...)         \
    do                                   \
    {                                    \
        if (__log_level__ >= 5)          \
            log_generic(5, fmt, ##args); \
    } while (0)
#define log_info(fmt, args...)           \
    do                                   \
    {                                    \
        if (__log_level__ >= 6)          \
            log_generic(6, fmt, ##args); \
    } while (0)
#define log_debug(fmt, args...)          \
    do                                   \
    {                                    \
        if (__log_level__ >= 7)          \
            log_generic(7, fmt, ##args); \
    } while (0)

#define error_log(fmt, args...) log_error(fmt, ##args)

#if __cplusplus
extern void _init_log_(const char *app, const char *dir = NULL);
#else
extern void _init_log_(const char *app, const char *dir);
#endif
extern void _init_log_alerter_(void);
extern void _init_log_stat_(void);
extern void _set_log_level_(int);
extern void _set_log_alert_hook_(int (*alert_func)(const char *, int));
extern void _set_log_thread_name_(const char *n);
extern void _write_log_(int, const char *, const char *, int, const char *, ...) __attribute__((format(printf, 5, 6)));
extern int _set_remote_log_fd_();
extern void remote_log(int type, const char *key, int op_type, int op_result, char *content, long op_time, int cmd, int magic, int contentlen);
extern void _set_business_id_(int);

#include <asm/unistd.h>
#include <unistd.h>
#ifndef __NR_gettid
#endif
static inline int _gettid_(void)
{
    return syscall(__NR_gettid);
}

#include <sys/time.h>
static inline unsigned int GET_MSEC(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#define INIT_MSEC(v) v = GET_MSEC()
#define CALC_MSEC(v) v = GET_MSEC() - (v)
static inline unsigned int GET_USEC(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
#define INIT_USEC(v) v = GET_USEC()
#define CALC_USEC(v) v = GET_USEC() - (v)

__END_DECLS
#endif
