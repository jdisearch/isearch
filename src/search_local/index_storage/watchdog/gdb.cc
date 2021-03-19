/*
 * =====================================================================================
 *
 *       Filename:  gdb.cc
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "gdb.h"
#include "log.h"

#define printf(fmt, args...) log_bare(2, fmt, ##args)

struct frame_t
{
	int index;
	int ftype; // 0 normal, 1 *this, 2 signal
};

static void wait_prompt(FILE *outfp)
{
	char buf[1024];
	while (fgets(buf, sizeof(buf) - 1, outfp))
	{
		if (!strncmp(buf, "GDB>", 4))
			break;
	}
}

static void dump_result(FILE *outfp)
{
	char buf[1024];
	while (fgets(buf, sizeof(buf) - 1, outfp))
	{
		if (!strncmp(buf, "GDB>", 4))
			break;
		printf("%s", buf);
	}
}

static void parse_frame(FILE *outfp, std::vector<frame_t> &frame)
{
	char buf[1024];
	int first = 1;
	while (fgets(buf, sizeof(buf) - 1, outfp))
	{
		if (!strncmp(buf, "GDB>", 4))
			break;
		printf("%s", buf);
		if (buf[0] == '#')
		{
			struct frame_t f;
			char *p = strstr(buf, "this");
			f.index = atoi(buf + 1);
			f.ftype = p && (!isalnum(p[-1] && !isalnum(p[4])));
			if (first && strstr(buf, "signal handler called") != NULL)
			{
				first = 0;
				frame.clear();
				f.ftype = 2;
			}
			frame.push_back(f);
		}
	}
}

static void dump_info(int pid)
{
	char fn[64];
	char buf[1024];
	int rv;

	printf("Process/Thread %d Crashed\n", pid);
	snprintf(fn, sizeof(fn), "/proc/%d/exe", pid);
	rv = readlink(fn, buf, sizeof(buf) - 1);
	if (rv > 0)
	{
		buf[rv] = 0;
		printf("Executable: %s\n", buf);
	}

	snprintf(fn, sizeof(fn), "/proc/%d/cwd", pid);
	rv = readlink(fn, buf, sizeof(buf) - 1);
	if (rv > 0)
	{
		buf[rv] = 0;
		printf("Working Directory: %s\n", buf);
	}

	snprintf(fn, sizeof(fn), "/proc/%d/cmdline", pid);
	FILE *fp = fopen(fn, "r");
	if (fp)
	{
		if (fgets(buf, sizeof(buf), fp))
			printf("Command Line: %s\n", buf);
		fclose(fp);
	}

	snprintf(fn, sizeof(fn), "/proc/%d/maps", pid);
	fp = fopen(fn, "r");
	if (fp)
	{
		printf("Dump memory maps:\n");
		while (fgets(buf, sizeof(buf), fp))
		{
			printf("    %s", buf);
		}
		fclose(fp);
	}
}

void gdb_dump(int pid)
{
	std::vector<frame_t> frame;
	int pcmd[2];
	int pout[2];
	int ret;

	ret = pipe(pcmd);
	ret = pipe(pout);

	if (fork() == 0)
	{
		dup2(pcmd[0], 0);
		dup2(pout[1], 1);
		dup2(pout[1], 2);
		close(pcmd[0]);
		close(pcmd[1]);
		close(pout[0]);
		close(pout[1]);
		execlp("gdb", "gdb", NULL);
		exit(1);
	}

	close(pcmd[0]);
	close(pout[1]);

	//always succ because the fd is valid
	FILE *cmdfp = fdopen(pcmd[1], "w");
	FILE *outfp = fdopen(pout[0], "r");

	setbuf(cmdfp, NULL);
	setbuf(outfp, NULL);

	fprintf(cmdfp, "set prompt GDB>\\n\n\n");
	fflush(cmdfp);
	wait_prompt(outfp);
	fprintf(cmdfp, "attach %d\n", pid);
	dump_info(pid);
	dump_result(outfp);
	printf("(gdb) backtrace\n");
	fprintf(cmdfp, "backtrace\n");
	parse_frame(outfp, frame);

#define DUMP(fmt, args...)                 \
	do                                     \
	{                                      \
		printf("(gdb) " fmt "\n", ##args); \
		fprintf(cmdfp, fmt "\n", ##args);  \
		dump_result(outfp);                \
	} while (0)

	for (unsigned int i = 0; i < frame.size(); i++)
	{
		DUMP("frame %d", frame[i].index);
		if (frame[i].ftype <= 1)
			DUMP("info locals");
		if (frame[i].ftype == 1)
			DUMP("print *this");
		if (frame[i].ftype == 2)
			DUMP("info registers");
	}

	fprintf(cmdfp, "set variable crash_continue = 1\n");
	dump_result(outfp);
	fprintf(cmdfp, "quit\n");
	fclose(cmdfp);
	fclose(outfp);
}

void gdb_attach(int pid, const char *fn)
{
	int ret;
	if (fn == NULL)
		fn = getenv("DISPLAY");
	char buf[256];
	if (fn == NULL || !fn[0] || !strcmp(fn, "screen"))
	{
		snprintf(buf, sizeof(buf), "screen -X -S gdb screen -t gdb.%d gdb /proc/%d/exe %d", pid, pid, pid);
		ret = system(buf);
		if (ret != 0)
		{
			snprintf(buf, sizeof(buf), "screen -S gdb -t gdb.%d -d -m gdb /proc/%d/exe %d", pid, pid, pid);
			ret = system(buf);
		}
	}
	else
	{
		snprintf(buf, sizeof(buf), "xterm -T gdb.%d -e gdb /proc/%d/exe %d &", pid, pid, pid);
		ret = system(buf);
	}
	dump_info(pid);
}
