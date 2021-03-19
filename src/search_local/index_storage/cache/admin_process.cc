/*
 * =====================================================================================
 *
 *       Filename:  admin_process.cc
 *
 *    Description:  cache initialization & task request method
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
#include "admin_tdef.h"
#include "key_route.h"
#include "table_def_manager.h"

DTC_USING_NAMESPACE

extern DTCTableDefinition *gTableDef[];
extern int targetNewHash;
extern int hashChanging;
extern KeyRoute *keyRoute;
extern DbConfig  *dbConfig;

extern int collect_load_config(DbConfig *dbconfig);
#if __WORDSIZE == 64
#define UINT64FMT_T "%lu"
#else
#define UINT64FMT_T "%llu"
#endif

BufferResult BufferProcess::buffer_process_admin(TaskRequest &Task)
{
	log_debug("BufferProcess::buffer_process_admin admin_code is %d ", Task.requestInfo.admin_code());
	if (Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::QueryServerInfo || Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::LogoutHB || Task.requestInfo.admin_code() == DRequest::ServerAdminCmd::GetUpdateKey)
	{
		if (hbFeature == NULL)
		{ // 热备功能尚未启动
			Task.set_error(-EBADRQC, CACHE_SVC, "hot-backup not active yet");
			return BUFFER_PROCESS_ERROR;
		}
	}

	switch (Task.requestInfo.admin_code())
	{
	case DRequest::ServerAdminCmd::QueryServerInfo:
		return buffer_query_serverinfo(Task);

	case DRequest::ServerAdminCmd::RegisterHB:
		return buffer_register_hb(Task);

	case DRequest::ServerAdminCmd::LogoutHB:
		return buffer_logout_hb(Task);

	case DRequest::ServerAdminCmd::GetKeyList:
		return buffer_get_key_list(Task);

	case DRequest::ServerAdminCmd::GetUpdateKey:
		return buffer_get_update_key(Task);

	case DRequest::ServerAdminCmd::GetRawData:
		return buffer_get_raw_data(Task);

	case DRequest::ServerAdminCmd::ReplaceRawData:
		return buffer_replace_raw_data(Task);

	case DRequest::ServerAdminCmd::AdjustLRU:
		return buffer_adjust_lru(Task);

	case DRequest::ServerAdminCmd::VerifyHBT:
		return buffer_verify_hbt(Task);

	case DRequest::ServerAdminCmd::GetHBTime:
		return buffer_get_hbt(Task);

	case DRequest::ServerAdminCmd::NodeHandleChange:
		return buffer_nodehandlechange(Task);

	case DRequest::ServerAdminCmd::Migrate:
		return buffer_migrate(Task);

	case DRequest::ServerAdminCmd::ClearCache:
		return buffer_clear_cache(Task);

	case DRequest::ServerAdminCmd::MigrateDB:
	case DRequest::ServerAdminCmd::MigrateDBSwitch:
		if (update_mode() || is_mem_dirty())
		{
			log_error("try to migrate when cache is async");
			Task.set_error(-EC_SERVER_ERROR, "cache process", "try to migrate when cache is async");
			return BUFFER_PROCESS_ERROR;
		}
		return BUFFER_PROCESS_NEXT;

	case DRequest::ServerAdminCmd::ColExpandStatus:
		return buffer_check_expand_status(Task);

	case DRequest::ServerAdminCmd::col_expand:
		return buffer_column_expand(Task);

	case DRequest::ServerAdminCmd::ColExpandDone:
		return buffer_column_expand_done(Task);

	case DRequest::ServerAdminCmd::ColExpandKey:
		return buffer_column_expand_key(Task);

	default:
		Task.set_error(-EBADRQC, CACHE_SVC, "invalid admin cmd code from client");
		log_notice("invalid admin cmd code[%d] from client", Task.requestInfo.admin_code());
		break;
	}

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_check_expand_status(TaskRequest &Task)
{
	if (update_mode() || is_mem_dirty())
	{
		Task.set_error(-EC_SERVER_ERROR, "cache process", "try to column expand when cache is async");
		log_error("try to column expand when cache is async");
		return BUFFER_PROCESS_ERROR;
	}

	int ret = 0;
	// get table.conf
	RowValue stRow(Task.table_definition());
	Task.update_row(stRow);
	log_debug("value[len: %d]", stRow[3].bin.len);
	DTCTableDefinition *t;
	// parse table.conf to tabledef
	// release t by DEC_DELETE, not delete
	if (stRow[3].bin.ptr == NULL ||
		(t = TableDefinitionManager::Instance()->load_buffered_table(stRow[3].bin.ptr)) == NULL)
	{
		log_error("expand column with illegal table.conf");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "table.conf illegal");
		return BUFFER_PROCESS_ERROR;
	}
	if ((ret = Cache.check_expand_status()) == -1)
	{
		// check tabledef
		if (t->is_same_table(TableDefinitionManager::Instance()->get_new_table_def()))
		{
			log_notice("expand same column while expanding, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DUPLICATE, "cache process", "expand same column while expanding, canceled");
		}
		else
		{
			log_error("new expanding task while expand, canceled");
			Task.set_error(-EC_ERR_COL_EXPANDING, "cache process", "new expanding task while expand, canceled");
		}
		// release t
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	else if (ret == -2)
	{
		log_error("column expand not enabled");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "column expand not enabled");
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}

	log_debug("buffer_check_expand_status ok");
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_column_expand(TaskRequest &Task)
{
	int ret = 0;
	// get table.conf
	RowValue stRow(Task.table_definition());
	Task.update_row(stRow);
	log_debug("value[len: %d]", stRow[3].bin.len);
	DTCTableDefinition *t;
	// parse table.conf to tabledef
	// release t by DEC_DELETE, not delete
	if (stRow[3].bin.ptr == NULL ||
		(t = TableDefinitionManager::Instance()->load_buffered_table(stRow[3].bin.ptr)) == NULL)
	{
		log_error("expand column with illegal table.conf");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "table.conf illegal");
		return BUFFER_PROCESS_ERROR;
	}
	// check expanding
	if ((ret = Cache.check_expand_status()) == -1)
	{
		// check tabledef
		if (t->is_same_table(TableDefinitionManager::Instance()->get_new_table_def()))
		{
			log_notice("expand same column while expanding, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DUPLICATE, "cache process", "expand same column while expanding, canceled");
		}
		else
		{
			log_error("new expanding task while expand, canceled");
			Task.set_error(-EC_ERR_COL_EXPANDING, "cache process", "new expanding task while expand, canceled");
		}
		// release t
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	else if (ret == -2)
	{
		log_error("column expand not enabled");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "column expand not enabled");
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	if (t->is_same_table(TableDefinitionManager::Instance()->get_cur_table_def()))
	{
		log_notice("expand same column, canceled");
		Task.set_error(-EC_ERR_COL_EXPAND_DUPLICATE, "cache process", "expand same column, canceled");
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	// if ok
	if (TableDefinitionManager::Instance()->get_cur_table_idx() != Cache.shm_table_idx())
	{
		log_error("tabledefmanager's idx and shm's are different, need restart");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "tabledefmanager's idx and shm's are different");
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	// set new table for tabledefmanger
	// copy table.conf to shm
	if ((ret = Cache.try_col_expand(stRow[3].bin.ptr, stRow[3].bin.len)) != 0)
	{
		log_error("try col expand error, ret: %d", ret);
		Task.set_error(-EC_SERVER_ERROR, "cache process", "try col expand error");
		DEC_DELETE(t);
		return BUFFER_PROCESS_ERROR;
	}
	TableDefinitionManager::Instance()->set_new_table_def(t, (Cache.shm_table_idx() + 1));
	TableDefinitionManager::Instance()->renew_table_file_def(stRow[3].bin.ptr, stRow[3].bin.len);
	TableDefinitionManager::Instance()->save_db_config();
	Cache.col_expand(stRow[3].bin.ptr, stRow[3].bin.len);
	// hotbackup for nodb mode
	if (nodbMode)
		write_hb_log(_DTC_HB_COL_EXPAND_, stRow[3].bin.ptr, stRow[3].bin.len, DTCHotBackup::SYNC_COLEXPAND_CMD);
	log_debug("buffer_column_expand ok");
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_column_expand_done(TaskRequest &Task)
{
	int ret = 0;
	// get table.conf
	RowValue stRow(Task.table_definition());
	Task.update_row(stRow);
	log_debug("value[len: %d]", stRow[3].bin.len);
	DTCTableDefinition *t;
	// parse table.conf to tabledef
	// release t by DEC_DELETE, not delete
	if (stRow[3].bin.ptr == NULL ||
		(t = TableDefinitionManager::Instance()->load_buffered_table(stRow[3].bin.ptr)) == NULL)
	{
		log_error("expand column with illegal table.conf");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "table.conf illegal");
		return BUFFER_PROCESS_ERROR;
	}
	if ((ret = Cache.check_expand_status()) == -2)
	{
		log_error("expand done when not expand task begin or feature not enabled");
		Task.set_error(-EC_SERVER_ERROR, "cache process", "expand done when not expand task begin");
		return BUFFER_PROCESS_ERROR;
	}
	else if (ret == 0)
	{
		// check tabledef
		if (t->is_same_table(TableDefinitionManager::Instance()->get_cur_table_def()))
		{
			log_notice("expand done same column while expanding not start, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DONE_DUPLICATE, "cache process", "expand same column while expanding not start, canceled");
		}
		else
		{
			log_error("new expand done task while expanding not start, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DONE_DISTINCT, "cache process", "new expanding task while expanding not start, canceled");
		}
		return BUFFER_PROCESS_ERROR;
	}
	else
	{
		// check tabledef
		if (!t->is_same_table(TableDefinitionManager::Instance()->get_new_table_def()))
		{
			log_error("new expand done task while expanding, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DONE_DISTINCT, "cache process", "new expanding task done while expanding, canceled");
			return BUFFER_PROCESS_ERROR;
		}
	}
	//若是有源的，则重新载入配置文件到helper

	if (!nodbMode)
	{
		char *buf = stRow[3].bin.ptr;
		char *bufLocal = (char *)MALLOC(strlen(buf) + 1);
		memset(bufLocal, 0, strlen(buf) + 1);
		strcpy(bufLocal, buf);
		DbConfig *dbconfig = DbConfig::load_buffered(bufLocal);
		FREE(bufLocal);
		if (!dbconfig)
		{
			log_error("reload dbconfig for collect failed, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DONE_DISTINCT, "cache process", "reload dbconfig for collect failed, canceled");
			return BUFFER_PROCESS_ERROR;
		}
		if (collect_load_config(dbconfig))
		{
			log_error("reload config to collect failed, canceled");
			Task.set_error(-EC_ERR_COL_EXPAND_DONE_DISTINCT, "cache process", "reload config to collect failed, canceled");
			return BUFFER_PROCESS_ERROR;
		}
	}

	TableDefinitionManager::Instance()->renew_cur_table_def();
	TableDefinitionManager::Instance()->save_new_table_conf();
	DTCColExpand::Instance()->expand_done();
	// hotbackup for nodb mode
	if (nodbMode)
		write_hb_log(_DTC_HB_COL_EXPAND_DONE_, stRow[3].bin.ptr, stRow[3].bin.len, DTCHotBackup::SYNC_COLEXPAND_CMD);
	log_debug("buffer_column_expand_done ok");

	//若是有源的，则需要通知work helper重新载入配置文件
	if (!nodbMode)
	{
		TaskRequest *pTask = new TaskRequest(TableDefinitionManager::Instance()->get_cur_table_def());
		if (NULL == pTask)
		{
			log_error("cannot notify work helper reload config, new task error, possible memory exhausted!");
		}
		else
		{
			log_error("notify work helper reload config start!");
			pTask->set_request_type(TaskTypeHelperReloadConfig);
			pTask->set_request_code(DRequest::ReloadConfig);
			pTask->push_reply_dispatcher(&cacheReply);
			output.task_notify(pTask);
		}
	}
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_column_expand_key(TaskRequest &Task)
{
	if (Cache.check_expand_status() != -1)
	{
		log_error("expand one when not expand task begin or feature not enabled");
		Task.set_error(-EC_ERR_COL_NOT_EXPANDING, "cache process", "expand one when not expand task begin");
		return BUFFER_PROCESS_ERROR;
	}
	int iRet = 0;

	const DTCFieldValue *condition = Task.request_condition();
	const DTCValue *key;

	// TODO this may need fix, as we do not check whether this field is key
	if (!condition || condition->num_fields() < 1 || condition->field_id(0) != 2)
	{
		Task.set_error(-EC_ERR_COL_NO_KEY, "cache process", "no key value append for col expand");
		log_error("no key value append for col expand");
		return BUFFER_PROCESS_ERROR;
	}
	key = condition->field_value(0);
	Node stNode = Cache.cache_find_auto_chose_hash(key->bin.ptr);
	if (!stNode)
	{
		log_notice("key not exist for col expand");
		return BUFFER_PROCESS_OK;
	}

	iRet = pstDataProcess->expand_node(Task, &stNode);
	if (iRet == -4)
	{
		Task.set_error(-EC_ERR_COL_EXPAND_NO_MEM, "cache process", pstDataProcess->get_err_msg());
		log_error("no mem to expand for key, %s", pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}
	else if (iRet != 0)
	{
		Task.set_error(-EC_SERVER_ERROR, "cache process", pstDataProcess->get_err_msg());
		log_error("expand key error: %s", pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}
	// hotbackup for nodb mode
	if (nodbMode)
		write_hb_log(key->bin.ptr, NULL, 0, DTCHotBackup::SYNC_COLEXPAND);

	log_debug("buffer_column_expand_key ok");
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_register_hb(TaskRequest &Task)
{
	if (hbFeature == NULL)
	{ // 共享内存还没有激活热备特性
		NEW(HBFeature, hbFeature);
		if (hbFeature == NULL)
		{
			log_error("new hot-backup feature error: %m");
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "new hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
		int iRet = hbFeature->Init(time(NULL));
		if (iRet == -ENOMEM)
		{
			Node stNode;
			if (Cache.try_purge_size(1, stNode) == 0)
				iRet = hbFeature->Init(time(NULL));
		}
		if (iRet != 0)
		{
			log_error("init hot-backup feature error: %d", iRet);
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "init hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
		iRet = Cache.add_feature(HOT_BACKUP, hbFeature->Handle());
		if (iRet != 0)
		{
			log_error("add hot-backup feature error: %d", iRet);
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "add hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
	}
	if (hbFeature->master_uptime() == 0)
		hbFeature->master_uptime() = time(NULL);

	//开启变更key日志
	hbLogSwitch = true;

	int64_t hb_timestamp = hbFeature->master_uptime();
	Task.versionInfo.set_master_hb_timestamp(hb_timestamp);
	Task.versionInfo.set_slave_hb_timestamp(hbFeature->slave_uptime());

	Task.set_request_type(TaskTypeRegisterHbLog);
	dispatch_hot_back_task(&Task);
	return BUFFER_PROCESS_PUSH_HB;
}

BufferResult BufferProcess::buffer_logout_hb(TaskRequest &Task)
{
	//TODO：暂时没有想到logout的场景
	return BUFFER_PROCESS_OK;
}

/*
 * 遍历cache中所有的Node节点
 */
