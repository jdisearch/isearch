/*
 * =====================================================================================
 *
 *       Filename:  buffer_process.cc
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
#include <endian.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "packet.h"
#include "log.h"
#include "buffer_process.h"
#include "buffer_flush.h"
#include "mysql_error.h"
#include "sys_malloc.h"
#include "data_chunk.h"
#include "raw_data_process.h"
#include "key_route.h"
#include "buffer_remoteLog.h"
#include "hotback_task.h"
#include "tree_data_process.h"
DTC_USING_NAMESPACE;

extern DTCTableDefinition *gTableDef[];
extern KeyRoute *keyRoute;
extern int hashChanging;
extern int targetNewHash;
extern DTCConfig *gConfig;

inline int BufferProcess::transation_find_node(TaskRequest &Task)
{
	// alreay cleared/zero-ed
	ptrKey = Task.packed_key();
	if (m_pstEmptyNodeFilter != NULL && m_pstEmptyNodeFilter->ISSET(Task.int_key()))
	{
		//Cache.cache_purge(ptrKey);
		m_stNode = Node();
		return nodeStat = NODESTAT_EMPTY;
	}

	int newhash, oldhash;
	if (hashChanging)
	{
		if (targetNewHash)
		{
			oldhash = 0;
			newhash = 1;
		}
		else
		{
			oldhash = 1;
			newhash = 0;
		}

		m_stNode = Cache.cache_find(ptrKey, oldhash);
		if (!m_stNode)
		{
			m_stNode = Cache.cache_find(ptrKey, newhash);
			if (!m_stNode)
				return nodeStat = NODESTAT_MISSING;
		}
		else
		{
			Cache.move_to_new_hash(ptrKey, m_stNode);
		}
	}
	else
	{
		if (targetNewHash)
		{
			m_stNode = Cache.cache_find(ptrKey, 1);
			if (!m_stNode)
				return nodeStat = NODESTAT_MISSING;
		}
		else
		{
			m_stNode = Cache.cache_find(ptrKey, 0);
			if (!m_stNode)
				return nodeStat = NODESTAT_MISSING;
		}
	}

	keyDirty = m_stNode.is_dirty();
	oldRows = Cache.node_rows_count(m_stNode);
	// prepare to decrease empty node count
	nodeEmpty = keyDirty == 0 && oldRows == 0;
	return nodeStat = NODESTAT_PRESENT;
}

inline void BufferProcess::transation_update_lru(bool async, int level)
{
	if (!keyDirty)
	{
		// clear node empty here, because the lru is adjusted
		// it's not a fresh node in EmptyButInCleanList state
		if (async == true)
		{
			m_stNode.set_dirty();
			Cache.inc_dirty_node(1);
			Cache.remove_from_lru(m_stNode);
			Cache.insert2_dirty_lru(m_stNode);
			if (nodeEmpty != 0)
			{
				// empty to non-empty
				Cache.dec_empty_node();
				nodeEmpty = 0;
			}
			lruUpdate = LRU_NONE;
		}
		else
		{
			lruUpdate = level;
		}
	}
}

void BufferProcess::transation_end(void)
{
	int newRows = 0;
	if (!!m_stNode && !keyDirty && !m_stNode.is_dirty())
	{
		newRows = Cache.node_rows_count(m_stNode);
		int nodeEmpty1 = newRows == 0;

		if (lruUpdate > noLRU || nodeEmpty1 != nodeEmpty)
		{
			if (newRows == 0)
			{
				Cache.remove_from_lru(m_stNode);
				Cache.insert2_empty_lru(m_stNode);
				if (nodeEmpty == 0)
				{
					// non-empty to empty
					Cache.inc_empty_node();
					nodeEmpty = 1;
				}
				//Cache.DumpEmptyNodeList();
			}
			else
			{
				Cache.remove_from_lru(m_stNode);
				Cache.insert2_clean_lru(m_stNode);
				if (nodeEmpty != 0)
				{
					// empty to non-empty
					Cache.dec_empty_node();
					nodeEmpty = 0;
				}
			}
		}
	}

	CacheTransation::Free();
}

int BufferProcess::write_lru_hb_log(const char *key)
{
	log_debug("write_lru_hb_log begin");
	if (!hbLogSwitch)
	{
		return 0;
	}
	log_debug("write_lru_hb_log new task");
	TaskRequest *pTask = new TaskRequest;
	if (pTask == NULL)
	{
		log_error("cannot write_hb_log row, new task error, possible memory exhausted\n");
		return -1;
	}

	pTask->set_request_type(TaskTypeWriteLruHbLog);
	HotBackTask &hotbacktask = pTask->get_hot_back_task();
	hotbacktask.set_type(DTCHotBackup::SYNC_LRU);
	hotbacktask.set_flag(DTCHotBackup::NON_VALUE);
	hotbacktask.set_value(NULL, 0);
	DTCValue packeKey = tableDef->packed_key(key);
	hotbacktask.set_packed_key(packeKey.bin.ptr, packeKey.bin.len);
	log_debug(" packed key len:%d, key len:%d,  key :%s", packeKey.bin.len, *(unsigned char *)packeKey.bin.ptr, packeKey.bin.ptr + 1);
	dispatch_hot_back_task(pTask);
	return 0;
}

int BufferProcess::write_hb_log(const char *key, char *pstChunk, unsigned int uiNodeSize, int iType)
{
	if (!hbLogSwitch)
	{
		return 0;
	}
	TaskRequest *pTask = new TaskRequest;
	if (pTask == NULL)
	{
		log_error("cannot write_hb_log row, new task error, possible memory exhausted\n");
		return -1;
	}

	pTask->set_request_type(TaskTypeWriteHbLog);

	HotBackTask &hotbacktask = pTask->get_hot_back_task();
	hotbacktask.set_type(iType);
	DTCValue packeKey;
	if (iType == DTCHotBackup::SYNC_COLEXPAND_CMD)
		packeKey.Set(key);
	else
		packeKey = tableDef->packed_key(key);
	hotbacktask.set_packed_key(packeKey.bin.ptr, packeKey.bin.len);
	log_debug(" packed key len:%d, key len:%d,  key :%s", packeKey.bin.len, *(unsigned char *)packeKey.bin.ptr, packeKey.bin.ptr + 1);
	if (uiNodeSize > 0 && (iType == DTCHotBackup::SYNC_COLEXPAND_CMD || uiNodeSize <= 100))
	{
		hotbacktask.set_flag(DTCHotBackup::HAS_VALUE);
		hotbacktask.set_value(pstChunk, uiNodeSize);
		dispatch_hot_back_task(pTask);
	}
	else
	{
		hotbacktask.set_flag(DTCHotBackup::NON_VALUE);
		hotbacktask.set_value(NULL, 0);
		dispatch_hot_back_task(pTask);
	}

	return 0;
}

int BufferProcess::write_hb_log(const char *key, Node &stNode, int iType)
{
	if (!hbLogSwitch)
	{
		return 0;
	}

	unsigned int uiNodeSize = 0;
	DataChunk *pstChunk = NULL;

	if (!(!stNode) && stNode.vd_handle() != INVALID_HANDLE)
	{
		pstChunk = (DataChunk *)DTCBinMalloc::Instance()->handle_to_ptr(stNode.vd_handle());
		uiNodeSize = pstChunk->node_size();
	}
	return write_hb_log(key, (char *)pstChunk, uiNodeSize, iType);
}

inline int BufferProcess::write_hb_log(TaskRequest &Task, Node &stNode, int iType)
{
	return write_hb_log(Task.packed_key(), stNode, iType);
}

void BufferProcess::purge_node_notify(const char *key, Node node)
{
	if (!node)
		return;

	if (node == m_stNode)
	{
		if (nodeEmpty)
		{
			// purge an empty node! decrease empty counter
			Cache.dec_empty_node();
			nodeEmpty = 0;
		}
		m_stNode = Node::Empty();
	}

	if (write_hb_log(key, node, DTCHotBackup::SYNC_PURGE))
	{
		log_crit("hb: log purge key failed");
	}
}

BufferProcess::BufferProcess(PollThread *p, DTCTableDefinition *tdef, EUpdateMode um)
	: TaskDispatcher<TaskRequest>(p),
	  //	output(p, this),
	  output(p),
	  remoteoutput(p),
	  hblogoutput(p),
	  cacheReply(this),
	  tableDef(tdef),
	  Cache(this),
	  nodbMode(false),
	  fullMode(false),
	  m_bReplaceEmpty(false),
	  noLRU(0),
	  asyncServer(um),
	  updateMode(MODE_SYNC),
	  insertMode(MODE_SYNC),
	  mem_dirty(false),
	  insertOrder(INSERT_ORDER_LAST),
	  nodeSizeLimit(0),
	  nodeRowsLimit(0),
	  nodeEmptyLimit(0),

	  flushReply(this),
	  flushTimer(NULL),
	  nFlushReq(0),
	  mFlushReq(0),
	  maxFlushReq(1),
	  markerInterval(300),
	  minDirtyTime(3600),
	  maxDirtyTime(43200),

	  m_pstEmptyNodeFilter(NULL),
	  //Hot Backup
	  hbLogSwitch(false),
	  hbFeature(NULL),
	  //Hot Backup
	  //BlackList
	  blacklist(0),
	  blacklist_timer(0)

//BlackList
{
	memset((char *)&cacheInfo, 0, sizeof(cacheInfo));

	statGetCount = statmgr.get_item_u32(DTC_GET_COUNT);
	statGetHits = statmgr.get_item_u32(DTC_GET_HITS);
	statInsertCount = statmgr.get_item_u32(DTC_INSERT_COUNT);
	statInsertHits = statmgr.get_item_u32(DTC_INSERT_HITS);
	statUpdateCount = statmgr.get_item_u32(DTC_UPDATE_COUNT);
	statUpdateHits = statmgr.get_item_u32(DTC_UPDATE_HITS);
	statDeleteCount = statmgr.get_item_u32(DTC_DELETE_COUNT);
	statDeleteHits = statmgr.get_item_u32(DTC_DELETE_HITS);
	statPurgeCount = statmgr.get_item_u32(DTC_PURGE_COUNT);

	statDropCount = statmgr.get_item_u32(DTC_DROP_COUNT);
	statDropRows = statmgr.get_item_u32(DTC_DROP_ROWS);
	statFlushCount = statmgr.get_item_u32(DTC_FLUSH_COUNT);
	statFlushRows = statmgr.get_item_u32(DTC_FLUSH_ROWS);
	//statIncSyncStep = statmgr.get_sample(HBP_INC_SYNC_STEP);

	statMaxFlushReq = statmgr.get_item_u32(DTC_MAX_FLUSH_REQ);
	statCurrFlushReq = statmgr.get_item_u32(DTC_CURR_FLUSH_REQ);

	statOldestDirtyTime = statmgr.get_item_u32(DTC_OLDEST_DIRTY_TIME);
	statAsyncFlushCount = statmgr.get_item_u32(DTC_ASYNC_FLUSH_COUNT);

	statExpireCount = statmgr.get_item_u32(DTC_KEY_EXPIRE_USER_COUNT);
	statBufferProcessExpireCount = statmgr.get_item_u32(CACHE_EXPIRE_REQ);

	maxExpireCount = gConfig->get_int_val("cache", "MaxExpireCount", 100);
	maxExpireTime = gConfig->get_int_val("cache", "MaxExpireTime", 3600 * 24 * 30);
}

BufferProcess::~BufferProcess()
{
	if (m_pstEmptyNodeFilter != NULL)
		delete m_pstEmptyNodeFilter;
}

int BufferProcess::set_insert_order(int o)
{
	if (nodbMode == true && o == INSERT_ORDER_PURGE)
	{
		log_error("NoDB server don't support TABLE_DEFINE.ServerOrderInsert = purge");
		return -1;
	}

	if (cacheInfo.syncUpdate == 0 && o == INSERT_ORDER_PURGE)
	{
		log_error("AsyncUpdate server don't support TABLE_DEFINE.ServerOrderInsert = purge");
		return -1;
	}
	insertOrder = o;
	if (pstDataProcess)
		pstDataProcess->set_insert_order(o);
	return 0;
}

int BufferProcess::enable_no_db_mode(void)
{
	if (insertOrder == INSERT_ORDER_PURGE)
	{
		log_error("NoDB server don't support TABLE_DEFINE.ServerOrderInsert = purge");
		return -1;
	}
	if (tableDef->has_auto_increment())
	{
		log_error("NoDB server don't support auto_increment field");
		return -1;
	}
	nodbMode = true;
	fullMode = true;
	return 0;
}

int BufferProcess::disable_lru_update(int level)
{
	if (level > LRU_WRITE)
		level = LRU_WRITE;
	if (level < 0)
		level = 0;
	noLRU = level;
	return 0;
}

int BufferProcess::disable_async_log(int disable)
{
	noAsyncLog = !!disable;
	return 0;
}

int BufferProcess::buffer_set_size(unsigned long cacheSize, unsigned int createVersion)
{
	cacheInfo.Init(tableDef->key_format(), cacheSize, createVersion);
	return 0;
}

/*
 * Function		: cache_open
 * Description	: 打开cache
 * Input			: iIpcKey		共享内存ipc key
 *				  ulNodeTotal_	数据节点总数
 ulBucketTotal	hash桶总数
 ulChunkTotal	chunk节点总数
 ulChunkSize	chunk节点大小(单位:byte)
 * Output		: 
 * Return		: 成功返回0,失败返回-1
 */
