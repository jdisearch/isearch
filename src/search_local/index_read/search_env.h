/*
 * =====================================================================================
 *
 *       Filename:  search_env.h
 *
 *    Description:  search env class definition.
 *
 *        Version:  1.0
 *        Created:  16/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SEARCH_ENV_H__
#define __SEARCH_ENV_H__
#include "singleton.h"

class SearchEnv {
public:
	SearchEnv() {
		NC_ENV_FDS = "NC_ENV_FDS";
	}
	static SearchEnv *Instance() {
		return CSingleton<SearchEnv>::Instance();
	}
	static void Destroy() {
		CSingleton<SearchEnv>::Destroy();
	}
	int WriteSocketEnv(int fd);
	int ReadSocketEnv(struct sockaddr *addr, int addrlen);
	int CleanSocketEnv();
	char *UnresolveDesc(int sd);
	char *UnresolveAddr(struct sockaddr *addr, int addrlen);
	char *GetEnv() {
		return os_environ;
	}
private:
	int cpa_atoi(char *line, size_t n);
	char os_environ[128];
	const char *NC_ENV_FDS;
};
#endif
