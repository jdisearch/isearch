/*
 * =====================================================================================
 *
 *       Filename:  main.cc
 *
 *    Description:  entrance.
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include <version.h>
#include <table_def.h>
#include <config.h>
#include <poll_thread.h>
#include <listener_pool.h>
#include <fence_unit.h>
#include <client_unit.h>
#include <helper_collect.h>
#include <helper_group.h>
#include <buffer_process.h>
#include <buffer_bypass.h>
#include <watchdog.h>
#include <dbconfig.h>
#include <log.h>
#include <daemon.h>
#include <pipetask.h>
#include <mem_check.h>
#include "unix_socket.h"
#include "stat_dtc.h"
#include "task_control.h"
#include "task_multiplexer.h"
#include "black_hole.h"
#include "container.h"
#include "proc_title.h"
#include "plugin_mgr.h"
#include "dtc_global.h"
#include "dynamic_helper_collection.h"
#include "key_route.h"
#include "proxy_listen_pool.h"
#include "proxy_process.h"
#include "version.h"
#include "dtcutils.h"
#include "relative_hour_calculator.h"
#include "buffer_remoteLog.h"
#include "hb_process.h"
#include "logger.h"

using namespace ClusterConfig;
const char progname[] = "dtcd";
const char usage_argv[] = "";
char cacheFile[256] = CACHE_CONF_NAME;
char tableFile[256] = TABLE_CONF_NAME;
int gMaxConnCnt;

static PollThread *cacheThread;
static BufferProcess *cachePool;
static int enablePlugin;
static int initPlugin;
static int disableCache;
static int cacheKey;
static int disableDataSource;
static PollThread *dsThread;
static GroupCollect *helperUnit;
static BarrierUnit *barCache;
static BarrierUnit *barHelper;
static BufferBypass *bypassUnit;
static ListenerPool *listener;
static TaskControl *serverControl;
static PluginManager *plugin_mgr;
KeyRoute *keyRoute;
static int asyncUpdate;
int targetNewHash;
int hashChanging;
static AgentListenPool *agentListener;
static AgentProcess *agentProcess;
static TaskMultiplexer *multiPlexer;

//remote dispatcher,用来迁移数据给远端dtc
static PollThread *remoteThread;
static DynamicHelperCollection *remoteClient;

//single thread version
static PollThread *workerThread;
static HBProcess *hotbackProcess;
static PollThread *hotbackThread;

extern DTCConfig *gConfig;
extern void _set_remote_log_config_(const char *addr, int port, int businessid);
int collect_load_config(DbConfig *dbconfig);
static int plugin_start(void)
{
	initPlugin = 0;

	plugin_mgr = PluginManager::Instance();
	if (NULL == plugin_mgr)
	{
		log_error("create PluginManager instance failed.");
		return -1;
	}

	if (plugin_mgr->open(gConfig->get_int_val("cache", "PluginNetworkMode", 0)) != 0)
	{
		log_error("init plugin manager failed.");
		return -1;
	}

	initPlugin = 1;

	return 0;
}

static int plugin_stop(void)
{
	plugin_mgr->close();
	PluginManager::Destroy();
	plugin_mgr = NULL;

	return 0;
}

static int StatOpenFd()
{
	int count = 0;
	for (int i = 0; i < 1000; i++)
	{
		if (fcntl(i, F_GETFL, 0) != -1)
			count++;
	}
	return count;
}

static int InitCacheMode()
{
	asyncUpdate = gConfig->get_int_val("cache", "DelayUpdate", 0);
	if (asyncUpdate < 0 || asyncUpdate > 1)
	{
		log_crit("Invalid DelayUpdate value");
		return -1;
	}

	const char *keyStr = gConfig->get_str_val("cache", "CacheShmKey");
	if (keyStr == NULL)
	{
		cacheKey = 0;
	}
	else if (!strcasecmp(keyStr, "none"))
	{
		log_notice("CacheShmKey set to NONE, Cache disabled");
		disableCache = 1;
	}
	else if (isdigit(keyStr[0]))
	{
		cacheKey = strtol(keyStr, NULL, 0);
	}
	else
	{
		log_crit("Invalid CacheShmKey value \"%s\"", keyStr);
		return -1;
	}

	disableDataSource = gConfig->get_int_val("cache", "DisableDataSource", 0);
	if (disableCache && disableDataSource)
	{
		log_crit("can't disableDataSource when CacheShmKey set to NONE");
		return -1;
	}

	if (disableCache && asyncUpdate)
	{
		log_crit("can't DelayUpdate when CacheShmKey set to NONE");
		return -1;
	}

	if (disableCache == 0 && cacheKey == 0)
		log_notice("CacheShmKey not set, cache data is volatile");

	if (disableDataSource)
		log_notice("disable data source, cache data is volatile");

	return 0;
}

static int StartHotbackThread()
{
	log_debug("StartHotbackThread begin");
	hotbackThread = new PollThread("hotback");
	hotbackProcess = new HBProcess(hotbackThread);
	if (hotbackThread->initialize_thread() == -1)
	{
		log_error("init hotback thread fail");
		return -1;
	}
	if (hotbackProcess->Init(gConfig->get_size_val("cache", "BinlogTotalSize", BINLOG_MAX_TOTAL_SIZE, 'M'),
							 gConfig->get_size_val("cache", "BinlogOneSize", BINLOG_MAX_SIZE, 'M')) == -1)
	{
		log_error("hotbackProcess init fail");
		return -1;
	}
	log_debug("StartHotbackThread end");
	return 0;
}
static int DTC_StartCacheThread()
{
	log_error("DTC_StartCacheThread start");
	cacheThread = new PollThread("cache");
	cachePool = new BufferProcess(cacheThread, TableDefinitionManager::Instance()->get_cur_table_def(), asyncUpdate ? MODE_ASYNC : MODE_SYNC);
	cachePool->set_limit_node_size(gConfig->get_int_val("cache", "LimitNodeSize", 100 * 1024 * 1024));
	cachePool->set_limit_node_rows(gConfig->get_int_val("cache", "LimitNodeRows", 0));
	cachePool->set_limit_empty_nodes(gConfig->get_int_val("cache", "LimitEmptyNodes", 0));

	if (cacheThread->initialize_thread() == -1)
	{
		return -1;
	}
	unsigned long long cacheSize = gConfig->get_size_val("cache", "CacheMemorySize", 0, 'M');
	if (cacheSize <= (50ULL << 20)) // 50M
	{
		log_error("CacheMemorySize too small");
		return -1;
	}
	else if (sizeof(long) == 4 && cacheSize >= 4000000000ULL)
	{
		log_crit("CacheMemorySize %lld too large", cacheSize);
	}
	else if (
		cachePool->buffer_set_size(
			cacheSize,
			gConfig->get_int_val("cache", "CacheShmVersion", 4)) == -1)
	{
		return -1;
	}

	/* disable async transaction log */
	cachePool->disable_async_log(1);

	int lruLevel = gConfig->get_int_val("cache", "disable_lru_update", 0);
	if (disableDataSource)
	{
		if (cachePool->enable_no_db_mode() < 0)
		{
			return -1;
		}
		if (gConfig->get_int_val("cache", "disable_auto_purge", 0) > 0)
		{
			cachePool->disable_auto_purge();
			// lruLevel = 3; /* LRU_WRITE */
		}
		int autoPurgeAlertTime = gConfig->get_int_val("cache", "AutoPurgeAlertTime", 0);
		cachePool->set_date_expire_alert_time(autoPurgeAlertTime);
		if (autoPurgeAlertTime > 0 && TableDefinitionManager::Instance()->get_cur_table_def()->lastcmod_field_id() <= 0)
		{
			log_crit("Can't start AutoPurgeAlert without lastcmod field");
			return -1;
		}
	}
	cachePool->disable_lru_update(lruLevel);
	cachePool->enable_lossy_data_source(gConfig->get_int_val("cache", "LossyDataSource", 0));

	if (asyncUpdate != MODE_SYNC && cacheKey == 0)
	{
		log_crit("Anonymous shared memory don't support DelayUpdate");
		return -1;
	}

	int iAutoDeleteDirtyShm = gConfig->get_int_val("cache", "AutoDeleteDirtyShareMemory", 0);
	/*disable empty node filter*/
	if (cachePool->cache_open(cacheKey, 0, iAutoDeleteDirtyShm) == -1)
	{
		return -1;
	}

	if (cachePool->update_mode() || cachePool->is_mem_dirty()) // asyncUpdate active
	{
		if (TableDefinitionManager::Instance()->get_cur_table_def()->uniq_fields() < 1)
		{
			log_crit("DelayUpdate needs uniq-field(s)");
			return -1;
		}

		if (disableDataSource)
		{
			if (cachePool->update_mode())
			{
				log_crit("Can't start async mode when disableDataSource.");
				return -1;
			}
			else
			{
				log_crit("Can't start disableDataSource with shm dirty,please flush async shm to db first or delete shm");
				return -1;
			}
		}
		else
		{
			if ((TableDefinitionManager::Instance()->get_cur_table_def()->compress_field_id() >= 0))
			{

				log_crit("sorry,DTC just support compress in disableDataSource mode now.");
				return -1;
			}
		}

		/*marker is the only source of flush speed calculattion, inc precision to 10*/
		cachePool->set_flush_parameter(
			gConfig->get_int_val("cache", "MarkerPrecision", 10),
			gConfig->get_int_val("cache", "MaxFlushSpeed", 1),
			gConfig->get_int_val("cache", "MinDirtyTime", 3600),
			gConfig->get_int_val("cache", "MaxDirtyTime", 43200));

		cachePool->set_drop_count(
			gConfig->get_int_val("cache", "MaxDropCount", 1000));
	}
	else
	{
		if (!disableDataSource)
			helperUnit->disable_commit_group();
	}

	if (cachePool->set_insert_order(dbConfig->ordIns) < 0)
		return -1;

	log_error("DTC_StartCacheThread end");
	return 0;
}

