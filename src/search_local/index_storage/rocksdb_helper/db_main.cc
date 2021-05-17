/*
 * =====================================================================================
 *
 *       Filename:  db_main.cc  
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
#if 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <thread>
#include <dtc_global.h>
#include <version.h>
#include <proc_title.h>
#include <log.h>
#include <config.h>
#include "table_def_manager.h"
#include <daemon.h>
#include <listener.h>
#include <net_addr.h>
#include <unix_socket.h>
#include <watchdog_listener.h>
#include "dtcutils.h"

#include "rocksdb_conn.h"
#include "db_process_rocks.h"
#include "rocksdb_direct_process.h"

extern void _set_remote_log_config_(const char *addr, int port, int businessid);
const char progname[] = "rocksdb_helper";
const char usage_argv[] = "machId addr [port]";
char cacheFile[256] = CACHE_CONF_NAME;
char tableFile[256] = TABLE_CONF_NAME;

static HelperProcessBase *helperProc;
static unsigned int procTimeout;

static RocksDBConn *gRocksdbConn;
std::string gRocksdbPath = "../rocksdb_data";
std::string gRocksDirectAccessPath = "/tmp/domain_socket/";
static RocksdbDirectProcess *gRocksdbDirectProcess;

int targetNewHash;
int hashChanging;

static int sync_decode(DTCTask *task, int netfd, HelperProcessBase *helperProc)
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
			log_info("decode error!!!!!");
			return -1;
		}
		if (code == DecodeDataError)
		{
			if (task->result_code() == 0 || task->result_code() == -EC_EXTRA_SECTION_DATA) // -EC_EXTRA_SECTION_DATA   verify package
				return 0;
			log_notice("decode error, fd=%d, %d", netfd, task->result_code());
			return -1;
		}
		helperProc->set_title("Receiving...");
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
	helperProc->set_title("listener");
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
			{ // �������Ѿ��˳�
				log_error("dtc father process not exist. helper[%d] exit now.", getpid());
				exit(0);
			}
			log_info("parent process close the connection!");
			usleep(10000);
		}
	}
	exit(0);
}

static void proc_timeout_handler(int signo)
{
	log_error("mysql process timeout(more than %u seconds), helper[pid: %d] exit now.", procTimeout, getpid());
	exit(-1);
}

#ifdef __DEBUG__
static void inline simulate_helper_delay(void)
{
	char *env = getenv("ENABLE_SIMULATE_DTC_HELPER_DELAY_SECOND");
	if (env && env[0] != 0)
	{
		unsigned delay_sec = atoi(env);
		if (delay_sec > 5)
			delay_sec = 5;

		log_debug("simulate dtc helper delay second[%d s]", delay_sec);
		sleep(delay_sec);
	}
	return;
}
#endif

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

	helperProc->set_title("Initializing...");

	if (procTimeout > 0)
		signal(SIGALRM, proc_timeout_handler);

	alarm(procTimeout);
	if (helperProc->Init(args->gid, dbConfig, TableDefinitionManager::Instance()->get_cur_table_def(), args->role) != 0)
	{
		log_error("%s", "helper process init failed");
		exit(-1);
	}

	helperProc->init_ping_timeout();
	alarm(0);

	_set_remote_log_config_(gConfig->get_str_val("cache", "RemoteLogAddr"),
							gConfig->get_int_val("cache", "RemoteLogPort", 0),
							dtc::utils::get_bid());

	_set_remote_log_fd_();

	hashChanging = gConfig->get_int_val("cache", "HashChanging", 0);
	targetNewHash = gConfig->get_int_val("cache", "TargetNewHash", 0);

	unsigned int timeout;

	while (!stop)
	{
		helperProc->set_title("Waiting...");
		DTCTask *task = new DTCTask(TableDefinitionManager::Instance()->get_cur_table_def());
		if (sync_decode(task, args->netfd, helperProc) < 0)
		{
			log_info("sync decode failed!");
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
			case DRequest::ReloadConfig:
			case DRequest::Replicate:
			case DRequest::LocalMigrate:
				timeout = 2 * procTimeout;
			default:
				timeout = procTimeout;
			}
			alarm(timeout);
#ifdef __DEBUG__
			simulate_helper_delay();
#endif
			helperProc->process_task(task);
			alarm(0);
		}

		helperProc->set_title("Sending...");
		Packet *reply = new Packet;
		reply->encode_result(task);

		if (sync_send(reply, args->netfd) < 0)
		{
			delete reply;
			delete task;
			break;
			log_info("sync send failed!");
		}
		delete reply;
		delete task;
	}
	close(args->netfd);
	helperProc->set_title("Exiting...");

	delete helperProc;
	daemon_cleanup();
#if MEMCHECK
	log_info("%s v%s: stopped", progname, version);
	dump_non_delete();
	log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());
#endif
	log_info("helper exit!");

	exit(0);
	return 0;
}

static int helper_proc_run_rocks(struct THelperProcParameter args)
{
	log_info("xx77xx11 test multiple thread model! threadId:%d, fd:%d", std::this_thread::get_id(), args.netfd);

	open("/dev/null", O_RDONLY);

	helperProc->set_title("Initializing...");

	if (procTimeout > 0)
		signal(SIGALRM, proc_timeout_handler);

	alarm(procTimeout);

	// helperProc->init_ping_timeout();
	alarm(0);

	_set_remote_log_config_(gConfig->get_str_val("cache", "RemoteLogAddr"),
							gConfig->get_int_val("cache", "RemoteLogPort", 0),
							dtc::utils::get_bid());

	_set_remote_log_fd_();

	hashChanging = gConfig->get_int_val("cache", "HashChanging", 0);
	targetNewHash = gConfig->get_int_val("cache", "TargetNewHash", 0);

	unsigned int timeout;

	while (!stop)
	{
		helperProc->set_title("Waiting...");
		DTCTask *task = new DTCTask(TableDefinitionManager::Instance()->get_cur_table_def());
		if (sync_decode(task, args.netfd, helperProc) < 0)
		{
			log_info("sync decode failed!");
			delete task;
			break;
		}

		log_info("recieve request, threadId:%d", std::this_thread::get_id());

		if (task->result_code() == 0)
		{
			switch (task->request_code())
			{
			case DRequest::Insert:
			case DRequest::Update:
			case DRequest::Delete:
			case DRequest::Replace:
			case DRequest::ReloadConfig:
			case DRequest::Replicate:
			case DRequest::LocalMigrate:
				timeout = 2 * procTimeout;
			default:
				timeout = procTimeout;
			}
			alarm(timeout);
#ifdef __DEBUG__
			simulate_helper_delay();
#endif
			helperProc->process_task(task);
			alarm(0);
		}

		helperProc->set_title("Sending...");
		Packet *reply = new Packet;
		reply->encode_result(task);

		if (sync_send(reply, args.netfd) < 0)
		{
			delete reply;
			delete task;
			break;
			log_info("sync send failed!");
		}
		delete reply;
		delete task;
	}
	close(args.netfd);
	helperProc->set_title("Exiting...");

	daemon_cleanup();
#if MEMCHECK
	log_info("%s v%s: stopped", progname, version);
	dump_non_delete();
	log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());
#endif
	log_info("helper exit!");

	return 0;
}

// check version base on DB type
int check_db_version(void)
{
	// dbConfig->dstype = 1;
	switch (dbConfig->dstype)
	{
		case 0:
		default:
		case 2:
		{
			log_debug("no need to check rocksdb!");
			// checker guass db version
			break;
		}
	}

	return -1;
}

int check_db_table(int gid, int role)
{
	HelperProcessBase *helper;
	switch (dbConfig->dstype)
	{
	case 0:
	default:
	case 2:
		// no table concept in rocksdb, no need to check
		log_error("no need to check table in rocksdb storage!");
		return 0;
	}

	if (procTimeout > 1)
	{
		helper->set_proc_timeout(procTimeout - 1);
		signal(SIGALRM, proc_timeout_handler);
	}

	alarm(procTimeout);
	log_debug("begin initialize gauss db");
	if (helper->Init(gid, dbConfig, TableDefinitionManager::Instance()->get_cur_table_def(), role) != 0)
	{
		log_error("%s", "helper process init failed");
		delete helper;
		alarm(0);
		return (-1);
	}

	if (helper->check_table() != 0)
	{
		delete helper;
		alarm(0);
		return (-2);
	}

	alarm(0);
	delete helper;

	return (0);
}

int create_rocks_domain_dir()
{
	// create domain socket directory
	int ret = access(gRocksDirectAccessPath.c_str(), F_OK);
	if (ret != 0)
	{
		int err = errno;
		if (errno == ENOENT)
		{
			// create log dir
			if (mkdir(gRocksDirectAccessPath.c_str(), 0755) != 0)
			{
				log_error("create rocksdb domain socket dir failed! path:%s, errno:%d", gRocksDirectAccessPath.c_str(), errno);
				return -1;
			}
		}
		else
		{
			log_error("access rocksdb domain socket dir failed!, path:%s, errno:%d", gRocksDirectAccessPath.c_str(), errno);
			return -1;
		}
	}

	return 0;
}

// process rocksdb direct access
int rocks_direct_access_proc()
{
	log_error("Rocksdb direct access channel open!");

	std::string socketPath = gRocksDirectAccessPath;
	std::string dtcDeployAddr = dbConfig->cfgObj->get_str_val("cache", "BindAddr");

	SocketAddress sockAddr;
	const char *strRet = sockAddr.set_address(dtcDeployAddr.c_str());
	if (strRet)
	{
		log_error("parse dtc bind addr failed, errmsg:%s", strRet);
		return -1;
	}

	int dtcDeployPort;
	switch (sockAddr.addr->sa_family)
	{
	case AF_INET:
		dtcDeployPort = ntohs(sockAddr.in4->sin_port);
		break;
	case AF_INET6:
		dtcDeployPort = ntohs(sockAddr.in6->sin6_port);
		break;
	default:
		log_error("unsupport addr type! addr:%s, type:%d", dtcDeployAddr.c_str(), sockAddr.addr->sa_family);
		return -1;
	}
	assert(dtcDeployPort > 0);

	socketPath.append("rocks_direct_").append(std::to_string(dtcDeployPort)).append(".sock");

	gRocksdbDirectProcess = new RocksdbDirectProcess(socketPath, helperProc);
	if (!gRocksdbDirectProcess)
	{
		log_error("create RocksdbDirectProcess failed!");
		return -1;
	}

	int ret = gRocksdbDirectProcess->init();
	if (ret != 0)
		return -1;

	return gRocksdbDirectProcess->run_process();
}

int main(int argc, char **argv)
{
	init_proc_title(argc, argv);
	if (dtc_daemon_init(argc, argv) < 0)
		return -1;

	check_db_version();
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

	int usematch = gConfig->get_int_val("cache", "UseMatchedAsAffectedRows", 1);
	int backlog = gConfig->get_int_val("cache", "MaxListenCount", 256);
	int helperTimeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	if (helperTimeout > 1)
		procTimeout = helperTimeout - 1;
	else
		procTimeout = 0;
	addr = argv[1];
	log_error("helper listen addr:%s", addr);

	if (dbConfig->checkTable && check_db_table(helperArgs.gid, helperArgs.role) != 0)
	{
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
	log_info("helper listen fd:%d", fd);

	log_debug("If you want to simulate db busy,"
			  "you can set \"ENABLE_SIMULATE_DTC_HELPER_DELAY_SECOND=second\" before dtc startup");

	daemon_start();

	// create helper instance base on database type
	switch (dbConfig->dstype)
	{
	default:
	case 2:
	{
		// rocksdb
		gRocksdbConn = RocksDBConn::Instance();
		helperProc = new RocksdbProcess(gRocksdbConn);
		assert(helperProc);

		int ret = helperProc->Init(helperArgs.gid, dbConfig, TableDefinitionManager::Instance()->get_cur_table_def(), helperArgs.role);
		if (ret != 0)
		{
			log_error("%s", "helper process init failed");
			return -1;
		}

		gRocksdbConn->set_key_type(TableDefinitionManager::Instance()->get_cur_table_def()->key_type());
		ret = gRocksdbConn->Open(gRocksdbPath);
		assert(ret == 0);

		// start direct rocksdb access channel
		ret = create_rocks_domain_dir();
		if (ret != 0)
			return -1;

		ret = rocks_direct_access_proc();
		if (ret != 0)
			return -1;

		break;
	}
	}

	if (usematch)
		helperProc->use_matched_rows();
#if HAS_LOGAPI
	helperProc->logapi.Init(
		gConfig->get_int_val("LogApi", "MessageId", 0),
		gConfig->get_int_val("LogApi", "CallerId", 0),
		gConfig->get_int_val("LogApi", "TargetId", 0),
		gConfig->get_int_val("LogApi", "InterfaceId", 0));
#endif

	helperProc->init_title(helperArgs.gid, helperArgs.role);
	if (procTimeout > 1)
		helperProc->set_proc_timeout(procTimeout - 1);
	while (!stop)
	{
		helperArgs.netfd = accept_connection(fd);
		char buf[16];
		memset(buf, 0, 16);
		buf[0] = WATCHDOG_INPUT_OBJECT;
		log_info("xx77xx11 procName:%s", helperProc->Name());
		snprintf(buf + 1, 15, "%s", helperProc->Name());

		log_info("fork child helper! fd:%d", helperArgs.netfd);

		if (dbConfig->dstype != 2)
		{
			watch_dog_fork(buf, (int (*)(void *))helper_proc_run, (void *)&helperArgs);
			close(helperArgs.netfd);
		}
		else
		{
			// rocksdb use multiple thread mode
			std::thread runner(helper_proc_run_rocks, helperArgs);
			runner.detach();
		}
	}

	/* close global rocksdb connection */
	if (gRocksdbConn)
	{
		int ret = gRocksdbConn->Close();
		if (ret != 0)
		{
			log_error("close rocksdb connection failed, rockspath:%s", gRocksdbPath.c_str());
		}
	}

	log_info("helper main process exist!");
	if (fd > 0 && addr && addr[0] == '/')
		unlink(addr);
	return 0;
}
#endif
