/*
 * =====================================================================================
 *
 *       Filename:  system_lock.h
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
#ifndef __SYSTEM_LOCK_H
#define __SYSTEM_LOCK_H

#include <sys/cdefs.h>

__BEGIN_DECLS
int unix_socket_lock(const char *fmt, ...);
__END_DECLS

#endif