int collect_load_config(DbConfig *dbconfig)
{
	if (0 != disableDataSource)
		return 0;
	if (!helperUnit)
		return -1;
	if (dbconfig == NULL)
	{
		log_error("dbconfig == NULL");
		return -1;
	}
	if (helperUnit->renew_config(dbconfig))
	{
		log_error("helperunit renew config error!");
		return -1;
	}

	return 0;
}

int StartRemoteClientThread()
{
	log_debug("StartRemoteClientThread begin");
	remoteThread = new PollThread("remoteClient");
	remoteClient = new DynamicHelperCollection(remoteThread, gConfig->get_int_val("cache", "HelperCountPerGroup", 16));
	if (remoteThread->initialize_thread() == -1)
	{
		log_error("init remote thread error");
		return -1;
	}

	//get helper timeout
	int timeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	int retry = gConfig->get_int_val("cache", "HelperRetryTimeout", 1);
	int connect = gConfig->get_int_val("cache", "HelperConnectTimeout", 10);

	remoteClient->set_timer_handler(
		remoteThread->get_timer_list(timeout),
		remoteThread->get_timer_list(connect),
		remoteThread->get_timer_list(retry));
	log_debug("StartRemoteClientThread end");
	return 0;
}

int DTC_SetupRemoteClientThread(PollThread *thread)
{
	log_debug("DTC_SetupRemoteClientThread begin");

	remoteClient = new DynamicHelperCollection(thread, gConfig->get_int_val("cache", "HelperCountPerGroup", 16));

	//get helper timeout
	int timeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	int retry = gConfig->get_int_val("cache", "HelperRetryTimeout", 1);
	int connect = gConfig->get_int_val("cache", "HelperConnectTimeout", 10);

	remoteClient->set_timer_handler(
		thread->get_timer_list(timeout),
		thread->get_timer_list(connect),
		thread->get_timer_list(retry));
	log_debug("DTC_SetupRemoteClientThread end");
	return 0;
}
int DTC_StartDataSourceThread()
{
	log_debug("DTC_StartDataSourceThread begin");
	if (disableDataSource == 0)
	{
		helperUnit = new GroupCollect();
		if (helperUnit->load_config(dbConfig, TableDefinitionManager::Instance()->get_cur_table_def()->key_format()) == -1)
		{
			return -1;
		}
	}

	//get helper timeout
	int timeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	int retry = gConfig->get_int_val("cache", "HelperRetryTimeout", 1);
	int connect = gConfig->get_int_val("cache", "HelperConnectTimeout", 10);

	dsThread = new PollThread("source");
	if (dsThread->initialize_thread() == -1)
		return -1;

	if (disableDataSource == 0)
		helperUnit->set_timer_handler(
			dsThread->get_timer_list(timeout),
			dsThread->get_timer_list(connect),
			dsThread->get_timer_list(retry));
	log_debug("DTC_StartDataSourceThread end");
	return 0;
}