int BufferProcess::cache_open(int iIpcKey, int iEnableEmptyFilter, int iEnableAutoDeleteDirtyShm)
{
	cacheInfo.keySize = tableDef->key_format();
	cacheInfo.ipcMemKey = iIpcKey;
	cacheInfo.syncUpdate = !asyncServer;
	cacheInfo.emptyFilter = iEnableEmptyFilter ? 1 : 0;
	cacheInfo.autoDeleteDirtyShm = iEnableAutoDeleteDirtyShm ? 1 : 0;
	cacheInfo.forceUpdateTableConf = gConfig->get_int_val("cache", "ForceUpdateTableConf", 0);

	log_debug("cache_info: \n\tshmkey[%d] \n\tshmsize[" UINT64FMT "] \n\tkeysize[%u]"
			  "\n\tversion[%u] \n\tsyncUpdate[%u] \n\treadonly[%u]"
			  "\n\tcreateonly[%u] \n\tempytfilter[%u] \n\tautodeletedirtysharememory[%u]",
			  cacheInfo.ipcMemKey, cacheInfo.ipcMemSize, cacheInfo.keySize,
			  cacheInfo.version, cacheInfo.syncUpdate, cacheInfo.readOnly,
			  cacheInfo.createOnly, cacheInfo.emptyFilter, cacheInfo.autoDeleteDirtyShm);

	if (Cache.cache_open(&cacheInfo))
	{
		log_error("%s", Cache.Error());
		return -1;
	}

	log_info("Current cache memory format is V%d\n", cacheInfo.version);

	int iMemSyncUpdate = Cache.dirty_lru_empty() ? 1 : 0;
	/*
	 * 1. sync dtc + dirty mem, SYNC + mem_dirty
	 * 2. sync dtc + clean mem, SYNC + !mem_dirty
	 * 3. async dtc + dirty mem/clean mem: ASYNC
	 * disable ASYNC <--> FLUSH switch, so FLUSH never happen forever
	 * updateMode == asyncServer
	 * */
	switch (asyncServer * 0x10000 + iMemSyncUpdate)
	{
	case 0x00000: // sync dtcd + async mem
		mem_dirty = true;
		updateMode = MODE_SYNC;
		break;
	case 0x00001: // sync dtcd + sync mem
		updateMode = MODE_SYNC;
		break;
	case 0x10000: // async dtcd + async mem
		updateMode = MODE_ASYNC;
		break;
	case 0x10001: // async dtcd + sync mem
		updateMode = MODE_ASYNC;
		break;
	default:
		updateMode = cacheInfo.syncUpdate ? MODE_SYNC : MODE_ASYNC;
	}

	if (tableDef->has_auto_increment() == 0 && updateMode == MODE_ASYNC)
		insertMode = MODE_ASYNC;

	log_info("Cache Update Mode: %s",
			 updateMode == MODE_SYNC ? "SYNC" : updateMode == MODE_ASYNC ? "ASYNC" : updateMode == MODE_FLUSH ? "FLUSH" : "<BAD>");

	// 空结点过滤
	const FEATURE_INFO_T *pstFeature;
	pstFeature = Cache.query_feature_by_id(EMPTY_FILTER);
	if (pstFeature != NULL)
	{
		NEW(EmptyNodeFilter, m_pstEmptyNodeFilter);
		if (m_pstEmptyNodeFilter == NULL)
		{
			log_error("new %s error: %m", "EmptyNodeFilter");
			return -1;
		}
		if (m_pstEmptyNodeFilter->Attach(pstFeature->fi_handle) != 0)
		{
			log_error("EmptyNodeFilter attach error: %s", m_pstEmptyNodeFilter->Error());
			return -1;
		}
	}

	Mallocator *pstMalloc = DTCBinMalloc::Instance();
	UpdateMode stUpdateMod = {asyncServer, updateMode, insertMode, insertOrder};
	if (tableDef->index_fields() > 0)
	{
		log_debug("tree index enable, index field num[%d]", tableDef->index_fields());
		pstDataProcess = new TreeDataProcess(pstMalloc, tableDef, &Cache, &stUpdateMod);
		if (pstDataProcess == NULL)
		{
			log_error("create TreeDataProcess error: %m");
			return -1;
		}
	}
	else
	{
		log_debug("%s", "use raw-data mode");
		pstDataProcess = new RawDataProcess(pstMalloc, tableDef, &Cache, &stUpdateMod);
		if (pstDataProcess == NULL)
		{
			log_error("create RawDataProcess error: %m");
			return -1;
		}
		((RawDataProcess *)pstDataProcess)->set_limit_node_size(nodeSizeLimit);
	}

	if (updateMode == MODE_SYNC)
	{
		noAsyncLog = 1;
	}

	// 热备特性
	pstFeature = Cache.query_feature_by_id(HOT_BACKUP);
	if (pstFeature != NULL)
	{
		NEW(HBFeature, hbFeature);
		if (hbFeature == NULL)
		{
			log_error("new hot-backup feature error: %m");
			return -1;
		}
		if (hbFeature->Attach(pstFeature->fi_handle) != 0)
		{
			log_error("hot-backup feature attach error: %s", hbFeature->Error());
			return -1;
		}

		if (hbFeature->master_uptime() != 0)
		{
			//开启变更key日志
			hbLogSwitch = true;
		}
	}
	//Hot Backup

	//DelayPurge
	Cache.start_delay_purge_task(owner->get_timer_list_by_m_seconds(10 /*10 ms*/));

	//Blacklist
	blacklist_timer = owner->get_timer_list(10 * 60); /* 10 min sched*/

	NEW(BlackListUnit(blacklist_timer), blacklist);
	if (NULL == blacklist || blacklist->init_blacklist(100000, tableDef->key_format()))
	{
		log_error("init blacklist failed");
		return -1;
	}

	blacklist->start_blacklist_expired_task();
	//Blacklist

	if (tableDef->expire_time_field_id() != -1)
	{
		if (nodbMode)
		{
			key_expire_timer = owner->get_timer_list_by_m_seconds(1000 /* 1s */);
			NEW(ExpireTime(key_expire_timer, &Cache, pstDataProcess,
						   tableDef, maxExpireCount),
				key_expire);
			if (key_expire == NULL)
			{
				log_error("init key expire time failed");
				return -1;
			}
			key_expire->start_key_expired_task();
		}
		else
		{
			log_error("db mode do not support expire time");
			return -1;
		}
	}

	//Empty Node list
	if (fullMode == true)
	{
		// nodb Mode has not empty nodes,
		nodeEmptyLimit = 0;
		// prune all present empty nodes
		Cache.prune_empty_node_list();
	}
	else if (nodeEmptyLimit)
	{
		// Enable Empty Node Limitation
		Cache.set_empty_node_limit(nodeEmptyLimit);
		// re-counting empty node count
		Cache.init_empty_node_list();
		// upgrade from old memory
		Cache.upgrade_empty_node_list();
		// shrinking empty list
		Cache.shrink_empty_node_list();
	}
	else
	{
		// move all empty node to clean list
		Cache.merge_empty_node_list();
	}
	REMOTE_LOG->set_remote_log_mode(tableDef, nodbMode, insertMode, updateMode);
	//Empty Node list
	return 0;
}

