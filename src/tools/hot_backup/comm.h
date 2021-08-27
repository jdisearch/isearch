/*
 * =====================================================================================
 *
 *       Filename:  comm.h
 *
 *    Description:  comm class definition.
 *
 *        Version:  1.0
 *        Created:  04/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __HB_COMM_H
#define __HB_COMM_H

#include <dtcapi.h>
#include <config.h>
#include "global.h"
#include "registor.h"

class ParserBase;

class CComm {
public:
	static void parse_argv(int argc, char **argv);
	static void show_usage(int argc, char **argv);
	static void show_version(int argc, char **argv);
	static int load_config(const char *p = SYS_CONFIG_FILE);
	static int check_hb_status();
	static int fixed_hb_env();
	static int fixed_slave_env();
	static int connect_ttc_server(int ping_master, ParserBase* pParser);
	static int uniq_lock(const char *p = ASYNC_FILE_PATH);
	static int log_server_info( ParserBase* pParser , const char *p=ASYNC_FILE_PATH);
	static int ReInitDtcAgency(ParserBase* pParser);

public:

	static DTC::Server master;
	static DTC::Server slave;
	static CConfig config;
	static CRegistor registor;

	static const char *version;
	static char *config_file;
	static int backend;
	static int fixed;
	static int purge;
	static int normal;
	static int skipfull;
};

#endif
