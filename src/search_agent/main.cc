
#include "log.h"
#include <stdlib.h>
#include <string>
#include <vector>
#include <getopt.h>
#include <stdlib.h>
#include "sa_core.h"
#include <signal.h>
#include <execinfo.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_BUILD 0

#define STRING_HELPER(str)	#str
#define STRING(x)		STRING_HELPER(x)



#define MAIN_VERSION      \
	STRING(VERSION_MAJOR) \
	"." STRING(VERSION_MINOR) "." STRING(VERSION_BUILD)

#ifndef INDEX_GIT_VERSION
#define INDEX_GIT_VERSION 0000000
#endif
#define SEARCH_VERSION_STR MAIN_VERSION "-" STRING(INDEX_GIT_VERSION)

#define SA_LOG_DEFAULT 3
#define SA_CONF_PATH "../conf/sa.conf"
#define SA_LOG_DIR "../log"
#define SA_EVENT_INTERVAL 2000

static int show_version;
static int show_help;
static int daemonize;

static int sa_stop;
static int sa_reload;

const char compdatestr[] = __DATE__;
const char comptimestr[] = __TIME__;

struct context
{
	uint32_t id; //uniq context id
	struct conf *cf;
	struct event_base *evb;
};

static struct option long_options[] =
	{
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"test-conf", no_argument, NULL, 't'},
		{"daemonize", no_argument, NULL, 'd'},
		{"verbose", required_argument, NULL, 'v'},
		{"conf-file", required_argument, NULL, 'c'},
		{"pid-file", required_argument, NULL, 'p'},
		{"cpu-affinity", required_argument, NULL, 'a'},
		{NULL, 0, NULL, 0}

};

static char short_options[] = "hVtdDv:o:c:p:m:a:";



static void set_default_options(struct instance *dai)
{
	dai->log_level = SA_LOG_DEFAULT;
	dai->conf_filename = (char *)SA_CONF_PATH;
	dai->log_dir = (char *)SA_LOG_DIR;
	dai->event_max_timeout = SA_EVENT_INTERVAL;
	int status = gethostname(dai->hostname, SA_MAXHOSTNAMELEN);
	if (status < 0)
	{
		snprintf(dai->hostname, SA_MAXHOSTNAMELEN, "unknown");
	}
	dai->hostname[SA_MAXHOSTNAMELEN - 1] = '\0';
	dai->pid = (pid_t)-1;
	dai->pid_filename = NULL;
	dai->pidfile = 0;
	dai->argv = NULL;
	dai->cpumask = -1;
}

static int get_options(int argc, char **argv, struct instance *dai)
{
	int c, value;

	for (;;)
	{
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
		{
			break;
		}
		switch (c)
		{
		case 'h':
			show_help = 1;
			show_version = 1;
			break;
		case 'V':
			show_version = 1;
			break;
		case 'd':
			daemonize = 1;
			break;
		case 'v':
			value = atoi(optarg);
			if (value > 0)
			{
				dai->log_level = value;
			}
			break;
		case 'o':
			dai->log_dir = optarg;
			break;
		case 'c':
			dai->conf_filename = optarg;
			break;
		case 'e':
			value = atoi(optarg);
			if (value > 0)
			{
				dai->event_max_timeout = value;
			}
			break;
		case 'p':
			dai->pid_filename = optarg;
			break;
		case 'a':
			value = atoi(optarg);
			if (value > 0)
			{
				dai->cpumask = value;
			}
			break;
		case '?':
			switch (optopt)
			{
			case 'o':
			case 'c':
			case 'p':
				printf("search_agent: option -%c requires a file name", optopt);
				break;
			case 'v':
				printf("search_agent: option -%c requires a number", optopt);
				break;
			case 'a':
				printf("search_agent: option -%c requires a string", optopt);
				break;
			default:
				printf("search_agent: invalid option -%c", optopt);
				break;
			}
			return -1;
		default:
			return -1;
		}
	}
	return 0;
}