bool BufferProcess::InsertEmptyNode(void)
{
	for (int i = 0; i < 2; i++)
	{
		m_stNode = Cache.cache_allocate(ptrKey);
		if (!(!m_stNode))
			break;

		if (Cache.try_purge_size(1, m_stNode) != 0)
			break;
	}
	if (!m_stNode)
	{
		log_debug("alloc cache node error");
		return false;
	}
	m_stNode.vd_handle() = INVALID_HANDLE;
	// new node created, it's EmptyButInCleanList
	nodeEmpty = 0; // means it's not in empty before transation
	return true;
}

BufferResult BufferProcess::InsertDefaultRow(TaskRequest &Task)
{
	int iRet;
	log_debug("%s", "insert default start!");

	if (!m_stNode)
	{
		//发现空节点
		if (InsertEmptyNode() == false)
		{
			log_warning("alloc cache node error");
			Task.set_error(-EIO, CACHE_SVC, "alloc cache node error");
			return BUFFER_PROCESS_ERROR;
		}
		if (m_pstEmptyNodeFilter)
			m_pstEmptyNodeFilter->CLR(Task.int_key());
	}
	else
	{
		uint32_t uiTotalRows = ((DataChunk *)(DTCBinMalloc::Instance()->handle_to_ptr(m_stNode.vd_handle())))->total_rows();
		if (uiTotalRows != 0)
			return BUFFER_PROCESS_OK;
	}

	RowValue stRowValue(Task.table_definition());
	stRowValue.default_value();

	RawData stDataRows(&g_stSysMalloc, 1);
	iRet = stDataRows.Init(ptrKey);
	if (iRet != 0)
	{
		log_warning("raw data init error: %d, %s", iRet, stDataRows.get_err_msg());
		Task.set_error(-ENOMEM, CACHE_SVC, "new raw-data error");
		Cache.purge_node_everything(ptrKey, m_stNode);
		return BUFFER_PROCESS_ERROR;
	}
	stDataRows.insert_row(stRowValue, false, false);
	iRet = pstDataProcess->replace_data(&m_stNode, &stDataRows);
	if (iRet != 0)
	{
		log_debug("replace data error: %d, %s", iRet, stDataRows.get_err_msg());
		Task.set_error(-ENOMEM, CACHE_SVC, "replace data error");
		/*标记加入黑名单*/
		Task.push_black_list_size(stDataRows.data_size());
		Cache.purge_node_everything(ptrKey, m_stNode);
		return BUFFER_PROCESS_ERROR;
	}

	if (m_stNode.vd_handle() == INVALID_HANDLE)
	{
		log_error("BUG: node[%u] vdhandle=0", m_stNode.node_id());
		Cache.purge_node(Task.packed_key(), m_stNode);
	}

	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_get_data
 * Description	: 处理get请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_get_data(TaskRequest &Task)
{
	int iRet;

	log_debug("buffer_get_data start ");
	transation_find_node(Task);

	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		if (fullMode == false)
		{
			if (Task.flag_no_cache() != 0)
				Task.mark_as_pass_thru();
			return BUFFER_PROCESS_NEXT;
		}
		--statGetHits; // FullCache Missing treat as miss
					   // FullCache Mode: treat as empty & fallthrough
	case NODESTAT_EMPTY:
		++statGetHits;
		//发现空节点，直接构建result
		log_debug("found Empty-Node[%u], response directed", Task.int_key());
		Task.prepare_result();
		Task.set_total_rows(0);
		Task.set_result_hit_flag(HIT_SUCCESS);
		return BUFFER_PROCESS_OK;
	}

	if (nodbMode)
	{
		BufferResult cacheRet = check_and_expire(Task);
		if (cacheRet != BUFFER_PROCESS_NEXT)
			return cacheRet;
	}
	++statGetHits;
	log_debug("[%s:%d]cache hit ", __FILE__, __LINE__);

	transation_update_lru(false, LRU_READ);
	iRet = pstDataProcess->get_data(Task, &m_stNode);
	if (iRet != 0)
	{
		log_error("get_data() failed");
		Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}
	log_debug(" noLRU:%d,LRU_READ:%d", noLRU, LRU_READ);
	// Hot Backup
	if (noLRU < LRU_READ && write_lru_hb_log(Task.packed_key()))
	{
		//为避免错误扩大， 给客户端成功响应
		log_crit("hb: log lru key failed");
	}
	// Hot Bakcup
	Task.set_result_hit_flag(HIT_SUCCESS);
	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_batch_get_data
 * Description	: 处理get请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_batch_get_data(TaskRequest &Task)
{
	int index;
	int iRet;

	log_debug("buffer_batch_get_data start ");

	Task.prepare_result_no_limit();
	for (index = 0; Task.set_batch_cursor(index) >= 0; index++)
	{
		++statGetCount;
		Task.set_result_hit_flag(HIT_INIT);
		transation_find_node(Task);
		switch (nodeStat)
		{
		case NODESTAT_EMPTY:
			++statGetHits;
			Task.done_batch_cursor(index);
			log_debug("[%s:%d]cache empty ", __FILE__, __LINE__);
			break;
		case NODESTAT_MISSING:
			if (fullMode)
				Task.done_batch_cursor(index);
			log_debug("[%s:%d]cache miss ", __FILE__, __LINE__);
			break;
		case NODESTAT_PRESENT:
			++statGetHits;
			log_debug("[%s:%d]cache hit ", __FILE__, __LINE__);

			transation_update_lru(false, LRU_BATCH);
			iRet = pstDataProcess->get_data(Task, &m_stNode);
			if (iRet != 0)
			{
				log_error("get_data() failed");
				Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
				return BUFFER_PROCESS_ERROR;
			}
			Task.done_batch_cursor(index);

			// Hot Backup
			if (noLRU < LRU_BATCH && write_lru_hb_log(Task.packed_key()))
			{
				//为避免错误扩大， 给客户端成功响应
				log_crit("hb: log lru key failed");
			}
			break;
		}
		transation_end();
	}
	// Hot Bakcup
	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_get_rb
 * Description	: 处理Helper的get回读task
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_get_rb(TaskRequest &Task)
{
	log_debug("buffer_get_rb start ");

	Task.prepare_result();
	int iRet = Task.append_result(Task.result);
	if (iRet < 0)
	{
		log_notice("task append_result error: %d", iRet);
		Task.set_error(iRet, CACHE_SVC, "append_result() error");
		return BUFFER_PROCESS_ERROR;
	}
	log_debug("buffer_get_rb success");

	return BUFFER_PROCESS_OK;
}

/* helper执行GET回来后，更新内存数据 */
BufferResult BufferProcess::buffer_replace_result(TaskRequest &Task)
{
	int iRet;
	int oldRows = 0;

	log_debug("cache replace all start!");

	transation_find_node(Task);

	//数据库回来的记录如果是0行则
	// 1. 设置bits 2. 直接构造0行的result响应包
	if (m_pstEmptyNodeFilter != NULL)
	{
		if ((Task.result == NULL || Task.result->total_rows() == 0))
		{
			log_debug("SET Empty-Node[%u]", Task.int_key());
			m_pstEmptyNodeFilter->SET(Task.int_key());
			Cache.cache_purge(ptrKey);
			return BUFFER_PROCESS_OK;
		}
		else
		{
			m_pstEmptyNodeFilter->CLR(Task.int_key());
		}
	}

	if (!m_stNode)
	{
		if (InsertEmptyNode() == false)
			return BUFFER_PROCESS_OK;
	}
	else
	{
		oldRows = Cache.node_rows_count(m_stNode);
	}

	unsigned int uiNodeID = m_stNode.node_id();
	iRet = pstDataProcess->replace_data(Task, &m_stNode);
	if (iRet != 0 || m_stNode.vd_handle() == INVALID_HANDLE)
	{
		if (nodbMode == true)
		{
			/* UNREACHABLE */
			log_info("cache replace data error: %d. node: %u", iRet, uiNodeID);
			Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
			return BUFFER_PROCESS_ERROR;
		}
		log_debug("cache replace data error: %d. purge node: %u", iRet, uiNodeID);
		Cache.purge_node_everything(ptrKey, m_stNode);
		Cache.inc_dirty_row(0 - oldRows);
		return BUFFER_PROCESS_OK;
	}
	Cache.inc_total_row(pstDataProcess->rows_inc());

	transation_update_lru(false, LRU_READ);
	if (oldRows != 0 || Cache.node_rows_count(m_stNode) != 0)
	{
		// Hot Backup
		if (noLRU < LRU_READ && write_lru_hb_log(Task.packed_key()))
		{
			//为避免错误扩大， 给客户端成功响应
			log_crit("hb: log lru key failed");
		}
		// Hot Bakcup
	}

	log_debug("buffer_replace_result success! ");

	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_flush_data
 * Description	: 处理flush请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_flush_data_before_delete(TaskRequest &Task)
{
	log_debug("%s", "flush start!");
	transation_find_node(Task);
	if (!m_stNode || !(m_stNode.is_dirty()))
	{
		log_debug("node is null or node is clean,return BUFFER_PROCESS_OK");
		return BUFFER_PROCESS_OK;
	}
	unsigned int uiFlushRowsCnt;

	Node node = m_stNode;
	int iRet = 0;

	/*init*/
	keyDirty = m_stNode.is_dirty();

	DTCFlushRequest *flushReq = new DTCFlushRequest(this, ptrKey);
	if (flushReq == NULL)
	{
		log_error("new DTCFlushRequest error: %m");
		return BUFFER_PROCESS_ERROR;
	}

	iRet = pstDataProcess->flush_data(flushReq, &m_stNode, uiFlushRowsCnt);
	if (iRet != 0)
	{
		log_error("flush_data error:%d", iRet);
		return BUFFER_PROCESS_ERROR;
	}
	if (uiFlushRowsCnt == 0)
	{
		delete flushReq;
		if (keyDirty)
			Cache.inc_dirty_node(-1);
		m_stNode.clr_dirty();
		Cache.remove_from_lru(m_stNode);
		Cache.insert2_clean_lru(m_stNode);
		return BUFFER_PROCESS_OK;
	}
	else
	{
		commit_flush_request(flushReq, NULL);
		Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());
		if (keyDirty)
			Cache.inc_dirty_node(-1);
		m_stNode.clr_dirty();
		Cache.remove_from_lru(m_stNode);
		Cache.insert2_clean_lru(m_stNode);
		++statFlushCount;
		statFlushRows += uiFlushRowsCnt;
		return BUFFER_PROCESS_OK;
	}
}

/*
 * Function		: buffer_flush_data
 * Description	: 处理flush请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_flush_data(TaskRequest &Task)
{

	log_debug("%s", "flush start!");
	transation_find_node(Task);
	if (!m_stNode || !(m_stNode.is_dirty()))
		return BUFFER_PROCESS_OK;

	unsigned int uiFlushRowsCnt;

	BufferResult iRet = buffer_flush_data(m_stNode, &Task, uiFlushRowsCnt);
	if (iRet == BUFFER_PROCESS_OK)
	{
		++statFlushCount;
		statFlushRows += uiFlushRowsCnt;
	}
	return (iRet);
}

/*called by flush next node*/
int BufferProcess::buffer_flush_data_timer(Node &stNode, unsigned int &uiFlushRowsCnt)
{
	int iRet, err = 0;

	/*init*/
	transation_begin(NULL);
	keyDirty = stNode.is_dirty();
	ptrKey = ((DataChunk *)(DTCBinMalloc::Instance()->handle_to_ptr(stNode.vd_handle())))->Key();

	DTCFlushRequest *flushReq = new DTCFlushRequest(this, ptrKey);
	if (flushReq == NULL)
	{
		log_error("new DTCFlushRequest error: %m");
		err = -1;
		goto __out;
	}

	iRet = pstDataProcess->flush_data(flushReq, &stNode, uiFlushRowsCnt);

	if (uiFlushRowsCnt == 0)
	{
		delete flushReq;
		if (iRet < 0)
		{
			err = -2;
			goto __out;
		}
		else
		{
			if (keyDirty)
				Cache.inc_dirty_node(-1);
			stNode.clr_dirty();
			Cache.remove_from_lru(stNode);
			Cache.insert2_clean_lru(stNode);
			err = 1;
			goto __out;
		}
	}
	else
	{
		commit_flush_request(flushReq, NULL);
		Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());
		if (iRet == 0)
		{
			if (keyDirty)
				Cache.inc_dirty_node(-1);
			stNode.clr_dirty();
			Cache.remove_from_lru(stNode);
			Cache.insert2_clean_lru(stNode);
			err = 2;
			goto __out;
		}
		else
		{
			err = -5;
			goto __out;
		}
	}

__out:
	/*clear init*/
	CacheTransation::Free();
	return err;
}
/*
 * Function		: buffer_flush_data
 * Description	: 处理flush请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_flush_data(Node &stNode, TaskRequest *pstTask, unsigned int &uiFlushRowsCnt)
{
	int iRet;

	/*could called by flush timer event, no transationFindNode called there, can't trust KeyDirty, recal it*/
	keyDirty = stNode.is_dirty();

	log_debug("%s", "flush node start!");

	int flushCnt = 0;
	DTCFlushRequest *flushReq = NULL;
	if (!nodbMode)
	{
		flushReq = new DTCFlushRequest(this, ptrKey);
		if (flushReq == NULL)
		{
			log_error("new DTCFlushRequest error: %m");
			if (pstTask != NULL)
				pstTask->set_error(-ENOMEM, CACHE_SVC, "new DTCFlushRequest error");
			return BUFFER_PROCESS_ERROR;
		}
	}

	iRet = pstDataProcess->flush_data(flushReq, &stNode, uiFlushRowsCnt);

	if (flushReq)
	{
		flushCnt = flushReq->numReq;
		commit_flush_request(flushReq, pstTask);
		if (iRet != 0)
		{
			log_error("flush_data() failed while flush data");
			if (pstTask != NULL)
				pstTask->set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());

			return BUFFER_PROCESS_ERROR;
		}
	}

	Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());

	if (keyDirty)
		Cache.inc_dirty_node(-1);

	stNode.clr_dirty();
	keyDirty = 0;
	transation_update_lru(false, LRU_ALWAYS);

	log_debug("buffer_flush_data success");
	if (flushCnt == 0)
		return BUFFER_PROCESS_OK;
	else
		return BUFFER_PROCESS_PENDING;
}