BufferResult BufferProcess::buffer_get_key_list(TaskRequest &Task)
{

	uint32_t lst, lcnt;
	lst = Task.requestInfo.limit_start();
	lcnt = Task.requestInfo.limit_count();

	log_debug("buffer_get_key_list start, Limit[%u %u]", lst, lcnt);

	// if the storage is Rocksdb, do replicate through it directly in full sync stage,
	// just dispath the task to helper unit
	if ( !nodbMode && dbConfig->dstype == 2/* rocksdb */ )
	{
		log_info("proc local replicate!");
		Task.set_request_code(DRequest::Replicate);
		// Task.SetRequestType(TaskTypeHelperReplicate);
		Task.set_request_type(TaskTypeRead);

		// due to the hotback has a different table definition with the normal query, so 
		// need to switch table definition during query the storage
		DTCTableDefinition* repTab = Task.table_definition();

		Task.set_table_definition(TableDefinitionManager::Instance()->get_cur_table_def());
		Task.set_replicate_table(repTab);

		return BUFFER_PROCESS_NEXT;
	}

	//遍历完所有的Node节点
	if (lst > Cache.max_node_id())
	{
		Task.set_error(-EC_FULL_SYNC_COMPLETE, "buffer_get_key_list", "node id is overflow");
		return BUFFER_PROCESS_ERROR;
	}

	Task.prepare_result_no_limit();

	RowValue r(Task.table_definition());
	RawData rawdata(&g_stSysMalloc, 1);

	for (unsigned i = lst; i < lst + lcnt; ++i)
	{
		if (i < Cache.min_valid_node_id())
			continue;
		if (i > Cache.max_node_id())
			break;

		//查找对应的Node节点
		Node node = I_SEARCH(i);
		if (!node)
			continue;
		if (node.not_in_lru_list())
			continue;
		if (Cache.is_time_marker(node))
			continue;

		// 解码Key
		DataChunk *keyptr = M_POINTER(DataChunk, node.vd_handle());

		//发送packedkey
		r[2] = TableDefinitionManager::Instance()->get_cur_table_def()->packed_key(keyptr->Key());

		//解码Value
		if (pstDataProcess->get_all_rows(&node, &rawdata))
		{
			rawdata.Destroy();
			continue;
		}

		r[3].Set((char *)(rawdata.get_addr()), (int)(rawdata.data_size()));

		Task.append_row(&r);

		rawdata.Destroy();
	}

	return BUFFER_PROCESS_OK;
}