static int DTC_SetupBufferProcess(PollThread *thread)
{
	log_error("DTC_SetupBufferProcess start");
	cachePool = new BufferProcess(thread, TableDefinitionManager::Instance()->get_cur_table_def(), asyncUpdate ? MODE_ASYNC : MODE_SYNC);
	cachePool->set_limit_node_size(gConfig->get_int_val("cache", "LimitNodeSize", 100 * 1024 * 1024));
	cachePool->set_limit_node_rows(gConfig->get_int_val("cache", "LimitNodeRows", 0));
	cachePool->set_limit_empty_nodes(gConfig->get_int_val("cache", "LimitEmptyNodes", 0));

	unsigned long long cacheSize = gConfig->get_size_val("cache", "CacheMemorySize", 0, 'M');
	if (cacheSize <= (50ULL << 20)) // 50M
	{
		log_error("CacheMemorySize too small");
		return -1;
	}
	else if (sizeof(long) == 4 && cacheSize >= 4000000000ULL)
	{
		log_crit("CacheMemorySize %lld too large", cacheSize);
	}
	else if (
		cachePool->buffer_set_size(
			cacheSize,
			gConfig->get_int_val("cache", "CacheShmVersion", 4)) == -1)
	{
		return -1;
	}

	/* disable async transaction log */
	cachePool->disable_async_log(1);

	int lruLevel = gConfig->get_int_val("cache", "disable_lru_update", 0);
	if (disableDataSource)
	{
		if (cachePool->enable_no_db_mode() < 0)
		{
			return -1;
		}
		if (gConfig->get_int_val("cache", "disable_auto_purge", 0) > 0)
		{
			cachePool->disable_auto_purge();
			// lruLevel = 3; /* LRU_WRITE */
		}
		int autoPurgeAlertTime = gConfig->get_int_val("cache", "AutoPurgeAlertTime", 0);
		cachePool->set_date_expire_alert_time(autoPurgeAlertTime);
		if (autoPurgeAlertTime > 0 && TableDefinitionManager::Instance()->get_cur_table_def()->lastcmod_field_id() <= 0)
		{
			log_crit("Can't start AutoPurgeAlert without lastcmod field");
			return -1;
		}
	}
	cachePool->disable_lru_update(lruLevel);
	cachePool->enable_lossy_data_source(gConfig->get_int_val("cache", "LossyDataSource", 0));

	if (asyncUpdate != MODE_SYNC && cacheKey == 0)
	{
		log_crit("Anonymous shared memory don't support DelayUpdate");
		return -1;
	}

	int iAutoDeleteDirtyShm = gConfig->get_int_val("cache", "AutoDeleteDirtyShareMemory", 0);
	/*disable empty node filter*/
	if (cachePool->cache_open(cacheKey, 0, iAutoDeleteDirtyShm) == -1)
	{
		return -1;
	}

	if (cachePool->update_mode() || cachePool->is_mem_dirty()) // asyncUpdate active
	{
		if (TableDefinitionManager::Instance()->get_cur_table_def()->uniq_fields() < 1)
		{
			log_crit("DelayUpdate needs uniq-field(s)");
			return -1;
		}

		if (disableDataSource)
		{
			if (cachePool->update_mode())
			{
				log_crit("Can't start async mode when disableDataSource.");
				return -1;
			}
			else
			{
				log_crit("Can't start disableDataSource with shm dirty,please flush async shm to db first or delete shm");
				return -1;
			}
		}
		else
		{
			if ((TableDefinitionManager::Instance()->get_cur_table_def()->compress_field_id() >= 0))
			{
				log_crit("sorry,DTC just support compress in disableDataSource mode now.");
				return -1;
			}
		}

		/*marker is the only source of flush speed calculattion, inc precision to 10*/
		cachePool->set_flush_parameter(
			gConfig->get_int_val("cache", "MarkerPrecision", 10),
			gConfig->get_int_val("cache", "MaxFlushSpeed", 1),
			gConfig->get_int_val("cache", "MinDirtyTime", 3600),
			gConfig->get_int_val("cache", "MaxDirtyTime", 43200));

		cachePool->set_drop_count(gConfig->get_int_val("cache", "MaxDropCount", 1000));
	}
	else
	{
		if (!disableDataSource)
			helperUnit->disable_commit_group();
	}

	if (cachePool->set_insert_order(dbConfig->ordIns) < 0)
		return -1;

	log_error("DTC_SetupBufferProcess end");
	return 0;
}