/*
 * Function		: buffer_purge_data
 * Description	: 处理purge请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_purge_data(TaskRequest &Task)
{
	transation_find_node(Task);

	switch (nodeStat)
	{
	case NODESTAT_EMPTY:
		m_pstEmptyNodeFilter->CLR(Task.int_key());
		return BUFFER_PROCESS_OK;

	case NODESTAT_MISSING:
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		break;
	}

	BufferResult iRet = BUFFER_PROCESS_OK;
	if (updateMode && m_stNode.is_dirty())
	{
		unsigned int uiFlushRowsCnt;
		iRet = buffer_flush_data(m_stNode, &Task, uiFlushRowsCnt);
		if (iRet != BUFFER_PROCESS_PENDING)
			return iRet;
	}

	++statDropCount;
	statDropRows += ((DataChunk *)(DTCBinMalloc::Instance()->handle_to_ptr(m_stNode.vd_handle())))->total_rows();
	Cache.inc_total_row(0LL - ((DataChunk *)(DTCBinMalloc::Instance()->handle_to_ptr(m_stNode.vd_handle())))->total_rows());

	unsigned int uiNodeID = m_stNode.node_id();
	if (Cache.cache_purge(ptrKey) != 0)
	{
		log_error("PANIC: purge node[id=%u] fail", uiNodeID);
	}

	return iRet;
}

/*
 * Function		: buffer_update_rows
 * Description	: 处理Helper的update task
 * Input		: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_update_rows(TaskRequest &Task, bool async, bool setrows)
{
	int iRet;

	log_debug("cache update data start! ");

	if (m_bReplaceEmpty == true)
	{
		BufferResult ret = InsertDefaultRow(Task);
		if (ret != BUFFER_PROCESS_OK)
			return (ret);
	}

	int rows = Cache.node_rows_count(m_stNode);
	iRet = pstDataProcess->update_data(Task, &m_stNode, pstLogRows, async, setrows);
	if (iRet != 0)
	{
		if (async == false && !Task.flag_black_hole())
		{
			Cache.purge_node_everything(ptrKey, m_stNode);
			Cache.inc_total_row(0LL - rows);
			return BUFFER_PROCESS_OK;
		}
		log_warning("update_data() failed: %d,%s", iRet, pstDataProcess->get_err_msg());
		Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
		transation_update_lru(async, LRU_ALWAYS);
		goto ERR_RETURN;
	}
	/*if update volatile field,node won't be dirty*/
	transation_update_lru((Task.resultInfo.affected_rows() > 0 &&
						   (Task.request_operation() && Task.request_operation()->has_type_commit()) //has core field modified
						   )
							  ? async
							  : false,
						  LRU_WRITE);

	Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());

	// Hot Backup
	if (nodeStat != NODESTAT_PRESENT ||
		(Task.request_operation() && Task.request_operation()->has_type_commit()))
	{
		// only write log if some non-volatile field got updated
		// or cache miss and m_bReplaceEmpty is set (equiv insert(default)+update)
		if (write_hb_log(Task, m_stNode, DTCHotBackup::SYNC_UPDATE))
		{
			//为避免错误扩大， 给客户端成功响应
			log_crit("hb: log update key failed");
		}
	}
	// Hot Bakcup

	return BUFFER_PROCESS_OK;