/*
 * hot backup拉取更新key或者lru变更，如果没有则挂起请求,直到
 * 1. 超时
 * 2. 有更新key, 或者LRU变更
 */
BufferResult BufferProcess::buffer_get_update_key(TaskRequest &Task)
{
	log_debug("buffer_get_update_key start");
	Task.set_request_type(TaskTypeReadHbLog);
	dispatch_hot_back_task(&Task);
	return BUFFER_PROCESS_PUSH_HB;
}

BufferResult BufferProcess::buffer_get_raw_data(TaskRequest &Task)
{
	int iRet;

	const DTCFieldValue *condition = Task.request_condition();
	const DTCValue *key;

	log_debug("buffer_get_raw_data start ");

	RowValue stRow(Task.table_definition()); //一行数据
	RawData stNodeData(&g_stSysMalloc, 1);

	Task.prepare_result_no_limit();

	for (int i = 0; i < condition->num_fields(); i++)
	{
		key = condition->field_value(i);
		stRow[1].u64 = DTCHotBackup::HAS_VALUE; //表示附加value字段
		stRow[2].Set(key->bin.ptr, key->bin.len);

		Node stNode = Cache.cache_find_auto_chose_hash(key->bin.ptr);
		if (!stNode)
		{ //master没有该key的数据
			stRow[1].u64 = DTCHotBackup::KEY_NOEXIST;
			stRow[3].Set(0);
			Task.append_row(&stRow);
			continue;
		}
		else
		{

			iRet = pstDataProcess->get_all_rows(&stNode, &stNodeData);
			if (iRet != 0)
			{
				log_error("get raw-data failed");
				Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
				return BUFFER_PROCESS_ERROR;
			}
			stRow[3].Set((char *)(stNodeData.get_addr()), (int)(stNodeData.data_size()));
		}

		Task.append_row(&stRow); //当前行添加到task中
		stNodeData.Destroy();
	}

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_replace_raw_data(TaskRequest &Task)
{
	log_debug("buffer_replace_raw_data start ");

	int iRet;

	const DTCFieldValue *condition = Task.request_condition();
	const DTCValue *key;

	RowValue stRow(Task.table_definition()); //一行数据
	RawData stNodeData(&g_stSysMalloc, 1);
	if (condition->num_fields() < 1)
	{
		log_debug("%s", "replace raw data need key");
		Task.set_error_dup(-EC_KEY_NEEDED, CACHE_SVC, pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}

	key = condition->field_value(0);
	stRow[2].Set(key->bin.ptr, key->bin.len);
	Task.update_row(stRow); //获取数据

	log_debug("value[len: %d]", stRow[3].bin.len);

	//调整备机的空节点过滤
	if (stRow[1].u64 & DTCHotBackup::EMPTY_NODE && m_pstEmptyNodeFilter)
	{
		m_pstEmptyNodeFilter->SET(*(unsigned int *)(key->bin.ptr));
	}

	//key在master不存在, 或者是空节点，purge cache.
	if (stRow[1].u64 & DTCHotBackup::KEY_NOEXIST || stRow[1].u64 & DTCHotBackup::EMPTY_NODE)
	{
		log_debug("purge slave data");
		Node stNode = Cache.cache_find_auto_chose_hash(key->bin.ptr);
		int rows = Cache.node_rows_count(stNode);
		log_debug("migrate replay ,row %d", rows);
		Cache.inc_total_row(0LL - rows);
		Cache.cache_purge(key->bin.ptr);
		return BUFFER_PROCESS_OK;
	}

	// 解析成raw data
	ALLOC_HANDLE_T hData = g_stSysMalloc.Malloc(stRow[3].bin.len);
	if (hData == INVALID_HANDLE)
	{
		log_error("malloc error: %m");
		Task.set_error(-ENOMEM, CACHE_SVC, "malloc error");
		return BUFFER_PROCESS_ERROR;
	}

	memcpy(g_stSysMalloc.handle_to_ptr(hData), stRow[3].bin.ptr, stRow[3].bin.len);

	if ((iRet = stNodeData.Attach(hData, 0, tableDef->key_format())) != 0)
	{
		log_error("parse raw-data error: %d, %s", iRet, stNodeData.get_err_msg());
		Task.set_error(-EC_BAD_RAW_DATA, CACHE_SVC, "bad raw data");
		return BUFFER_PROCESS_ERROR;
	}

	// 检查packed key是否匹配
	DTCValue packed_key = TableDefinitionManager::Instance()->get_cur_table_def()->packed_key(stNodeData.Key());
	if (packed_key.bin.len != key->bin.len || memcmp(packed_key.bin.ptr, key->bin.ptr, key->bin.len))
	{
		log_error("packed key miss match, key size=%d, packed key size=%d", key->bin.len, packed_key.bin.len);
		log_error("packed key miss match, packed_key %s,key %s", packed_key.bin.ptr, key->bin.ptr);
		Task.set_error(-EC_BAD_RAW_DATA, CACHE_SVC, "packed key miss match");
		return BUFFER_PROCESS_ERROR;
	}

	// 查找分配node节点
	unsigned int uiNodeID;
	Node stNode = Cache.cache_find_auto_chose_hash(key->bin.ptr);

	if (!stNode)
	{
		for (int i = 0; i < 2; i++)
		{
			stNode = Cache.cache_allocate(key->bin.ptr);
			if (!(!stNode))
				break;
			if (Cache.try_purge_size(1, stNode) != 0)
				break;
		}
		if (!stNode)
		{
			log_error("alloc cache node error");
			Task.set_error(-EIO, CACHE_SVC, "alloc cache node error");
			return BUFFER_PROCESS_ERROR;
		}
		stNode.vd_handle() = INVALID_HANDLE;
	}
	else
	{
		Cache.remove_from_lru(stNode);
		Cache.insert2_clean_lru(stNode);
	}

	uiNodeID = stNode.node_id();

	// 替换数据
	iRet = pstDataProcess->replace_data(&stNode, &stNodeData);
	if (iRet != 0)
	{
		if (nodbMode)
		{
			/* FIXME: no backup db, can't purge data, no recover solution yet */
			log_error("cache replace raw data error: %d, %s", iRet, pstDataProcess->get_err_msg());
			Task.set_error(-EIO, CACHE_SVC, "ReplaceRawData() error");
			return BUFFER_PROCESS_ERROR;
		}
		else
		{
			log_error("cache replace raw data error: %d, %s. purge node: %u", iRet, pstDataProcess->get_err_msg(), uiNodeID);
			Cache.purge_node_everything(key->bin.ptr, stNode);
			return BUFFER_PROCESS_OK;
		}
	}

	Cache.inc_total_row(pstDataProcess->rows_inc());

	log_debug("buffer_replace_raw_data success! ");

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_adjust_lru(TaskRequest &Task)
{

	const DTCFieldValue *condition = Task.request_condition();
	const DTCValue *key;

	log_debug("buffer_adjust_lru start ");

	RowValue stRow(Task.table_definition()); //一行数据

	for (int i = 0; i < condition->num_fields(); i++)
	{
		key = condition->field_value(i);

		Node stNode;
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

			stNode = Cache.cache_find(key->bin.ptr, oldhash);
			if (!stNode)
			{
				stNode = Cache.cache_find(key->bin.ptr, newhash);
			}
			else
			{
				Cache.move_to_new_hash(key->bin.ptr, stNode);
			}
		}
		else
		{
			if (targetNewHash)
			{
				stNode = Cache.cache_find(key->bin.ptr, 1);
			}
			else
			{
				stNode = Cache.cache_find(key->bin.ptr, 0);
			}
		}
		if (!stNode)
		{
			//		            continue;
			Task.set_error(-EC_KEY_NOTEXIST, CACHE_SVC, "key not exist");
			return BUFFER_PROCESS_ERROR;
		}
		Cache.remove_from_lru(stNode);
		Cache.insert2_clean_lru(stNode);
	}

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_verify_hbt(TaskRequest &Task)
{
	log_debug("buffer_verify_hbt start ");

	if (hbFeature == NULL)
	{ // 共享内存还没有激活热备特性
		NEW(HBFeature, hbFeature);
		if (hbFeature == NULL)
		{
			log_error("new hot-backup feature error: %m");
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "new hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
		int iRet = hbFeature->Init(0);
		if (iRet == -ENOMEM)
		{
			Node stNode;
			if (Cache.try_purge_size(1, stNode) == 0)
				iRet = hbFeature->Init(0);
		}
		if (iRet != 0)
		{
			log_error("init hot-backup feature error: %d", iRet);
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "init hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
		iRet = Cache.add_feature(HOT_BACKUP, hbFeature->Handle());
		if (iRet != 0)
		{
			log_error("add hot-backup feature error: %d", iRet);
			Task.set_error(-EC_SERVER_ERROR, "buffer_register_hb", "add hot-backup feature fail");
			return BUFFER_PROCESS_ERROR;
		}
	}

	int64_t master_timestamp = Task.versionInfo.master_hb_timestamp();
	if (hbFeature->slave_uptime() == 0)
	{
		hbFeature->slave_uptime() = master_timestamp;
	}
	else if (hbFeature->slave_uptime() != master_timestamp)
	{
		log_error("hot backup timestamp incorrect, master[%lld], this slave[%lld]", (long long)master_timestamp, (long long)(hbFeature->slave_uptime()));
		Task.set_error(-EC_ERR_SYNC_STAGE, "buffer_verify_hbt", "verify hot backup timestamp fail");
		return BUFFER_PROCESS_ERROR;
	}

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_get_hbt(TaskRequest &Task)
{
	log_debug("buffer_get_hbt start ");

	if (hbFeature == NULL)
	{ // 共享内存还没有激活热备特性
		Task.versionInfo.set_master_hb_timestamp(0);
		Task.versionInfo.set_slave_hb_timestamp(0);
	}
	else
	{
		Task.versionInfo.set_master_hb_timestamp(hbFeature->master_uptime());
		Task.versionInfo.set_slave_hb_timestamp(hbFeature->slave_uptime());
	}

	log_debug("master-up-time: %lld, slave-up-time: %lld", (long long)(Task.versionInfo.master_hb_timestamp()), (long long)(Task.versionInfo.slave_hb_timestamp()));

	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_query_serverinfo(TaskRequest &Task)
{
	log_debug("buffer_query_serverinfo start");
	Task.set_request_type(TaskTypeQueryHbLogInfo);
	dispatch_hot_back_task(&Task);
	return BUFFER_PROCESS_PUSH_HB;
}

/* finished in one cache process cycle */
BufferResult BufferProcess::buffer_nodehandlechange(TaskRequest &Task)
{
	log_debug("buffer_nodehandlechange start ");

	const DTCFieldValue *condition = Task.request_condition();
	const DTCValue *key = condition->field_value(0);
	Node node;
	MEM_HANDLE_T node_handle;
	RawData node_raw_data(DTCBinMalloc::Instance(), 0);
	/* no need of private raw data, just for copy */
	char *private_buff = NULL;
	int buff_len;
	MEM_HANDLE_T new_node_handle;

	if (condition->num_fields() < 1)
	{
		log_debug("%s", "nodehandlechange need key");
		Task.set_error_dup(-EC_KEY_NEEDED, CACHE_SVC, pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}

	/* packed key -> node id -> node handle -> node raw data -> private buff*/
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
		node = Cache.cache_find(key->bin.ptr, oldhash);
		if (!node)
		{
			node = Cache.cache_find(key->bin.ptr, newhash);
		}
		else
		{
			Cache.move_to_new_hash(key->bin.ptr, node);
		}
	}
	else
	{
		if (targetNewHash)
		{
			node = Cache.cache_find(key->bin.ptr, 1);
		}
		else
		{
			node = Cache.cache_find(key->bin.ptr, 0);
		}
	}

	if (!node)
	{
		log_debug("%s", "key not exist for defragmentation");
		Task.set_error(-ER_KEY_NOT_FOUND, CACHE_SVC, "node not found");
		return BUFFER_PROCESS_ERROR;
	}

	node_handle = node.vd_handle();
	if (node_handle == INVALID_HANDLE)
	{
		Task.set_error(-EC_BAD_RAW_DATA, CACHE_SVC, "chunk not exist");
		return BUFFER_PROCESS_ERROR;
	}

	node_raw_data.Attach(node_handle, tableDef->key_fields() - 1, tableDef->key_format());

	if ((private_buff = (char *)MALLOC(node_raw_data.data_size())) == NULL)
	{
		log_error("no mem");
		Task.set_error(-ENOMEM, CACHE_SVC, "malloc error");
		return BUFFER_PROCESS_ERROR;
	}

	memcpy(private_buff, node_raw_data.get_addr(), node_raw_data.data_size());
	buff_len = node_raw_data.data_size();
	if (node_raw_data.Destroy())
	{
		log_error("node raw data detroy error");
		Task.set_error(-ENOMEM, CACHE_SVC, "free error");
		FREE_IF(private_buff);
		return BUFFER_PROCESS_ERROR;
	}
	log_debug("old node handle: " UINT64FMT_T ", raw data size %d", node_handle, buff_len);

	/* new chunk */
	/* new node handle -> new node handle ptr <- node raw data ptr*/
	new_node_handle = DTCBinMalloc::Instance()->Malloc(buff_len);
	log_debug("new node handle: " UINT64FMT_T, new_node_handle);

	if (new_node_handle == INVALID_HANDLE)
	{
		log_error("malloc error: %m");
		Task.set_error(-ENOMEM, CACHE_SVC, "malloc error");
		FREE_IF(private_buff);
		return BUFFER_PROCESS_ERROR;
	}

	memcpy(DTCBinMalloc::Instance()->handle_to_ptr(new_node_handle), private_buff, buff_len);

	/* free node raw data, set node handle */
	node.vd_handle() = new_node_handle;
	FREE_IF(private_buff);

	log_debug("buffer_nodehandlechange success! ");
	return BUFFER_PROCESS_OK;
}

BufferResult BufferProcess::buffer_migrate(TaskRequest &Task)
{
	if (keyRoute == 0)
	{
		log_error("not support migrate cmd @ bypass mode");
		Task.set_error(-EC_SERVER_ERROR, "buffer_migrate", "Not Support @ Bypass Mode");
		return BUFFER_PROCESS_ERROR;
	}
	int iRet;

	const DTCFieldValue *ui = Task.request_operation();
	const DTCValue key = TableDefinitionManager::Instance()->get_cur_table_def()->packed_key(Task.packed_key());
	if (key.bin.ptr == 0 || key.bin.len <= 0)
	{
		Task.set_error(-EC_KEY_NEEDED, "buffer_migrate", "need set migrate key");
		return BUFFER_PROCESS_ERROR;
	}

	log_debug("cache_cache_migrate start ");

	RowValue stRow(Task.table_definition()); //一行数据
	RawData stNodeData(&g_stSysMalloc, 1);

	Node stNode = Cache.cache_find_auto_chose_hash(key.bin.ptr);

	//如果有updateInfo则说明请求从DTC过来
	int flag = 0;
	if (ui && ui->field_value(0))
	{
		flag = ui->field_value(0)->s64;
	}
	if ((flag & 0xFF) == DTCMigrate::FROM_SERVER)
	{
		log_debug("this migrate cmd is from DTC");
		RowValue stRow(Task.table_definition()); //一行数据
		RawData stNodeData(&g_stSysMalloc, 1);
		stRow[2].Set(key.bin.ptr, key.bin.len);
		Task.update_row(stRow); //获取数据

		log_debug("value[len: %d]", stRow[3].bin.len);

		//key在master不存在, 或者是空节点，purge cache.
		if (stRow[1].u64 & DTCHotBackup::KEY_NOEXIST || stRow[1].u64 & DTCHotBackup::EMPTY_NODE)
		{
			log_debug("purge slave data");
			Cache.cache_purge(key.bin.ptr);
			return BUFFER_PROCESS_OK;
		}

		// 解析成raw data
		ALLOC_HANDLE_T hData = g_stSysMalloc.Malloc(stRow[3].bin.len);
		if (hData == INVALID_HANDLE)
		{
			log_error("malloc error: %m");
			Task.set_error(-ENOMEM, CACHE_SVC, "malloc error");
			return BUFFER_PROCESS_ERROR;
		}

		memcpy(g_stSysMalloc.handle_to_ptr(hData), stRow[3].bin.ptr, stRow[3].bin.len);

		if ((iRet = stNodeData.Attach(hData, 0, tableDef->key_format())) != 0)
		{
			log_error("parse raw-data error: %d, %s", iRet, stNodeData.get_err_msg());
			Task.set_error(-EC_BAD_RAW_DATA, CACHE_SVC, "bad raw data");
			return BUFFER_PROCESS_ERROR;
		}

		// 检查packed key是否匹配
		DTCValue packed_key = TableDefinitionManager::Instance()->get_cur_table_def()->packed_key(stNodeData.Key());
		if (packed_key.bin.len != key.bin.len || memcmp(packed_key.bin.ptr, key.bin.ptr, key.bin.len))
		{
			log_error("packed key miss match, key size=%d, packed key size=%d",
					  key.bin.len, packed_key.bin.len);

			Task.set_error(-EC_BAD_RAW_DATA, CACHE_SVC, "packed key miss match");
			return BUFFER_PROCESS_ERROR;
		}

		// 查找分配node节点
		unsigned int uiNodeID;

		if (!stNode)
		{
			for (int i = 0; i < 2; i++)
			{
				stNode = Cache.cache_allocate(key.bin.ptr);
				if (!(!stNode))
					break;
				if (Cache.try_purge_size(1, stNode) != 0)
					break;
			}
			if (!stNode)
			{
				log_error("alloc cache node error");
				Task.set_error(-EIO, CACHE_SVC, "alloc cache node error");
				return BUFFER_PROCESS_ERROR;
			}
			stNode.vd_handle() = INVALID_HANDLE;
		}
		else
		{
			Cache.remove_from_lru(stNode);
			Cache.insert2_clean_lru(stNode);
		}
		if ((flag >> 8) & 0xFF) //如果为脏节点
		{

			Cache.remove_from_lru(stNode);
			Cache.insert2_dirty_lru(stNode);
		}

		uiNodeID = stNode.node_id();

		// 替换数据
		iRet = pstDataProcess->replace_data(&stNode, &stNodeData);
		if (iRet != 0)
		{
			if (nodbMode)
			{
				/* FIXME: no backup db, can't purge data, no recover solution yet */
				log_error("cache replace raw data error: %d, %s", iRet, pstDataProcess->get_err_msg());
				Task.set_error(-EIO, CACHE_SVC, "ReplaceRawData() error");
				return BUFFER_PROCESS_ERROR;
			}
			else
			{
				log_error("cache replace raw data error: %d, %s. purge node: %u",
						  iRet, pstDataProcess->get_err_msg(), uiNodeID);
				Cache.purge_node_everything(key.bin.ptr, stNode);
				return BUFFER_PROCESS_OK;
			}
		}
		if (write_hb_log(key.bin.ptr, stNode, DTCHotBackup::SYNC_UPDATE))
		{
			log_crit("buffer_migrate: log update key failed");
		}
		Cache.inc_total_row(pstDataProcess->rows_inc());

		Task.prepare_result_no_limit();

		return BUFFER_PROCESS_OK;
	}

	log_debug("this migrate cmd is from api");
	//请求从工具过来，我们需要构造请求发给其他dtc

	if (!stNode)
	{
		Task.set_error(-EC_KEY_NOTEXIST, "buffer_migrate", "this key not found in cache");
		return BUFFER_PROCESS_ERROR;
	}
	//获取该节点的raw-data，构建replace请求给后端helper
	iRet = pstDataProcess->get_all_rows(&stNode, &stNodeData);
	if (iRet != 0)
	{
		log_error("get raw-data failed");
		Task.set_error_dup(-EIO, CACHE_SVC, pstDataProcess->get_err_msg());
		return BUFFER_PROCESS_ERROR;
	}

	DTCFieldValue *uitmp = new DTCFieldValue(4);
	if (uitmp == NULL)
	{
		Task.set_error(-EIO, CACHE_SVC, "migrate:new DTCFieldValue error");
		return BUFFER_PROCESS_ERROR;
	}
	//id0 {"type", DField::Unsigned, 4, DTCValue::Make(0), 0}
	//type的最后一个字节用来表示请求来着其他dtc还是api
	//倒数第二个字节表示节点是否为脏
	uitmp->add_value(0, DField::Set, DField::Unsigned, DTCValue::Make(DTCMigrate::FROM_SERVER | (stNode.is_dirty() << 8)));

	//id1 {"flag", DField::Unsigned, 1, DTCValue::Make(0), 0},
	uitmp->add_value(1, DField::Set, DField::Unsigned, DTCValue::Make(DTCHotBackup::HAS_VALUE));
	//id2 {"key", DField::Binary, 255, DTCValue::Make(0), 0},

	//id3 {"value", DField::Binary, MAXPACKETSIZE, DTCValue::Make(0), 0},

	FREE_IF(Task.migratebuf);
	Task.migratebuf = (char *)calloc(1, stNodeData.data_size());
	if (Task.migratebuf == NULL)
	{
		log_error("create buffer failed");
		Task.set_error(-EIO, CACHE_SVC, "migrate:get raw data,create buffer failed");
		return BUFFER_PROCESS_ERROR;
	}
	memcpy(Task.migratebuf, (char *)(stNodeData.get_addr()), (int)(stNodeData.data_size()));
	uitmp->add_value(3, DField::Set, DField::Binary,
					DTCValue::Make(Task.migratebuf, stNodeData.data_size()));
	Task.set_request_operation(uitmp);
	keyRoute->key_migrating(stNodeData.Key());

	return BUFFER_PROCESS_REMOTE;
}

BufferResult BufferProcess::buffer_clear_cache(TaskRequest &Task)
{
	if (updateMode != MODE_SYNC)
	{
		log_error("try to clear cache for async mode, abort...");
		Task.set_error(-EC_SERVER_ERROR, "buffer_clear_cache", "can not clear cache for aync mode, abort");
		return BUFFER_PROCESS_ERROR;
	}
	// clean and rebuild
	int64_t mu = 0, su = 0;
	if (hbFeature != NULL)
	{
		mu = hbFeature->master_uptime();
		su = hbFeature->slave_uptime();
	}
	// table.conf in shm is set in clear_create
	int ret = Cache.clear_create();
	if (ret < 0)
	{
		log_error("clear and create cache error: %s", Cache.Error());
		if (ret == -1)
		{
			log_error("fault error, exit...");
			exit(-1);
		}
		if (ret == -2)
		{
			log_error("error, abort...");
			Task.set_error(-EC_SERVER_ERROR, "buffer_clear_cache", "clear cache error, abort");
			return BUFFER_PROCESS_ERROR;
		}
	}
	pstDataProcess->change_mallocator(DTCBinMalloc::Instance());
	// setup hotbackup
	if (hbFeature != NULL)
	{
		hbFeature->Detach();
		// no need consider no enough mem, as mem is just cleared
		hbFeature->Init(0);
		int iRet = Cache.add_feature(HOT_BACKUP, hbFeature->Handle());
		if (iRet != 0)
		{
			log_error("add hot-backup feature error: %d", iRet);
			exit(-1);
		}
		hbFeature->master_uptime() = mu;
		hbFeature->slave_uptime() = su;
	}
	// hotbackup
	char buf[16];
	memset(buf, 0, sizeof(buf));
	Node node;
	if (write_hb_log(buf, node, DTCHotBackup::SYNC_CLEAR))
		log_error("hb: log clear cache error");

	return BUFFER_PROCESS_OK;
}
