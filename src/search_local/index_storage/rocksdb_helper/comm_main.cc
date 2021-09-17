/*
 * =====================================================================================
 *
 *       Filename:  comm_main.cc
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

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sched.h>
#include <dlfcn.h>

#include <dtc_global.h>
#include <version.h>
#include <proc_title.h>
#include <log.h>
#include <config.h>
#include <dbconfig.h>
#include "comm_process.h"
#include <daemon.h>
#include <net_addr.h>
#include <listener.h>
#include <unix_socket.h>
#include <watchdog_listener.h>
#include <task_base.h>

const char service_file[] = "./helper-service.so";
const char create_handle_name[] = "create_process";
const char progname[] = "custom-helper";
const char usage_argv[] = "machId addr [port]";
char cacheFile[256] = CACHE_CONF_NAME;
char tableFile[256] = TABLE_CONF_NAME;

static CreateHandle CreateHelper = NULL;
static CommHelper *helperProc;
static unsigned int procTimeout;

class HelperMain
{
public:
	HelperMain(CommHelper *helper) : h(helper){};

	void attach(DTCTask *task) { h->attach((void *)task); }
	void init_title(int group, int role) { h->init_title(group, role); }
	void set_title(const char *status) { h->SetTitle(status); }
	const char *Name() { return h->Name(); }
	int pre_init(int gid, int r)
	{
		if (dbConfig->machineCnt <= gid)
		{
			log_error("parse config error, machineCnt[%d] <= GroupID[%d]", dbConfig->machineCnt, gid);
			return (-1);
		}
		h->GroupID = gid;
		h->Role = r;
		h->_dbconfig = dbConfig;
		h->_tdef = gTableDef[0];
		h->_config = gConfig;
		h->_server_string = dbConfig->mach[gid].role[r].addr;
		h->logapi.init_target(h->_server_string);
		return 0;
	}

private:
	CommHelper *h;
};

static int load_service(const char *dll_file)
{
	void *dll;

	dll = dlopen(dll_file, RTLD_NOW | RTLD_GLOBAL);
	if (dll == (void *)NULL)
	{
		log_crit("dlopen(%s) error: %s", dll_file, dlerror());
		return -1;
	}

	CreateHelper = (CreateHandle)dlsym(dll, create_handle_name);
	if (CreateHelper == NULL)
	{
		log_crit("function[%s] not found", create_handle_name);
		return -2;
	}

	return 0;
}

static int sync_decode(DTCTask *task, int netfd, CommHelper *helperProc)
{
	SimpleReceiver receiver(netfd);
	int code;
	do
	{
		code = task->Decode(receiver);
		if (code == DecodeFatalError)
		{
			if (errno != 0)
				log_notice("decode fatal error, fd=%d, %m", netfd);
			return -1;
		}
		if (code == DecodeDataError)
		{
			if (task->result_code() == 0 || task->result_code() == -EC_EXTRA_SECTION_DATA) // -EC_EXTRA_SECTION_DATA   verify package
				return 0;
			log_notice("decode error, fd=%d, %d", netfd, task->result_code());
			return -1;
		}
		HelperMain(helperProc).set_title("Receiving...");
	} while (!stop && code != DecodeDone);

	if (task->result_code() < 0)
	{
		log_notice("register result, fd=%d, %d", netfd, task->result_code());
		return -1;
	}
	return 0;
}

static int sync_send(Packet *reply, int netfd)
{
	int code;
	do
	{
		code = reply->Send(netfd);
		if (code == SendResultError)
		{
			log_notice("send error, fd=%d, %m", netfd);
			return -1;
		}
	} while (!stop && code != SendResultDone);

	return 0;
}

static void alarm_handler(int signo)
{
	if (background == 0 && getppid() == 1)
		exit(0);
	alarm(10);
}

static int accept_connection(int fd)
{
	HelperMain(helperProc).set_title("listener");
	signal(SIGALRM, alarm_handler);
	while (!stop)
	{
		alarm(10);
		int newfd;
		if ((newfd = accept(fd, NULL, 0)) >= 0)
		{
			alarm(0);
			return newfd;
		}
		if (newfd < 0 && errno == EINVAL)
		{
			if (getppid() == (pid_t)1)
			{ // 父进程已经退出
				log_error("dtc father process not exist. helper[%d] exit now.", getpid());
				exit(0);
			}
			usleep(10000);
		}
	}
	exit(0);
}

static void proc_timeout_handler(int signo)
{
	log_error("comm-helper process timeout(more than %u seconds), helper[pid: %d] exit now.", procTimeout, getpid());
	exit(-1);
}

struct THelperProcParameter
{
	int netfd;
	int gid;
	int role;
};

static int helper_proc_run(struct THelperProcParameter *args)
{
	// close listen fd
	close(0);
	open("/dev/null", O_RDONLY);

	HelperMain(helperProc).set_title("Initializing...");

	if (procTimeout > 0)
		signal(SIGALRM, proc_timeout_handler);

	alarm(procTimeout);
	if (helperProc->Init() != 0)
	{
		log_error("%s", "helper process init failed");
		exit(-1);
	}

	alarm(0);

	unsigned int timeout;

	while (!stop)
	{
		HelperMain(helperProc).set_title("Waiting...");
		DTCTask *task = new DTCTask(gTableDef[0]);
		if (sync_decode(task, args->netfd, helperProc) < 0)
		{
			delete task;
			break;
		}

		if (task->result_code() == 0)
		{
			switch (task->request_code())
			{
			case DRequest::Insert:
			case DRequest::Update:
			case DRequest::Delete:
			case DRequest::Replace:
				timeout = 2 * procTimeout;
			default:
				timeout = procTimeout;
			}
			HelperMain(helperProc).attach(task);
			alarm(timeout);
			helperProc->Execute();
			alarm(0);
		}

		HelperMain(helperProc).set_title("Sending...");
		Packet *reply = new Packet;
		reply->encode_result(task);

		delete task;
		if (sync_send(reply, args->netfd) < 0)
		{
			delete reply;
			break;
		}
		delete reply;
	}
	close(args->netfd);
	HelperMain(helperProc).set_title("Exiting...");

	delete helperProc;
	daemon_cleanup();
#if MEMCHECK
	log_info("%s v%s: stopped", progname, version);
	dump_non_delete();
	log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());
#endif
	exit(0);
	return 0;
}

int main(int argc, char **argv)
{
	init_proc_title(argc, argv);
	if (dtc_daemon_init(argc, argv) < 0)
		return -1;

	argc -= optind;
	argv += optind;

	struct THelperProcParameter helperArgs = {0, 0, 0};
	char *addr = NULL;

	if (argc > 0)
	{
		char *p;
		helperArgs.gid = strtol(argv[0], &p, 0);
		if (*p == '\0' || *p == MACHINEROLESTRING[0])
			helperArgs.role = 0;
		else if (*p == MACHINEROLESTRING[1])
			helperArgs.role = 1;
		else
		{
			log_error("Bad machine id: %s", argv[0]);
			return -1;
		}
	}

	if (argc != 2 && argc != 3)
	{
		show_usage();
		return -1;
	}

	int backlog = gConfig->get_int_val("cache", "MaxListenCount", 256);
	int helperTimeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	if (helperTimeout > 1)
		procTimeout = helperTimeout - 1;
	else
		procTimeout = 0;
	addr = argv[1];

	// load dll file
	const char *file = getenv("HELPER_SERVICE_FILE");
	if (file == NULL || file[0] == 0)
		file = service_file;
	if (load_service(file) != 0)
		return -1;

	helperProc = CreateHelper();
	if (helperProc == NULL)
	{
		log_crit("create helper error");
		return -1;
	}
	if (HelperMain(helperProc).pre_init(helperArgs.gid, helperArgs.role) < 0)
	{
		log_error("%s", "helper prepare init failed");
		exit(-1);
	}
	if (helperProc->global_init() != 0)
	{
		log_crit("helper gobal-init error");
		return -1;
	}

	int fd = -1;
	if (!strcmp(addr, "-"))
		fd = 0;
	else
	{
		if (strcasecmp(gConfig->get_str_val("cache", "CacheShmKey") ?: "", "none") != 0)
		{
			log_warning("standalone %s need CacheShmKey set to NONE", progname);
			return -1;
		}

		SocketAddress sockaddr;
		const char *err = sockaddr.set_address(addr, argc == 2 ? NULL : argv[2]);
		if (err)
		{
			log_warning("host %s port %s: %s", addr, argc == 2 ? "NULL" : argv[2], err);
			return -1;
		}
		if (sockaddr.socket_type() != SOCK_STREAM)
		{
			log_warning("standalone %s don't support UDP protocol", progname);
			return -1;
		}
		fd = sock_bind(&sockaddr, backlog);
		if (fd < 0)
			return -1;
	}

	daemon_start();

#if HAS_LOGAPI
	helperProc->logapi.Init(
		gConfig->get_int_val("LogApi", "MessageId", 0),
		gConfig->get_int_val("LogApi", "CallerId", 0),
		gConfig->get_int_val("LogApi", "TargetId", 0),
		gConfig->get_int_val("LogApi", "InterfaceId", 0));
#endif

	HelperMain(helperProc).init_title(helperArgs.gid, helperArgs.role);
	if (procTimeout > 1)
		helperProc->set_proc_timeout(procTimeout - 1);
	while (!stop)
	{
		helperArgs.netfd = accept_connection(fd);

		watch_dog_fork(HelperMain(helperProc).Name(), (int (*)(void *))helper_proc_run, (void *)&helperArgs);

		close(helperArgs.netfd);
	}

	if (fd > 0 && addr && addr[0] == '/')
		unlink(addr);
	return 0;
}