ERR_RETURN:
	return BUFFER_PROCESS_ERROR;
}

/* buffer_replace_rows don't allow empty stNode */
BufferResult BufferProcess::buffer_replace_rows(TaskRequest &Task, bool async, bool setrows)
{
	int iRet;

	log_debug("cache replace rows start!");

	int rows = Cache.node_rows_count(m_stNode);
	iRet = pstDataProcess->replace_rows(Task, &m_stNode, pstLogRows, async, setrows);
	if (iRet != 0)
	{
		if (keyDirty == false && !Task.flag_black_hole())
		{
			Cache.purge_node_everything(ptrKey, m_stNode);
			Cache.inc_total_row(0LL - rows);
		}

		/* 如果是同步replace命令，返回成功*/
		if (async == false && !Task.flag_black_hole())
			return BUFFER_PROCESS_OK;

		log_error("cache replace rows error: %d,%s", iRet, pstDataProcess->get_err_msg());
		Task.set_error(-EIO, CACHE_SVC, "replace_data() error");
		return BUFFER_PROCESS_ERROR;
	}
	Cache.inc_total_row(pstDataProcess->rows_inc());
	Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());

	BufferResult ret = BUFFER_PROCESS_OK;

	transation_update_lru(async, LRU_WRITE);

	// Hot Backup
	if (write_hb_log(Task, m_stNode, DTCHotBackup::SYNC_UPDATE))
	//    if(hbLog.write_update_key(Task.packed_key(), DTCHotBackup::SYNC_UPDATE))
	{
		//为避免错误扩大， 给客户端成功响应
		log_crit("hb: log update key failed");
	}
	// Hot Bakcup

	log_debug("buffer_replace_rows success! ");

	if (m_stNode.vd_handle() == INVALID_HANDLE)
	{
		log_error("BUG: node[%u] vdhandle=0", m_stNode.node_id());
		Cache.purge_node(Task.packed_key(), m_stNode);
		Cache.inc_total_row(0LL - rows);
	}

	return ret;
}

/*
 * Function	: buffer_insert_row
 * Description	: 处理Helper的insert task
 * Input		: Task			请求信息
 * Output	: Task			返回信息
 * Return	: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_insert_row(TaskRequest &Task, bool async, bool setrows)
{
	int iRet;
	bool emptyFlag = false;

	if (!m_stNode)
	{
		emptyFlag = true;
		if (InsertEmptyNode() == false)
		{
			if (async == true || Task.flag_black_hole())
			{
				Task.set_error(-EIO, CACHE_SVC, "allocate_node Error while insert row");
				return BUFFER_PROCESS_ERROR;
			}
			return BUFFER_PROCESS_OK;
		}

		RawData stDataRows(&g_stSysMalloc, 1);
		//iRet = stDataRows.Init(0, Task.table_definition()->key_format(), ptrKey);
		iRet = stDataRows.Init(ptrKey);
		if (iRet != 0)
		{
			log_warning("raw data init error: %d, %s", iRet, stDataRows.get_err_msg());
			Task.set_error(-ENOMEM, CACHE_SVC, "new raw-data error");
			Cache.purge_node_everything(ptrKey, m_stNode);
			return BUFFER_PROCESS_ERROR;
		}
		iRet = pstDataProcess->replace_data(&m_stNode, &stDataRows);
		if (iRet != 0)
		{
			log_warning("raw data init error: %d, %s", iRet, stDataRows.get_err_msg());
			Task.set_error(-ENOMEM, CACHE_SVC, "new raw-data error");
			Cache.purge_node_everything(ptrKey, m_stNode);
			return BUFFER_PROCESS_ERROR;
		}

		if (m_pstEmptyNodeFilter)
			m_pstEmptyNodeFilter->CLR(Task.int_key());
	}

	int oldRows = Cache.node_rows_count(m_stNode);
	iRet = pstDataProcess->append_data(Task, &m_stNode, pstLogRows, async, setrows);
	if (iRet == -1062)
	{
		Task.set_error(-ER_DUP_ENTRY, CACHE_SVC, "duplicate unique key detected");
		return BUFFER_PROCESS_ERROR;
	}
	else if (iRet != 0)
	{
		if ((async == false && !Task.flag_black_hole()) || emptyFlag)
		{
			log_debug("append_data() failed, purge now [%d %s]", iRet, pstDataProcess->get_err_msg());
			Cache.inc_total_row(0LL - oldRows);
			Cache.purge_node_everything(ptrKey, m_stNode);
			return BUFFER_PROCESS_OK;
		}
		else
		{
			log_error("append_data() failed while update data");
			Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
			return BUFFER_PROCESS_ERROR;
		}
	}
	transation_update_lru(async, LRU_WRITE);

	Cache.inc_total_row(pstDataProcess->rows_inc());
	if (async == true)
		Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());

	// Hot Backup
	if (write_hb_log(Task, m_stNode, DTCHotBackup::SYNC_INSERT))
	//    if(hbLog.write_update_key(Task.packed_key(), DTCHotBackup::SYNC_INSERT))
	{
		//为避免错误扩大， 给客户端成功响应
		log_crit("hb: log update key failed");
	}
	// Hot Bakcup

	log_debug("buffer_insert_row success");
	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_delete_rows
 * Description	: 处理del请求
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 成功返回0,失败返回-1
 */
BufferResult BufferProcess::buffer_delete_rows(TaskRequest &Task)
{
	int iRet;

	log_debug("buffer_delete_rows start! ");

	uint32_t oldRows = Cache.node_rows_count(m_stNode);

	int all_row_delete = Task.all_rows();

	if (Task.all_rows() != 0)
	{ //如果没有del条件则删除整个节点
	empty:
		if (lossyMode || Task.flag_black_hole())
		{
			Task.resultInfo.set_affected_rows(oldRows);
		}

		/*row cnt statistic dec by 1*/
		Cache.inc_total_row(0LL - oldRows);

		/*dirty node cnt staticstic dec by 1*/
		if (keyDirty)
		{
			Cache.inc_dirty_node(-1);
		}

		/* dirty row cnt statistic dec, if count dirty row error, let statistic wrong with it*/
		if (all_row_delete)
		{
			int old_dirty_rows = pstDataProcess->dirty_rows_in_node(Task, &m_stNode);
			if (old_dirty_rows > 0)
				Cache.inc_dirty_row(old_dirty_rows);
		}
		else
		{
			Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());
		}

		Cache.purge_node_everything(ptrKey, m_stNode);
		if (m_pstEmptyNodeFilter)
			m_pstEmptyNodeFilter->SET(Task.int_key());

		// Hot Backup
		Node stEmpytNode;
		if (write_hb_log(Task, stEmpytNode, DTCHotBackup::SYNC_PURGE))
		//		if(hbLog.write_update_key(Task.packed_key(), DTCHotBackup::SYNC_UPDATE))
		{
			//为避免错误扩大， 给客户端成功响应
			log_crit("hb: log update key failed");
		}
		// Hot Bakcup

		return BUFFER_PROCESS_OK;
	}

	/*delete error handle is too simple, statistic can not trust if error happen here*/
	iRet = pstDataProcess->delete_data(Task, &m_stNode, pstLogRows);
	if (iRet != 0)
	{
		log_error("delete_data() failed: %d,%s", iRet, pstDataProcess->get_err_msg());
		Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
		if (!keyDirty)
		{
			Cache.inc_total_row(0LL - oldRows);
			Cache.purge_node_everything(ptrKey, m_stNode);
		}
		return BUFFER_PROCESS_ERROR;
	}

	/* Delete to empty */
	uint32_t uiTotalRows = ((DataChunk *)(DTCBinMalloc::Instance()->handle_to_ptr(m_stNode.vd_handle())))->total_rows();
	if (uiTotalRows == 0)
		goto empty;

	Cache.inc_dirty_row(pstDataProcess->dirty_rows_inc());
	Cache.inc_total_row(pstDataProcess->rows_inc());

	transation_update_lru(false, LRU_WRITE);

	// Hot Backup
	if (write_hb_log(Task, m_stNode, DTCHotBackup::SYNC_DELETE))
	{
		//为避免错误扩大， 给客户端成功响应
		log_crit("hb: log update key failed");
	}
	// Hot Bakcup

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::check_allowed_insert(TaskRequest &Task)
{
	int rows = Cache.node_rows_count(m_stNode);
	// single rows checker
	if (tableDef->key_as_uniq_field() && rows != 0)
	{
		Task.set_error(-ER_DUP_ENTRY, CACHE_SVC, "duplicate unique key detected");
		return BUFFER_PROCESS_ERROR;
	}
	if (nodeRowsLimit > 0 && rows >= nodeRowsLimit)
	{
		/* check weather allowed execute insert operation*/
		Task.set_error(-EC_NOT_ALLOWED_INSERT, __FUNCTION__,
					   "rows exceed limit, not allowed insert any more data");
		return BUFFER_PROCESS_ERROR;
	}
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_sync_insert_precheck(TaskRequest &Task)
{
	log_debug("%s", "buffer_sync_insert begin");
	if (m_bReplaceEmpty == true)
	{ // 这种模式下，不支持insert操作
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "insert cmd from client, not support under replace mode");
		log_notice("insert cmd from client, not support under replace mode");
		return BUFFER_PROCESS_ERROR;
	}

	if (tableDef->key_as_uniq_field() || nodeRowsLimit > 0)
	{
		transation_find_node(Task);

		// single rows checker
		if (nodeStat == NODESTAT_PRESENT && check_allowed_insert(Task) == BUFFER_PROCESS_ERROR)
			return BUFFER_PROCESS_ERROR;
	}

	return BUFFER_PROCESS_NEXT;
}