int DTC_SetupGroupCollect(PollThread *thread)
{
	log_debug("DTC_SetupGroupCollect begin");

	helperUnit = new GroupCollect();
	if (helperUnit->load_config(dbConfig, TableDefinitionManager::Instance()->get_cur_table_def()->key_format()) == -1)
	{
		return -1;
	}
	//get helper timeout
	int timeout = gConfig->get_int_val("cache", "HelperTimeout", 30);
	int retry = gConfig->get_int_val("cache", "HelperRetryTimeout", 1);
	int connect = gConfig->get_int_val("cache", "HelperConnectTimeout", 10);

	helperUnit->set_timer_handler(thread->get_timer_list(timeout),
								thread->get_timer_list(connect),
								thread->get_timer_list(retry));

	helperUnit->Attach(thread);
	if (disableCache)
	{
		helperUnit->disable_commit_group();
	}
	log_debug("DTC_SetupGroupCollect end");
	return 0;
}

static int DTC_Startup_Single_Thread()
{
	//instant workerThread
	if (0 != StartHotbackThread())
	{
		return -1;
	}
	workerThread = new PollThread("worker");
	if (workerThread->initialize_thread() == -1)
		return -1;

	if (DTC_SetupRemoteClientThread(workerThread) < 0)
		return -1;

	if (disableDataSource == 0)
	{
		if (DTC_SetupGroupCollect(workerThread) < 0)
			return -1;
	}

	if (disableCache == 0)
	{
		if (DTC_SetupBufferProcess(workerThread) < 0)
			return -1;
	}

	int iMaxBarrierCount = gConfig->get_int_val("cache", "MaxBarrierCount", 1000);
	int iMaxKeyCount = gConfig->get_int_val("cache", "MaxKeyCount", 1000);

	barCache = new BarrierUnit(workerThread, iMaxBarrierCount, iMaxKeyCount, BarrierUnit::IN_FRONT);
	if (disableCache)
	{
		bypassUnit = new BufferBypass(workerThread);
		barCache->bind_dispatcher(bypassUnit);
		bypassUnit->bind_dispatcher(helperUnit);
	}
	else
	{
		keyRoute = new KeyRoute(workerThread, TableDefinitionManager::Instance()->get_cur_table_def()->key_format());
		if (!check_and_create())
		{
			log_error("check_and_create error");
			return -1;
		}
		else
		{
			log_debug("check_and_create ok");
		}
		std::vector<ClusterNode> clusterConf;
		if (!parse_cluster_config(&clusterConf))
		{
			log_error("parse_cluster_config error");
			return -1;
		}
		else
			log_debug("parse_cluster_config ok");

		keyRoute->Init(clusterConf);
		if (keyRoute->load_node_state_if_any() != 0)
		{
			log_error("key route init error!");
			return -1;
		}
		log_debug("keyRoute->Init ok");
		barCache->bind_dispatcher(keyRoute);
		keyRoute->bind_cache(cachePool);
		keyRoute->bind_remote_helper(remoteClient);
		cachePool->bind_dispatcher_remote(remoteClient);
		cachePool->bind_hb_log_dispatcher(hotbackProcess);
		if (disableDataSource)
		{
			BlackHole *hole = new BlackHole(workerThread);
			cachePool->bind_dispatcher(hole);
		}
		else
		{
			if (cachePool->update_mode() || cachePool->is_mem_dirty())
			{
				barHelper = new BarrierUnit(workerThread, iMaxBarrierCount, iMaxKeyCount, BarrierUnit::IN_BACK);
				cachePool->bind_dispatcher(barHelper);
				barHelper->bind_dispatcher(helperUnit);
			}
			else
			{
				cachePool->bind_dispatcher(helperUnit);
			}
		}
	}

	serverControl = TaskControl::get_instance(workerThread);
	if (NULL == serverControl)
	{
		log_crit("create TaskControl object failed, errno[%d], msg[%s]", errno, strerror(errno));
		return -1;
	}
	serverControl->bind_dispatcher(barCache);
	log_debug("bind server control ok");

	multiPlexer = new TaskMultiplexer(workerThread);
	multiPlexer->bind_dispatcher(serverControl);

	agentProcess = new AgentProcess(workerThread);
	agentProcess->bind_dispatcher(multiPlexer);

	agentListener = new AgentListenPool();
	if (agentListener->Bind(gConfig, agentProcess, workerThread) < 0)
		return -1;

	int open_cnt = StatOpenFd();
	gMaxConnCnt = gConfig->get_int_val("cache", "MaxFdCount", 1024) - open_cnt - 10; // reserve 10 fds
	if (gMaxConnCnt < 0)
	{
		log_crit("MaxFdCount should large than %d", open_cnt + 10);
		return -1;
	}

	workerThread->running_thread();
	if (hotbackThread)
	{
		hotbackThread->running_thread();
	}

	return 0;
}

