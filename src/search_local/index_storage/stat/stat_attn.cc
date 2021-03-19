/*
 * =====================================================================================
 *
 *       Filename:  stat_attn.cc
 *
 *    Description:  
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <vector>
#include <ctype.h>

#include "../attr_api/Attr_API.h"
#include "stat_attn.h"
#include "stat_client.h"
#include "stat_alarm_reporter.h"
#ifndef LONG_MAX
#define LONG_MAX (long)2147483647
#endif

struct StatAttn
{
	int attnId;
	StatClient::iterator info;
	unsigned char hide;
	unsigned char add;
	unsigned char subid;
	unsigned char cat;
};

static std::vector<StatAttn> attnInfo;

int ReloadConfigFile(StatClient &stc, const char *filename)
{
	/* timestamp checking */
	{
		static time_t mtime = 0;
		struct stat st;

		if (stat(filename, &st) != 0)
			st.st_mtime = 0;
		if (st.st_mtime == mtime)
			return 0;
		mtime = st.st_mtime;
	}

	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
		return -1;

	// hide all info
	for (unsigned int i = 0; i < attnInfo.size(); i++)
	{
		if (attnInfo[i].hide)
			attnInfo[i].hide = 2;
		else
			attnInfo[i].hide = 1;
	}

	char buf[128];
	while (fgets(buf, sizeof(buf), fp) != NULL)
	{

		char *p = buf;
		long id = strtol(p, &p, 10);
		if (id == LONG_MAX)
			continue;

		while (isblank(*p))
			p++;

		int add = 0;
		if (p[0] == '+')
		{
			add = 1;
			p++;
			if (*p == '=')
				p++;
		}
		else if (p[0] == '=')
		{
			p++;
		}
		else if (p[0] == ':' && p[1] == '=')
		{
			p += 2;
		}

		long sid = strtol(p, &p, 10);
		if (sid == LONG_MAX)
			continue;

		StatClient::iterator si = stc[(unsigned int)sid];
		if (si == NULL)
			continue;

		int sub = 0;
		if (*p == '.')
		{
			long lsub = strtol(p + 1, &p, 10);
			if (lsub == LONG_MAX)
				continue;
			if (lsub >= 0x10000000)
				continue;
			sub = lsub;
		}
		if (sub < 0)
			continue;
		if (!si->is_sample() && sub > 0)
			continue;
		if (si->is_sample() && (unsigned)sub >= si->count() + 2)
			continue;

		while (isblank(*p))
			p++;
		int cat = SCC_10S;
		if (!strncmp(p, "[cur]", 5))
			cat = SC_CUR;
		if (!strncmp(p, "[10s]", 5))
			cat = SCC_10S;
		if (!strncmp(p, "[10m]", 5))
			cat = SCC_10M;
		if (!strncmp(p, "[all]", 5))
			cat = SCC_ALL;

		unsigned int i;
		for (i = 0; i < attnInfo.size(); i++)
		{
			if (attnInfo[i].attnId == id)
				break;
		}
		if (i == attnInfo.size())
		{
			attnInfo.resize(i + 1);
			attnInfo[i].hide = 2;
		}

		if (attnInfo[i].hide == 2)
			printf("NEW: %d %c %d.%d %d\n", (int)id, add ? '+' : '=', (int)sid, (int)sub, cat);
		attnInfo[i].attnId = id;
		attnInfo[i].info = si;
		attnInfo[i].hide = 0;
		attnInfo[i].subid = sub;
		attnInfo[i].cat = cat;
		attnInfo[i].add = add;
	}
	fclose(fp);
	for (unsigned int i = 0; i < attnInfo.size(); i++)
	{
		if (attnInfo[i].hide == 1)
			printf("DEL: %d %c %d.%d %d\n",
				   (int)attnInfo[i].attnId,
				   attnInfo[i].add ? '+' : '=',
				   (int)attnInfo[i].info->id(),
				   (int)attnInfo[i].subid,
				   attnInfo[i].cat);
	}
	return 0;
}

int ReportData(StatClient &stc)
{
	static int chkpt = 0;

	int c = stc.check_point();
	if (c == chkpt)
		return 0;
	chkpt = c;

	for (unsigned int i = 0; i < attnInfo.size(); i++)
	{
		StatAttn &a = attnInfo[i];
		if (a.hide)
			continue;
		int v;
		if (a.info->is_sample())
		{
			if (a.subid == 0)
				v = stc.read_sample_average(a.info, a.cat);
			else
				v = stc.read_sample_counter(a.info, a.cat, a.subid - 1);
		}
		else
		{
			v = stc.read_counter_value(a.info, a.cat);
		}
		if (a.add)
			Attr_API(a.attnId, v);
		else
			Attr_API_Set(a.attnId, v);
	}
	return 0;
}

void LockReporter(StatClient &stc)
{
	for (int i = 0; i < 60; i++)
	{
		if (stc.Lock("attn") == 0)
			return;
		if (i == 0)
			fprintf(stderr, "Reporter locked, trying 1 minute.\n");
		sleep(1);
	}
	fprintf(stderr, "Can't lock reporter. Exiting.\n");
	exit(-1);
}

static void term(int signo)
{
	exit(0);
}

int run_reporter(StatClient &stc, const char *filename)
{
	int ppid = getppid();
	signal(SIGTERM, term);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	LockReporter(stc);
	for (; getppid() == ppid; sleep(10))
	{

		//ReloadConfigFile(stc, filename);
		//ReportData(stc);
		ALARM_REPORTER->init_alarm_cfg(std::string(filename));
		ALARM_REPORTER->report_alarm();
	}
	return 0;
}