BufferResult BufferProcess::buffer_sync_insert(TaskRequest &Task)
{
	log_debug("%s", "buffer_sync_insert begin");
	if (m_bReplaceEmpty == true)
	{ // 这种模式下，不支持insert操作
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "insert cmd from client, not support under replace mode");
		log_notice("insert cmd from client, not support under replace mode");
		return BUFFER_PROCESS_ERROR;
	}

	if (Task.resultInfo.insert_id() > 0)
		Task.update_packed_key(Task.resultInfo.insert_id()); // 如果自增量字段是key，则会更新key

	transation_find_node(Task);

	// Missing is NO-OP, otherwise insert it
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		return BUFFER_PROCESS_OK;

	case NODESTAT_EMPTY:
	case NODESTAT_PRESENT:
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		break;
	}

	return buffer_insert_row(Task, false /* async */, lossyMode /* setrows */);
}

BufferResult BufferProcess::buffer_sync_update(TaskRequest &Task)
{
	bool setrows = lossyMode;
	log_debug("%s", "buffer_sync_update begin");
	// NOOP sync update
	if (Task.request_operation() == NULL)
	{
		// no field need to update
		return BUFFER_PROCESS_OK; //如果helper更新的纪录数为0则直接返回
	}
	else if (setrows == false && Task.resultInfo.affected_rows() == 0)
	{
		if (Task.request_operation()->has_type_commit() == 0)
		{
			// pure volatile update, ignore upstream affected-rows
			setrows = true;
		}
		else if (Task.request_condition() && Task.request_condition()->has_type_timestamp())
		{
			// update base timestamp fields, ignore upstream affected-rows
			setrows = true;
		}
		else
		{
			log_debug("%s", "helper's affected rows is zero");
			return BUFFER_PROCESS_OK; //如果helper更新的纪录数为0则直接返回
		}
	}

	transation_find_node(Task);

	// Missing or Empty is NO-OP except EmptyAsDefault logical
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		return BUFFER_PROCESS_OK;

	case NODESTAT_EMPTY:
		if (m_bReplaceEmpty == true)
			break;
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		break;
	}

	return buffer_update_rows(Task, false /*Async*/, setrows);
}

BufferResult BufferProcess::buffer_sync_replace(TaskRequest &Task)
{
	const int setrows = lossyMode;
	log_debug("%s", "buffer_sync_replace begin");
	// NOOP sync update
	if (lossyMode == false && Task.resultInfo.affected_rows() == 0)
	{
		log_debug("%s", "helper's affected rows is zero");
		return BUFFER_PROCESS_OK; //如果helper更新的纪录数为0则直接返回
	}

	transation_find_node(Task);

	// missing node is NO-OP, empty node insert it, otherwise replace it
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		return BUFFER_PROCESS_OK;

	case NODESTAT_EMPTY:
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		return buffer_insert_row(Task, false, setrows);

	case NODESTAT_PRESENT:
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		break;
	}

	return buffer_replace_rows(Task, false, lossyMode);
}

BufferResult BufferProcess::buffer_sync_delete(TaskRequest &Task)
{
	log_debug("%s", "buffer_sync_delete begin");
	// didn't check zero affected_rows
	transation_find_node(Task);

	// missing and empty is NO-OP, otherwise delete it
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		return BUFFER_PROCESS_OK;
	case NODESTAT_EMPTY:
		if (lossyMode)
		{
			Task.set_error(0, NULL, NULL);
			Task.resultInfo.set_affected_rows(0);
		}
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		break;
	}

	return buffer_delete_rows(Task);
}

BufferResult BufferProcess::buffer_nodb_insert(TaskRequest &Task)
{
	BufferResult iRet;
	log_debug("%s", "buffer_asyn_prepare_insert begin");
	if (m_bReplaceEmpty == true)
	{ // 这种模式下，不支持insert操作
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "insert cmd from client, not support under replace mode");
		log_notice("insert cmd from client, not support under replace mode");
		return BUFFER_PROCESS_ERROR;
	}

	transation_find_node(Task);
	if (nodeStat == NODESTAT_PRESENT)
	{
		iRet = check_and_expire(Task);
		if (iRet == BUFFER_PROCESS_ERROR)
		{
			return iRet;
		}
		else if (iRet == BUFFER_PROCESS_OK)
		{
			nodeStat = NODESTAT_MISSING;
			m_stNode = Node();
		}
	}
	if (nodeStat == NODESTAT_PRESENT && check_allowed_insert(Task) == BUFFER_PROCESS_ERROR)
		return BUFFER_PROCESS_ERROR;

	// update key expire time
	if (Task.request_operation() && Task.update_key_expire_time(maxExpireTime) != 0)
	{
		Task.set_error(-EC_BAD_INVALID_FIELD, CACHE_SVC, "key expire time illegal");
		return BUFFER_PROCESS_ERROR;
	}

	return buffer_insert_row(Task, false /* async */, true /* setrows */);
}

BufferResult BufferProcess::buffer_nodb_update(TaskRequest &Task)
{
	log_debug("%s", "buffer_fullmode_prepare_update begin");

	transation_find_node(Task);

	// missing & empty is NO-OP, except EmptyAsDefault logical
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
	case NODESTAT_EMPTY:
		if (m_bReplaceEmpty == true)
			break;
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		break;
	}

	BufferResult cacheRet = check_and_expire(Task);
	if (cacheRet != BUFFER_PROCESS_NEXT)
		return cacheRet;
	// update key expire time
	if (Task.request_operation() && Task.update_key_expire_time(maxExpireTime) != 0)
	{
		Task.set_error(-EC_BAD_INVALID_FIELD, CACHE_SVC, "key expire time illegal");
		return BUFFER_PROCESS_ERROR;
	}

	return buffer_update_rows(Task, false /*Async*/, true /*setrows*/);
}

BufferResult BufferProcess::buffer_nodb_replace(TaskRequest &Task)
{
	log_debug("%s", "buffer_asyn_prepare_replace begin");
	transation_find_node(Task);

	// update key expire time
	if (Task.request_operation() && Task.update_key_expire_time(maxExpireTime) != 0)
	{
		Task.set_error(-EC_BAD_INVALID_FIELD, CACHE_SVC, "key expire time illegal");
		return BUFFER_PROCESS_ERROR;
	}
	// missing & empty insert it, otherwise replace it
	switch (nodeStat)
	{
	case NODESTAT_EMPTY:
	case NODESTAT_MISSING:
		return buffer_insert_row(Task, false, true /* setrows */);

	case NODESTAT_PRESENT:
		break;
	}

	BufferResult cacheRet = check_and_expire(Task);
	if (cacheRet == BUFFER_PROCESS_ERROR)
	{
		return cacheRet;
	}
	else if (cacheRet == BUFFER_PROCESS_OK)
	{
		nodeStat = NODESTAT_MISSING;
		m_stNode = Node();
		return buffer_insert_row(Task, false, true /* setrows */);
	}

	return buffer_replace_rows(Task, false, true);
}

BufferResult BufferProcess::buffer_nodb_delete(TaskRequest &Task)
{
	log_debug("%s", "buffer_fullmode_delete begin");
	transation_find_node(Task);

	// missing & empty is NO-OP
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
	case NODESTAT_EMPTY:
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		break;
	}

	return buffer_delete_rows(Task);
}