static int DTC_Startup_Muti_Thread()
{
	if (DTC_StartDataSourceThread() < 0)
		return -1;
	if (StartRemoteClientThread() < 0)
		return -1;
	if (0 != StartHotbackThread())
	{
		return -1;
	}
	if (disableCache)
	{
		helperUnit->disable_commit_group();
	}
	else if (DTC_StartCacheThread() < 0)
		return -1;

	if (!disableDataSource)
		helperUnit->Attach(dsThread);

	int iMaxBarrierCount = gConfig->get_int_val("cache", "MaxBarrierCount", 1000);
	int iMaxKeyCount = gConfig->get_int_val("cache", "MaxKeyCount", 1000);

	barCache = new BarrierUnit(cacheThread ?: dsThread, iMaxBarrierCount, iMaxKeyCount, BarrierUnit::IN_FRONT);
	if (disableCache)
	{
		bypassUnit = new BufferBypass(dsThread);
		barCache->bind_dispatcher(bypassUnit);
		bypassUnit->bind_dispatcher(helperUnit);
	}
	else
	{
		keyRoute = new KeyRoute(cacheThread, TableDefinitionManager::Instance()->get_cur_table_def()->key_format());
		if (!check_and_create())
		{
			log_error("check_and_create error");
			return -1;
		}
		else
			log_debug("check_and_create ok");
		std::vector<ClusterNode> clusterConf;
		if (!parse_cluster_config(&clusterConf))
		{
			log_error("parse_cluster_config error");
			return -1;
		}
		else
			log_debug("parse_cluster_config ok");

		keyRoute->Init(clusterConf);
		if (keyRoute->load_node_state_if_any() != 0)
		{
			log_error("key route init error!");
			return -1;
		}
		log_debug("keyRoute->Init ok");
		barCache->bind_dispatcher(keyRoute);
		keyRoute->bind_cache(cachePool);
		keyRoute->bind_remote_helper(remoteClient);
		cachePool->bind_dispatcher_remote(remoteClient);
		cachePool->bind_hb_log_dispatcher(hotbackProcess);
		if (disableDataSource)
		{
			BlackHole *hole = new BlackHole(dsThread);
			cachePool->bind_dispatcher(hole);
		}
		else
		{
			if (cachePool->update_mode() || cachePool->is_mem_dirty())
			{
				barHelper = new BarrierUnit(dsThread, iMaxBarrierCount, iMaxKeyCount, BarrierUnit::IN_BACK);
				cachePool->bind_dispatcher(barHelper);
				barHelper->bind_dispatcher(helperUnit);
			}
			else
			{
				cachePool->bind_dispatcher(helperUnit);
			}
		}
	}

	serverControl = TaskControl::get_instance(cacheThread ?: dsThread);
	if (NULL == serverControl)
	{
		log_crit("create TaskControl object failed, errno[%d], msg[%s]", errno, strerror(errno));
		return -1;
	}
	serverControl->bind_dispatcher(barCache);
	log_debug("bind server control ok");

	multiPlexer = new TaskMultiplexer(cacheThread ?: dsThread);
	multiPlexer->bind_dispatcher(serverControl);

	agentProcess = new AgentProcess(cacheThread ?: dsThread);
	agentProcess->bind_dispatcher(multiPlexer);

	agentListener = new AgentListenPool();
	if (agentListener->Bind(gConfig, agentProcess) < 0)
		return -1;

	int open_cnt = StatOpenFd();
	gMaxConnCnt = gConfig->get_int_val("cache", "MaxFdCount", 1024) - open_cnt - 10; // reserve 10 fds
	if (gMaxConnCnt < 0)
	{
		log_crit("MaxFdCount should large than %d", open_cnt + 10);
		return -1;
	}

	if (dsThread)
	{
		dsThread->running_thread();
	}

	if (remoteThread)
	{
		remoteThread->running_thread();
	}
	if (hotbackThread)
	{
		hotbackThread->running_thread();
	}
	if (cacheThread)
	{
		cacheThread->running_thread();
	}

	agentListener->Run();

	return 0;
}

