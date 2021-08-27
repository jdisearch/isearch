/*
 * =====================================================================================
 *
 *       Filename:  index_read.h
 *
 *    Description:  index read class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "db_manager.h"
#include "split_manager.h"
#include "search_conf.h"
#include "log.h"
#include "data_manager.h"
#include "index_tbl_op.h"
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "stat_index.h"
#include "task_request.h"
#include "config.h"
#include "poll_thread.h"
#include "pipetask.h"
#include "memcheck.h"
#include "agent_process.h"
#include "agent_listen_pkg.h"
#include "search_process.h"
#include "result_cache.h"
#include "ternary.h"
#include "mem_pool.h"
#include "skiplist.h"
#include "monitor.h"
#include "index_sync/sync_index_timer.h"
#include "correction/correction_manager.h"
using namespace std;

#define STRING_HELPER(str)                                      #str
#define STRING(x)                                               STRING_HELPER(x)
#define VERSION_MAJOR                                           1
#define VERSION_MINOR                                           1
#define VERSION_BUILD                                           0
#define MAIN_VERSION \
        STRING(VERSION_MAJOR) "." \
        STRING(VERSION_MINOR) "." \
        STRING(VERSION_BUILD)

#ifndef ACTIVE_GIT_VERSION
#define ACTIVE_GIT_VERSION                                                     0000000
#endif
#define SEARCH_VERSION_STR                                                     MAIN_VERSION "-" STRING(ACTIVE_GIT_VERSION)

static char conf_filename[1024] = "../conf/index_read.conf";
const char *cache_file = "../conf/cache.conf";

static CPollThread* workerThread;
static CPollThread* syncThread;
static CAgentProcess* agentProcess;
static CAgentListenPkg* agentListener;
static CSearchProcess* searchProcess;
static CPollThread* updateThread;

extern MemPool skipNodePool;

pthread_mutex_t mutex;
SyncIndexTimer *globalSyncIndexTimer;
int stop = 0;

static void log_init(const char *log_dir, int log_level) {
	stat_init_log_("index_read", log_dir);
	stat_set_log_level_(log_level);
}

void catch_signal(int32_t signal) {
	switch (signal) {
	case SIGTERM:
		stop = 1;
		log_error("catch a stop signal.");
		break;
	default:
		break;
	}
}

void search_create_pid(string str_pid_file) {
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

void search_delete_pid(string str_pid_file) {
	unlink(str_pid_file.c_str());
}

static int ServicePreRun(int log_level, bool deam, string log_path)
{
	if (deam)
		daemon(1,0);
	log_init(log_path.c_str(), log_level);
	log_debug("start service verison %s build at %s %s", SEARCH_VERSION_STR, __DATE__, __TIME__);

	pthread_mutex_init(&mutex, NULL);
	skipNodePool.SetMemInfo("skipnode", sizeof(SkipListNode));

	if (SIG_ERR == signal(SIGTERM, catch_signal))
		log_info("set stop signal handler error. errno:%d,errmsg:%s.", errno, strerror(errno));
	if (SIG_ERR == signal(SIGINT, catch_signal))
		log_info("set stop signal handler error. errno:%d,errmsg:%s.", errno, strerror(errno));

	SGlobalConfig &global_cfg = SearchConf::Instance()->GetGlobalConfig();
	search_create_pid(global_cfg.pid_file);

	if (!SplitManager::Instance()->Init(global_cfg.sWordsPath, global_cfg.sTrainPath, global_cfg.sSplitMode)) {
		log_error("g_splitManager init error");
		return -RT_INIT_ERR;
	}

	if(!CorrectManager::Instance()->Init(global_cfg.sEnWordsPath, global_cfg.sCharacterPath)){
		log_error("Correction init error");
		return -RT_INIT_ERR;
	}

	if (!DataManager::Instance()->Init(global_cfg)){
		log_error("data manager error");
		return -RT_INIT_ERR;
	}

	if (!DBManager::Instance()->Init(global_cfg)){
		log_error("db manager error");
		return -RT_INIT_ERR;
	}

	CConfig* DTCCacheConfig = new CConfig();
	if (cache_file == NULL || DTCCacheConfig->ParseConfig(cache_file, "search_cache")) {
		log_error("no cache config or config file [%s] is error", cache_file);
		return -RT_INIT_ERR;
	}
	const char *master_bind = DTCCacheConfig->GetStrVal("search_cache", "BindAddr");
	if (master_bind == NULL) {
		log_error("has no master bind addr");
		return -RT_INIT_ERR;
	}
	string bindAddr = master_bind;
	SDTCHost &dtchost = SearchConf::Instance()->GetDTCConfig();
	if (g_IndexInstance.InitServer(dtchost, bindAddr) != 0) {
		log_error("dtc init error");
		return -RT_INIT_ERR;
	}

	SDTCHost &indexHost = SearchConf::Instance()->GetIndexDTCConfig();
	if (g_hanpinIndexInstance.InitServer2(indexHost) != 0) {
		log_error("index dtc init error");
		return -RT_INIT_ERR;
	}

	return 0;
}

void ServicePostRun(string str_pid_file) {
	SearchConf::Instance()->Destroy();
	DataManager::Instance()->Destroy();
	SplitManager::Instance()->Destroy();
	CorrectManager::Instance()->Destroy();
	search_delete_pid(str_pid_file);
}

void MainThreadWait()
{
	while (!stop)
	{
		sleep(10);
	}
	log_info("index_read will exit because catching a stop signal.");
}

bool InitSyncThread() {
	syncThread = new CPollThread("sync");
	if (syncThread == NULL || syncThread->InitializeThread() == -1) {
		log_error("initialize syncThread error");
		return false;
	}
	CTimerList *syncTimer = syncThread->GetTimerList(1);
	globalSyncIndexTimer = new SyncIndexTimer(syncTimer);
	if (globalSyncIndexTimer != NULL) {
		return globalSyncIndexTimer->StartSyncTask();
	}
	return false;
}


int main(int argc, char *argv[]) {
	if (!SearchConf::Instance()->ParseConf(conf_filename)) {
		cout << "load conf file error " << conf_filename << endl;
		return -RT_PARSE_CONF_ERR;
	}

	SGlobalConfig &globalconfig = SearchConf::Instance()->GetGlobalConfig();
	if (ServicePreRun(globalconfig.iLogLevel, globalconfig.daemon, globalconfig.logPath) != 0) {
		log_error("service preRun error.");
		return -RT_PRE_RUN_ERR;
	}
	if (InitStat(globalconfig.sServerName.c_str()) != 0) {
		log_error("init stat error.");
		return -RT_PRE_RUN_ERR;
	}
	//start statistic thread.
	statmgr.StartBackgroundThread();
	//ump monitor initialize
	common::ProfilerMonitor::GetInstance().Initialize();

	updateThread = new CPollThread("update");
	if (updateThread->InitializeThread() == -1) {
		log_error("init updateThread error.");
		return -RT_INIT_ERR;
	}

	if (InitSyncThread() == false) {
		log_error("InitSyncThread error.");
		return -RT_PRE_RUN_ERR;
	}

	workerThread = new CPollThread("worker");
	if (workerThread->InitializeThread() == -1) {
		log_error("init workerThread error.");
		return -RT_INIT_ERR;
	}

	searchProcess = new CSearchProcess(workerThread);
	agentProcess = new CAgentProcess(workerThread);
	agentProcess->BindDispatcher(searchProcess);

	agentListener = new CAgentListenPkg();
	stringstream bindAddr;
	bindAddr << "*:" << globalconfig.iPort << "/tcp";
	int32_t listen_fd = 0;
	if (agentListener->Bind(bindAddr.str().c_str(), agentProcess, listen_fd) < 0) {
		log_error("bind agentListener error.");
		return -RT_BIND_ERR;
	}
	CacheOperator *m_CacheOperator = new CacheOperator(workerThread, globalconfig.iTimeInterval);
	if (m_CacheOperator == NULL)
	{
		log_error("no memory for m_CacheOperator");
		return -RT_INIT_ERR;
	}
	if (m_CacheOperator->Init(globalconfig.iExpireTime, globalconfig.iMaxCacheSlot) != 0) {
		log_error("m_CacheOperator init error.");
		return -RT_INIT_ERR;
	}

	CacheOperator *m_IndexCacheOperator = new CacheOperator(workerThread, globalconfig.iTimeInterval);
	if (m_IndexCacheOperator == NULL)
	{
		log_error("no memory for m_IndexCacheOperator");
		return -RT_INIT_ERR;
	}
	if (m_IndexCacheOperator->InitIndex(globalconfig.iExpireTime, globalconfig.iIndexMaxCacheSlot) !=0 ) {
		log_error("m_IndexCacheOperator init error.");
		return -RT_INIT_ERR;
	}

	stringstream ssAppFieldFile;
	ssAppFieldFile << globalconfig.sCaDir << globalconfig.iCaPid;
	UpdateOperator *m_updateOperator = new UpdateOperator(updateThread, globalconfig.iUpdateInterval, ssAppFieldFile.str());
	if (m_updateOperator == NULL)
	{
		log_error("no memory for m_updateOperator");
		return -RT_INIT_ERR;
	}
	m_updateOperator->Attach();

	workerThread->RunningThread();
	updateThread->RunningThread();

	agentListener->Run();

	MainThreadWait();
	ServicePostRun(globalconfig.pid_file);
	statmgr.StopBackgroundThread();
	DELETE(m_CacheOperator);
	DELETE(m_IndexCacheOperator);
	DELETE(agentListener);
	DELETE(agentProcess);
	DELETE(m_updateOperator);
	if (workerThread)
	{
		workerThread->interrupt();
	}
	if (updateThread)
	{
		updateThread->interrupt();
	}
	DELETE(searchProcess);
	//DELETE(workerThread);
	DELETE(updateThread);
	pthread_mutex_destroy(&mutex);


	return 0;
}