BufferResult BufferProcess::buffer_async_insert(TaskRequest &Task)
{
	log_debug("%s", "buffer_async_insert begin");
	if (m_bReplaceEmpty == true)
	{ // 这种模式下，不支持insert操作
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "insert cmd from client, not support under replace mode");
		log_notice("insert cmd from client, not support under replace mode");
		return BUFFER_PROCESS_ERROR;
	}

	transation_find_node(Task);
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		if (fullMode == false)
			return BUFFER_PROCESS_NEXT;
		if (updateMode == MODE_FLUSH)
			return BUFFER_PROCESS_NEXT;
		break;

	case NODESTAT_EMPTY:
		if (updateMode == MODE_FLUSH)
			return BUFFER_PROCESS_NEXT;
		break;

	case NODESTAT_PRESENT:
		if (check_allowed_insert(Task) == BUFFER_PROCESS_ERROR)
			return BUFFER_PROCESS_ERROR;
		if (updateMode == MODE_FLUSH && !(m_stNode.is_dirty()))
			return BUFFER_PROCESS_NEXT;
		break;
	}

	log_debug("%s", "buffer_async_insert data begin");
	//对insert 操作命中数据进行采样
	++statInsertHits;

	return buffer_insert_row(Task, true /* async */, true /* setrows */);
}

BufferResult BufferProcess::buffer_async_update(TaskRequest &Task)
{
	log_debug("%s", "buffer_asyn_update begin");

	transation_find_node(Task);
	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		if (fullMode == false)
			return BUFFER_PROCESS_NEXT;
		// FALLTHROUGH
	case NODESTAT_EMPTY:
		if (m_bReplaceEmpty == true)
		{
			if (updateMode == MODE_FLUSH)
				return BUFFER_PROCESS_NEXT;
			break;
		}
		return BUFFER_PROCESS_OK;

	case NODESTAT_PRESENT:
		if (updateMode == MODE_FLUSH && !(m_stNode.is_dirty()))
			return BUFFER_PROCESS_NEXT;
		break;
	}

	log_debug("%s", "buffer_async_update update data begin");
	//对update 操作命中数据进行采样
	++statUpdateHits;

	return buffer_update_rows(Task, true /*Async*/, true /*setrows*/);
}

BufferResult BufferProcess::buffer_async_replace(TaskRequest &Task)
{
	log_debug("%s", "buffer_asyn_prepare_replace begin");
	transation_find_node(Task);

	switch (nodeStat)
	{
	case NODESTAT_MISSING:
		if (fullMode == false)
			return BUFFER_PROCESS_NEXT;
		if (updateMode == MODE_FLUSH)
			return BUFFER_PROCESS_NEXT;
		if (tableDef->key_as_uniq_field() == false)
			return BUFFER_PROCESS_NEXT;
		return buffer_insert_row(Task, true, true);

	case NODESTAT_EMPTY:
		if (updateMode == MODE_FLUSH)
			return BUFFER_PROCESS_NEXT;
		return buffer_insert_row(Task, true, true);

	case NODESTAT_PRESENT:
		if (updateMode == MODE_FLUSH && !(m_stNode.is_dirty()))
			return BUFFER_PROCESS_NEXT;
		break;
	}

	return buffer_replace_rows(Task, true, true);
}

/*
 * Function		: buffer_process_request
 * Description	: 处理incoming task
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 0 			成功
 *				: -1			失败
 */
BufferResult BufferProcess::buffer_process_request(TaskRequest &Task)
{
	Task.renew_timestamp();
	szErrMsg[0] = 0;

	Task.field_type(0);

	/* 取命令字 */
	int iCmd = Task.request_code();
	log_debug("BufferProcess::buffer_process_request cmd is %d ", iCmd);
	switch (iCmd)
	{
	case DRequest::Get:
		Task.set_result_hit_flag(HIT_INIT); //set hit flag init status
		if (Task.count_only() && (Task.requestInfo.limit_start() || Task.requestInfo.limit_count()))
		{
			Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "There's nothing to limit because no fields required");
			return BUFFER_PROCESS_ERROR;
		}

		/* 如果命中黑名单，则purge掉当前节点，走PassThru模式 */
		if (blacklist->in_blacklist(Task.packed_key()))
		{
			/* 
				 * 理论上是在黑名单的节点是不可能在cache中的
				 * 为了防止异常，预purge。
				 */
			log_debug("blacklist hit, passthough to datasource");
			buffer_purge_data(Task);
			Task.mark_as_pass_thru();
			return BUFFER_PROCESS_NEXT;
		}

		log_debug("blacklist miss, normal process");

		++statGetCount;

		return buffer_get_data(Task);

	case DRequest::Insert:
		++statInsertCount;

		if (updateMode == MODE_ASYNC && insertMode != MODE_SYNC)
			return buffer_async_insert(Task);

		//标示task将提交给helper
		return buffer_sync_insert_precheck(Task);

	case DRequest::Update:
		++statUpdateCount;

		if (updateMode)
			return buffer_async_update(Task);

		//标示task将提交给helper
		return BUFFER_PROCESS_NEXT;

		//如果clinet 上送Delete 操作，删除Cache中数据，同时提交Helper
		//现阶段异步Cache暂时不支持Delete操作
	case DRequest::Delete:
		if (updateMode != MODE_SYNC)
		{
			if (Task.request_condition() && Task.request_condition()->has_type_rw())
			{
				Task.set_error(-EC_BAD_ASYNC_CMD, CACHE_SVC, "Delete base non ReadOnly fields");
				return BUFFER_PROCESS_ERROR;
			}
			//异步delete前先flush
			BufferResult iRet = BUFFER_PROCESS_OK;
			iRet = buffer_flush_data_before_delete(Task);
			if (iRet == BUFFER_PROCESS_ERROR)
				return iRet;
		}

		//对于delete操作，直接提交DB，不改变原有逻辑
		++statDeleteCount;

		//标示task将提交给helper
		return BUFFER_PROCESS_NEXT;

	case DRequest::Purge:
		//删除指定key在cache中的数据
		++statPurgeCount;
		return buffer_purge_data(Task);

	case DRequest::Flush:
		if (updateMode)
			//flush指定key在cache中的数据
			return buffer_flush_data(Task);
		else
			return BUFFER_PROCESS_OK;

	case DRequest::Replace: //如果是淘汰的数据，不作处理
		++statUpdateCount;

		// 限制key字段作为唯一字段才能使用replace命令
		if (!(Task.table_definition()->key_part_of_uniq_field()) || Task.table_definition()->has_auto_increment())
		{
			Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "replace cmd require key fields part of uniq-fields and no auto-increment field");
			return BUFFER_PROCESS_ERROR;
		}

		if (updateMode)
			return buffer_async_replace(Task);

		//标示task将提交给helper
		return BUFFER_PROCESS_NEXT;

	case DRequest::SvrAdmin:
		return buffer_process_admin(Task);

	default:
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "invalid cmd from client");
		log_notice("invalid cmd[%d] from client", iCmd);
		break;
	} //end of switch

	return BUFFER_PROCESS_ERROR;
}

/*
 * Function		: buffer_process_batch
 * Description	: 处理incoming batch task
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 0 			成功
 *				: -1			失败
 */
BufferResult BufferProcess::buffer_process_batch(TaskRequest &Task)
{
	Task.renew_timestamp();
	szErrMsg[0] = 0;

	/* 取命令字 */
	int iCmd = Task.request_code();
	if (nodeEmptyLimit)
	{
		int bsize = Task.get_batch_size();
		if (bsize * 10 > nodeEmptyLimit)
		{
			Task.set_error(-EC_TOO_MANY_KEY_VALUE, __FUNCTION__, "batch count exceed LimitEmptyNodes/10");
			return BUFFER_PROCESS_ERROR;
		}
	}
	switch (iCmd)
	{
	case DRequest::Get:
		return buffer_batch_get_data(Task);

	default: // unknwon command treat as OK, fallback to split mode
		break;
	} //end of switch

	return BUFFER_PROCESS_OK;
}

/*
 * Function		: buffer_process_reply
 * Description	: 处理task from helper reply
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 0 			成功
 *				: -1			失败
 */

