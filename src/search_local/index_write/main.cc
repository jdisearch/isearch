/*
 * =====================================================================================
 *
 *       Filename:  main.cc
 *
 *    Description:  Entrance.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "agent_listen_pkg.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "stat_index.h"
#include "task_request.h"
#include "config.h"
#include "poll_thread.h"
#include "log.h"
#include "pipetask.h"
#include "memcheck.h"
#include "agent_process.h"
#include "index_conf.h"
#include "index_write.h"
#include "snapshot_service.h"
#include "top_index_service.h"
#include "image_service.h"
#include "dtc_tools.h"
#include "comm.h"
#include "version.h"
#include "monitor.h"
#include "index_tbl_op.h"

#define STRING_HELPER(str)                                      #str
#define STRING(x)                                               STRING_HELPER(x)
#define VERSION_MAJOR                                           1
#define VERSION_MINOR                                           1
#define VERSION_BUILD                                           0
#define MAIN_VERSION \
        STRING(VERSION_MAJOR) "." \
        STRING(VERSION_MINOR) "." \
        STRING(VERSION_BUILD)

#ifndef GIT_VERSION
#define GIT_VERSION                                                     0000000
#endif
#define INDEX_VERSION_STR                                                     MAIN_VERSION "-" STRING(GIT_VERSION)

volatile int stop = 0;
int background = 1;
pthread_t mainthreadid;

const char progname[] = "index_write";
const char *conf_filename = "../conf/index_write.conf";
int gMaxConnCnt;
static CAgentListenPkg *agentListener;
static CAgentProcess   *agentProcess;
static CTaskIndexGen *indexGen;
static CTaskTopIndex *topIndex;
static CTaskSnapShot *snapShot;
static CTaskImage *image;
//single thread version
static CPollThread *workerThread;

static int Startup_Thread()
{
	int ret = 0;

	indexGen = NULL;
	topIndex = NULL;
	snapShot = NULL;

	workerThread = new CPollThread("worker");
	if (workerThread->InitializeThread() == -1){
		log_error("InitializeThread error");
		return -1;
	}
	agentProcess = new CAgentProcess(workerThread);
	if(NULL == agentProcess){
		return -1;
	}
	switch(IndexConf::Instance()->GetGlobalConfig().service_type)
	{
	default:
	case SERVICE_INDEXGEN:
		indexGen = new CTaskIndexGen(workerThread);
		if(NULL == indexGen)
			return -1;
		agentProcess->BindDispatcher(indexGen);
		break;
	case SERVICE_TOPINDEX:
		topIndex = new CTaskTopIndex(workerThread);
		if(NULL == topIndex)
			return -1;
		ret = topIndex->pre_process();
		if(ret != 0){
			return -1;
		}
		agentProcess->BindDispatcher(topIndex);
		break;
	case SERVICE_SNAPSHOT:
		snapShot = new CTaskSnapShot(workerThread);
		if(NULL == snapShot)
			return -1;
		ret = snapShot->pre_process();
		if(ret != 0){
			return -1;
		}
		agentProcess->BindDispatcher(snapShot);
		break;
	case SERVICE_PIC:
		image = new CTaskImage(workerThread);
		if(NULL == image)
			return -1;
		ret = image->pre_process();
		if(0!= ret)
			return -1;
		agentProcess->BindDispatcher(image);
		break;
	}

	agentListener = new CAgentListenPkg();
	if(agentListener->Bind(IndexConf::Instance()->GetGlobalConfig().listen_addr.c_str(), agentProcess, 0) < 0){
		log_error("bind addr error");
		return -1;
	}
		
	workerThread->RunningThread();
	agentListener->Run();
	return 0;
}

int configInit(void)
{
	if (!IndexConf::Instance()->ParseConf(conf_filename)) {
		cout << "load conf file error " << conf_filename << endl;
		return -RT_PARSE_CONF_ERR;
	}
	SGlobalIndexConfig &globalconfig = IndexConf::Instance()->GetGlobalConfig();

	stat_init_log_(progname, globalconfig.logPath.c_str());
	stat_set_log_level_(globalconfig.iLogLevel);
	log_info("%s v%s: log level %d starting....", progname, INDEX_VERSION_STR, globalconfig.iLogLevel);

	return 0;
}

static void sigterm_handler(int signo)
{
	stop = 1;
}

void index_create_pid(string str_pid_file) {
	ofstream pid_file;
	pid_file.open(str_pid_file.c_str(), ios::out | ios::trunc);
	if (pid_file.is_open()) {
		pid_file << getpid();
		pid_file.close();
	}
	else {
		log_error("open pid file error. file:%s, errno:%d, errmsg:%s.",
			str_pid_file.c_str(), errno, strerror(errno));
	}
}

static int DaemonStart()
{
	struct sigaction sa;
	sigset_t sset;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	sigemptyset(&sset);
	sigaddset(&sset, SIGTERM);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	sigprocmask(SIG_UNBLOCK, &sset, &sset);
	if(!IndexConf::Instance()->GetGlobalConfig().background){
		background = 0;
	}

	int ret = background ? daemon (1, 1) : 0;
	mainthreadid = pthread_self();
	return ret;
}

void ServicePostRun(string str_pid_file) {
	IndexConf::Instance()->Destroy();
	SplitManager::Instance()->Destroy();
	unlink(str_pid_file.c_str());
	DTCTools::Destroy();
}

int main()
{
	CThread *mainThread;
	NEW(CThread("main", CThread::ThreadTypeProcess), mainThread);
	if(mainThread != NULL) {
		mainThread->InitializeThread();
	}
	if(configInit() != 0){
		log_error("config init error");
		return -1;
	}
	if (DaemonStart () < 0){
		log_error("DaemonStart error");
		return -1;
	}
	index_create_pid(IndexConf::Instance()->GetGlobalConfig().pid_file);
	InitStat(IndexConf::Instance()->GetGlobalConfig().service_name.c_str());
	SDTCHost &dtchost = IndexConf::Instance()->GetDTCIndexConfig();
	if (g_IndexInstance.InitServer(dtchost) != 0) {
		log_error("dtc init error");
		return -1;
	}
	if (g_delIndexInstance.InitServer(dtchost) != 0) {
		log_error("dtc init error");
		return -1;
	}
	SDTCHost &indexHost = IndexConf::Instance()->GetDTCIntelligentConfig();
	if (g_hanpinIndexInstance.InitServer(indexHost) != 0) {
		log_error("dtc init error");
		return -1;
	}
	if (!SplitManager::Instance()->Init(IndexConf::Instance()->GetGlobalConfig())) {
		log_error("g_splitManager init error");
		return -1;
	}
	//start statistic thread.
	statmgr.StartBackgroundThread();
	//ump monitor initialize
	common::ProfilerMonitor::GetInstance().Initialize();
	DeleteTask::GetInstance().Initialize();
	if(Startup_Thread() < 0){
		stop = 1;
	}
	log_info("%s v%s: running...", progname, INDEX_VERSION_STR);
	while(!stop){
		sleep(10);
	}

	log_info("%s v%s: stoppping...", progname, INDEX_VERSION_STR);

	if(workerThread){
		workerThread->interrupt();
	}
	
	//DELETE(workerThread);
	DELETE(agentListener);
	DELETE(indexGen);
	DELETE(agentProcess);
	ServicePostRun(IndexConf::Instance()->GetGlobalConfig().pid_file);
	statmgr.StopBackgroundThread();
	log_info("%s v%s: stopped", progname, INDEX_VERSION_STR);
	return 0;
}

/* ends here */
