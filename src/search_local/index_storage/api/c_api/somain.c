/*
 * =====================================================================================
 *
 *       Filename:  somain.c
 *
 *    Description:  entry function.
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
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <version.h>
#include <compiler.h>

const char __invoke_dynamic_linker__[]
	__attribute__((section(".interp")))
	__HIDDEN =
#if __x86_64__
		"/lib64/ld-linux-x86-64.so.2"
#else
		"/lib/ld-linux.so.2"
#endif
	;

__HIDDEN
void _so_start(char *arg1, ...)
{
#define BANNER "DTC client API v" DTC_VERSION_DETAIL "\n"                \
			   "  - TCP connection supported\n"                          \
			   "  - UDP connection supported\n"                          \
			   "  - UNIX stream connection supported\n"                  \
			   "  - embeded threading connection supported\n"            \
			   "  - protocol packet encode/decode interface supported\n" \
			   "  - async execute (except embeded threading) supported \n"

	int unused;
	unused = write(1, BANNER, sizeof(BANNER) - 1);
	_exit(0);
}