BufferResult BufferProcess::buffer_process_reply(TaskRequest &Task)
{
	Task.renew_timestamp();
	szErrMsg[0] = '\0';
	int iLimit = 0;

	int iCmd = Task.request_code();
	switch (iCmd)
	{
	case DRequest::Get: //一定是cache miss,全部replace入cache
		if (Task.flag_pass_thru())
		{
			if (Task.result)
				Task.pass_all_result(Task.result);
			return BUFFER_PROCESS_OK;
		}

		// ATTN: if failed, node always purged
		if (Task.result &&
			((nodeSizeLimit > 0 && Task.result->data_len() >= nodeSizeLimit) ||
			 (nodeRowsLimit > 0 && Task.result->total_rows() >= nodeRowsLimit)))
		{
			log_error("key[%d] rows[%d] size[%d] exceed limit",
					  Task.int_key(), Task.result->total_rows(), Task.result->data_len());
			iLimit = 1;
		}

		/* don't add empty node if Task back from blackhole */
		if (!iLimit && !Task.flag_black_hole())
			buffer_replace_result(Task);

		return buffer_get_rb(Task);

	case DRequest::Insert: //没有回读则必定是multirow,新数据附在原有数据后面
		if (Task.flag_black_hole())
			return buffer_nodb_insert(Task);

		if (insertOrder == INSERT_ORDER_PURGE)
		{
			buffer_purge_data(Task);
			return BUFFER_PROCESS_OK;
		}
		return buffer_sync_insert(Task);

	case DRequest::Update:
		if (Task.flag_black_hole())
			return buffer_nodb_update(Task);

		if (insertOrder == INSERT_ORDER_PURGE && Task.resultInfo.affected_rows() > 0)
		{
			buffer_purge_data(Task);
			return BUFFER_PROCESS_OK;
		}

		return buffer_sync_update(Task);

	case DRequest::Delete:
		if (Task.flag_black_hole())
			return buffer_nodb_delete(Task);
		return buffer_sync_delete(Task);

	case DRequest::Replace:
		if (Task.flag_black_hole())
			return buffer_nodb_replace(Task);
		return buffer_sync_replace(Task);

	case DRequest::SvrAdmin:
		if (Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::Migrate)
		{
			const DTCFieldValue *condition = Task.request_condition();
			const DTCValue *key = condition->field_value(0);
			Node stNode = Cache.cache_find_auto_chose_hash(key->bin.ptr);
			int rows = Cache.node_rows_count(stNode);
			log_debug("migrate replay ,row %d", rows);
			Cache.inc_total_row(0LL - rows);
			Cache.purge_node_everything(key->bin.ptr, stNode);
			log_debug("should purgenode everything");
			keyRoute->key_migrated(key->bin.ptr);
			delete (Task.request_operation());
			Task.set_request_operation(NULL);
			return BUFFER_PROCESS_OK;
		}
		if (Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::MigrateDB ||
			Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::MigrateDBSwitch)
		{
			return BUFFER_PROCESS_OK;
		}
		else
		{
			Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "invalid cmd from helper");
		}

	case DRequest::Replicate:
		// 处理主从同步
		return buffer_process_replicate(Task);

	default:
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "invalid cmd from helper");
	} //end of switch

	return BUFFER_PROCESS_ERROR;
}

/*
 * Function		: buffer_process_fullmode
 * Description	: 处理incoming task
 * Input			: Task			请求信息
 * Output		: Task			返回信息
 * Return		: 0 			成功
 *				: -1			失败
 */
BufferResult BufferProcess::buffer_process_nodb(TaskRequest &Task)
{
	// nodb mode always blackhole-d
	Task.mark_as_black_hole();
	Task.renew_timestamp();
	szErrMsg[0] = 0;

	/* 取命令字 */
	int iCmd = Task.request_code();
	switch (iCmd)
	{
	case DRequest::Get:
		if (Task.count_only() && (Task.requestInfo.limit_start() || Task.requestInfo.limit_count()))
		{
			Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "There's nothing to limit because no fields required");
			return BUFFER_PROCESS_ERROR;
		}
		++statGetCount;
		Task.set_result_hit_flag(HIT_INIT);
		return buffer_get_data(Task);

	case DRequest::Insert:
		++statInsertCount;
		return buffer_nodb_insert(Task);

	case DRequest::Update:
		++statUpdateCount;
		return buffer_nodb_update(Task);

	case DRequest::Delete:
		++statDeleteCount;
		return buffer_nodb_delete(Task);

	case DRequest::Purge:
		//删除指定key在cache中的数据
		++statPurgeCount;
		return buffer_purge_data(Task);

	case DRequest::Flush:
		return BUFFER_PROCESS_OK;

	case DRequest::Replace: //如果是淘汰的数据，不作处理
		++statUpdateCount;
		// 限制key字段作为唯一字段才能使用replace命令
		if (!(Task.table_definition()->key_part_of_uniq_field()) || Task.table_definition()->has_auto_increment())
		{
			Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "replace cmd require key fields part of uniq-fields and no auto-increment field");
			return BUFFER_PROCESS_ERROR;
		}
		return buffer_nodb_replace(Task);

	case DRequest::SvrAdmin:
		return buffer_process_admin(Task);

	default:
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "invalid cmd from client");
		log_notice("invalid cmd[%d] from client", iCmd);
		break;
	} //end of switch

	return BUFFER_PROCESS_ERROR;
}

/*
 * 当DTC后端使用诸如Rocksdb之类的单机内嵌式持久引擎时，主从同步需要从存储侧拉取全量
 * 数据，这里处理从存储引擎侧的返回值并返回给hotback主从同步端，注意：不对当前cache
 * 做任何更改
 * */
BufferResult BufferProcess::buffer_process_replicate(TaskRequest &Task)
{
	int iRet;

	log_info("do cache process replicate start!");

	// switch back the tabledef
	Task.set_request_code(DRequest::SvrAdmin);

	// 数据库回来的记录如果是0行，则表示全量同步结束
	if ((Task.result == NULL || Task.result->total_rows() == 0))
	{
		log_info("full replicate stage finished! key:[%u]", Task.int_key());
		Task.set_table_definition(Task.get_replicate_table());
		Task.set_error(-EC_FULL_SYNC_COMPLETE, "buffer_process_replicate", "full sync finished!");
		return BUFFER_PROCESS_ERROR;
	}

	// 处理返回值
	RowValue row(Task.get_replicate_table());
	RawData rawdata(&g_stSysMalloc, 1);

	Task.prepare_result_no_limit();

	if (Task.result != NULL)
	{
		ResultSet *pstResultSet = Task.result;
		for (int i = 0; i < pstResultSet->total_rows(); i++)
		{
			RowValue *pstRow = pstResultSet->_fetch_row();
			if (pstRow == NULL)
			{
				log_info("%s!", "call FetchRow func error");
				rawdata.Destroy();
				// hotback can not handle error exception now, just continue
				log_error("replicate: get data from storage failed!");
				continue;
			}

			// 设置key
			Task.set_request_key(pstRow->field_value(0));
			Task.build_packed_key();
			row[2] = (*pstRow)[0];

			// only bring back the key list
			Task.append_row(&row);

			rawdata.Destroy();
		}
	}

	log_info("do cache process replicate finished! ");
	Task.set_table_definition(Task.get_replicate_table());

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_flush_reply(TaskRequest &Task)
{
	szErrMsg[0] = '\0';

	int iCmd = Task.request_code();
	switch (iCmd)
	{
	case DRequest::Replace: //如果是淘汰的数据，不作处理
		return BUFFER_PROCESS_OK;
	default:
		Task.set_error(-EC_BAD_COMMAND, CACHE_SVC, "invalid cmd from helper");
	} //end of switch

	return BUFFER_PROCESS_ERROR;
}

BufferResult BufferProcess::buffer_process_error(TaskRequest &Task)
{
	// execute timeout
	szErrMsg[0] = '\0';

	switch (Task.request_code())
	{
	case DRequest::Insert:
		if (lossyMode == true && Task.result_code() == -ER_DUP_ENTRY)
		{
			// upstream is un-trusted
			Task.renew_timestamp();
			return buffer_sync_insert(Task);
		}
		// FALLTHROUGH
	case DRequest::Delete:
		switch (Task.result_code())
		{
		case -EC_UPSTREAM_ERROR:
		case -CR_SERVER_LOST:
			if (updateMode == MODE_SYNC)
			{
				log_notice("SQL execute result unknown, purge data");
				buffer_purge_data(Task);
			}
			else
			{
				log_crit("SQL execute result unknown, data may be corrupted");
			}
			break;
		}
		break;

	case DRequest::Update:
		switch (Task.result_code())
		{
		case -ER_DUP_ENTRY:
			if (lossyMode == true)
			{
				// upstream is un-trusted
				Task.renew_timestamp();
				return buffer_sync_update(Task);
			}
			// FALLTHROUGH
		case -EC_UPSTREAM_ERROR:
		case -CR_SERVER_LOST:
			if (updateMode == MODE_SYNC)
			{
				log_notice("SQL execute result unknown, purge data");
				buffer_purge_data(Task);
			}
			// must be cache miss
			break;
		}
		break;
	}

	return BUFFER_PROCESS_ERROR;
}

BufferResult BufferProcess::check_and_expire(TaskRequest &Task)
{
	uint32_t expire, now;
	int iRet = pstDataProcess->get_expire_time(Task.table_definition(), &m_stNode, expire);
	if (iRet != 0)
	{
		log_error("get_expire_time failed");
		Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}
	if (expire != 0 && expire <= (now = time(NULL)))
	{
		//expired
		++statExpireCount;
		log_debug("key: %u expired, purge current key when update, expire time: %d, current time: %d",
				  Task.int_key(), expire, now);
		if (Task.request_code() == DRequest::Get)
		{
			Task.prepare_result();
			Task.set_total_rows(0);
		}
		Cache.inc_total_row(0LL - Cache.node_rows_count(m_stNode));
		if (Cache.cache_purge(ptrKey) != 0)
			log_error("PANIC: purge node[id=%u] fail", m_stNode.node_id());
		return BUFFER_PROCESS_OK;
	}

	return BUFFER_PROCESS_NEXT;
}