void show_usage()
{
	printf(
		"Usage: search [-?hVdDt] [-v verbosity level] [-o output file]\r\n"
		" [-c conf file] [-p pid file] [-m mbuf size] [-a cpu affinity]\r\n"
		"");
	printf(
		"Options:\r\n"
		" -h, --help : this help\r\n"
		" -V, --version : show version and exit\r\n"
		" -t, --test-conf : test configuration for syntax errors and exit\r\n"
		" -d, --daemonize : run as a daemon\r\n"
		" -D, --describe-stats : print stats description and exit\r\n");
	printf(
		" -v, --verbosity=N : set logging level (default: 3, min: 1, max: 7)\r\n"
		" -o, --output=S : set logging dir (default: ../log/)\r\n"
		" -c, --conf-file=S : set configuration file (default: ../conf/agent.xml)\r\n"
		" -e, --event-max-timeout=S : set epoll max timeout(ms)(default: (30*1000)ms)\r\n"
		" -i, --stats_interval=S : set stats aggregator interval(ms)(default: (10*1000)ms)\r\n"
		" -p, --pid-file=S : set pid file (default: off)\r\n"
		" -a, --bind-cpu-mask=S : set processor bind cpu(default: no bind)\r\n"
		"");
}

void stop(int signo)
{
	sa_stop = 1;
}

void reload(int signo)
{
	sa_reload =1;
}

void debug(int signo)
{
	void *trace[100];
	int trace_size = 0;
	char **message = NULL;
	trace_size = backtrace(trace, 100);
	message = backtrace_symbols(trace, trace_size);
	for(int i  = 0 ; i < trace_size ; i++){
		log_error("DEBUG:%s\n", message[i]);
	}
}

void init_signal()
{
	signal(SIGTERM, stop);
	signal(SIGUSR1, reload);
	signal(SIGUSR2, debug);
	signal(SIGPIPE, debug);
	signal(SIGCHLD, debug);
}

static int create_pidfile(struct instance *dai)
{
	int fd = open(dai->pid_filename, O_WRONLY|O_CREAT |O_TRUNC,0644);
	if(fd < 0){
		log_error("opening pid file %s failed %s", dai->pid_filename, strerror(errno));
		return -1;
	}
	dai->pidfile = 1;
	char pid[256];
	int pid_len = snprintf(pid, 256,"%d", dai->pid);
	int ret  = write(fd, pid, pid_len);
	if(ret < 0 ){
		log_error("write to  pid file %s failed %s", dai->pid_filename, strerror(errno));
		return -1;
	}
	close(fd);
	return 0;
}

int sa_pre_run(struct instance *dai)
{
	int status;
	stat_init_log_("search_agent", dai->log_dir);
	stat_set_log_level_(dai->log_level);
	if (daemonize == 1)
	{
		daemon(1, 0);
	}
	if (dai->cpumask != -1)
	{
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(dai->cpumask, &set);
		int ret = sched_setaffinity(0, sizeof(cpu_set_t), &set);
		if (ret == -1)
		{
			log_error("set cpu affinity error");
			return -1;
		}
	}
	init_signal();
	dai->pid = getpid();
	if(dai->pid_filename){
		status = create_pidfile(dai);
		if(status != 0){
			return -1;
		}
	}
	log_error("search-agent-%s started on pid %d", SEARCH_VERSION_STR, dai->pid);
	return 0;
}

static void sa_remove_pidfile(struct instance *dai)
{
	int status;
	status = unlink(dai->pid_filename);
	if(status < 0 )
	{
		log_error("unlink of pid file '%s' failed, ignored %s", dai->pid_filename, strerror(errno));
	}
}
static void sa_post_run(struct instance *dai)
{
	if(dai->pidfile)
	{
			sa_remove_pidfile(dai);
	}
	log_error("search-agent %s stopped", SEARCH_VERSION_STR);
}

static void sa_run(struct instance *dai)
{
	CSearchAgentCore core ;
	int status = core.CoreStart(dai);
	if(status < 0 )
	{
		log_error("core start error");
		return ;
	}
	while(sa_stop == 0)
	{
		core.CoreLoop();
	}
}

int main(int argc, char *argv[])
{
	int status;
	struct instance dai;

	set_default_options(&dai);
	dai.argv = argv;
	status = get_options(argc, argv, &dai);
	if (status != 0)
	{
		show_usage();
		exit(1);
	}
	if (show_version)
	{
		printf("This is search-agent-%s\r\n", SEARCH_VERSION_STR);
		printf("search-agent compile data %s %s", compdatestr, comptimestr);
		if (show_help)
		{
			show_usage();
		}
		exit(0);
	}
	status = sa_pre_run(&dai);
	if(status < 0 )
	{
		sa_post_run(&dai);
		exit(1);
	}
	sa_run(&dai);
	sa_post_run(&dai);
	return 0;
}
