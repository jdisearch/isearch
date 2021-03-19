/*
 * =====================================================================================
 *
 *       Filename:  gdb.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <vector>

void gdb_dump(int);
void gdb_attach(int, const char *);
void gdb_server(int, const char *);