// second part of entry
static int main2(void *dummy);

int main(int argc, char *argv[])
{
	enable_memchecker();
	init_proc_title(argc, argv);
	mkdir("../stat", 0777);
	mkdir("../data", 0777);
	if (dtc_daemon_init(argc, argv) < 0)
		return -1;

	if (gConfig->get_int_val("cache", "EnableCoreDump", 0))
		daemon_enable_core_dump();

	hashChanging = gConfig->get_int_val("cache", "HashChanging", 0);
	targetNewHash = gConfig->get_int_val("cache", "TargetNewHash", 0);

	DTCGlobal::_pre_alloc_NG_num = gConfig->get_int_val("cache", "PreAllocNGNum", 1024);
	DTCGlobal::_pre_alloc_NG_num = DTCGlobal::_pre_alloc_NG_num <= 1 ? 1 : DTCGlobal::_pre_alloc_NG_num >= (1 << 12) ? 1 : DTCGlobal::_pre_alloc_NG_num;

	DTCGlobal::_min_chunk_size = gConfig->get_int_val("cache", "MinChunkSize", 0);
	if (DTCGlobal::_min_chunk_size < 0)
	{
		DTCGlobal::_min_chunk_size = 0;
	}

	DTCGlobal::_pre_purge_nodes = gConfig->get_int_val("cache", "pre_purge_nodes", 0);
	if (DTCGlobal::_pre_purge_nodes < 0)
	{
		DTCGlobal::_pre_purge_nodes = 0;
	}
	else if (DTCGlobal::_pre_purge_nodes > 10000)
	{
		DTCGlobal::_pre_purge_nodes = 10000;
	}

	RELATIVE_HOUR_CALCULATOR->SetBaseHour(gConfig->get_int_val("cache", "RelativeYear", 2014));

	InitStat();

	log_info("Table %s: key/field# %d/%d, keysize %d",
			 dbConfig->tblName,
			 TableDefinitionManager::Instance()->get_cur_table_def()->key_fields(),
			 TableDefinitionManager::Instance()->get_cur_table_def()->num_fields() + 1,
			 TableDefinitionManager::Instance()->get_cur_table_def()->max_key_size());

	if (InitCacheMode() < 0)
		return -1;

	if (daemon_start() < 0)
		return -1;

	Thread::set_auto_config_instance(gConfig->get_auto_config_instance("cache"));

	_set_remote_log_config_(gConfig->get_str_val("cache", "RemoteLogAddr"),
							gConfig->get_int_val("cache", "RemoteLogPort", 0),
							dtc::utils::get_bid());
	REMOTE_LOG->set_remote_port(gConfig->get_int_val("cache", "RemoteLogPort", 0));
	if (gConfig->get_int_val("cache", "RemoteOpLogOn", 0) != 0)
		REMOTE_LOG->set_op_log_on();

	if (start_watch_dog(main2, NULL) < 0)
		return -1;
	return main2(NULL);
}

static int main2(void *dummy)
{

	Thread *mainThread;
	NEW(Thread("main", Thread::ThreadTypeProcess), mainThread);
	if (mainThread != NULL)
	{
		mainThread->initialize_thread();
	}

	if (daemon_set_fd_limit(gConfig->get_int_val("cache", "MaxFdCount", 0)) < 0)
		return -1;

	//start statistic thread.
	statmgr.start_background_thread();

	int ret = 0;
	int useSingleThread = gConfig->get_int_val("cache", "UseSingleThread", 0);
	if (useSingleThread)
	{
		ret = DTC_Startup_Single_Thread();
	}
	else
	{
		ret = DTC_Startup_Muti_Thread();
	}

	if (ret == 0)
	{
		extern void InitTaskExecutor(const char *, AgentListenPool *, TaskDispatcher<TaskRequest> *);
		InitTaskExecutor(TableDefinitionManager::Instance()->get_cur_table_def()->table_name(), agentListener, serverControl);

		//init plugin
		enablePlugin = gConfig->get_int_val("cache", "EnablePlugin", 0);
		if (enablePlugin)
		{
			if (plugin_start() < 0)
			{
				return -1;
			}
		}

#if MEMCHECK
		log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());

		report_mallinfo();
#endif
		log_info("%s v%s: running...", progname, version);
		daemon_wait();
	}

	log_info("%s v%s: stoppping...", progname, version);
#if MEMCHECK
	log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());
	report_mallinfo();
#endif

	//stop plugin
	if (enablePlugin && initPlugin)
	{
		plugin_stop();
	}

	DELETE(listener);

	if (cacheThread)
	{
		cacheThread->interrupt();
	}
	if (hotbackThread)
	{
		hotbackThread->interrupt();
	}
	if (dsThread)
	{
		dsThread->interrupt();
	}

	if (remoteThread)
	{
		remoteThread->interrupt();
	}

	if (workerThread)
	{
		workerThread->interrupt();
	}

	extern void StopTaskExecutor(void);
	StopTaskExecutor();

	DELETE(cachePool);
	DELETE(helperUnit);
	DELETE(barCache);
	DELETE(keyRoute);
	DELETE(barHelper);
	DELETE(bypassUnit);
	DELETE(hotbackProcess);
	DELETE(remoteClient);
	DELETE(cacheThread);
	DELETE(dsThread);
	DELETE(remoteThread);
	DELETE(workerThread);
	DELETE(hotbackThread);
	statmgr.stop_background_thread();
	log_info("%s v%s: stopped", progname, version);
	daemon_cleanup();
#if MEMCHECK
	dump_non_delete();
	log_debug("memory allocated %lu virtual %lu", count_alloc_size(), count_virtual_size());
#endif
	return ret;
}
