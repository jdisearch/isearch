/*
 * =====================================================================================
 *
 *       Filename:  sync_unit.h
 *
 *    Description:  sync uint class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "sync_unit.h"
#include "sync_index_timer.h"
#include "log.h"
#include "value.h"
#include "../db_manager.h"

int SyncUnit::ConnectDtcServer()
{
	const char *master_bind = _CacheConfig->GetStrVal("search_cache", "BindAddr");
	if (master_bind == NULL) {
		log_error("has no master bind port ");
		return -1;
	}
	//_MasterAddress = "127.0.0.1";
	//_MasterAddress.append(master_bind + 1);
	_MasterAddress = master_bind;
	log_info("master address is  [%s]", _MasterAddress.c_str());
	
	
	_MasterDtc.StringKey();
	const char *table_name = _CacheConfig->GetStrVal("search_cache", "TableName");
	if (table_name == NULL) {
		log_error("has no table name ");
		return -1;
	}
	_MasterDtc.SetTableName(table_name);
	_MasterDtc.SetAddress(_MasterAddress.c_str());
	_MasterDtc.SetMTimeout(300);

	int ret;
	if ((ret = _MasterDtc.Ping()) != 0 && ret != -DTC::EC_TABLE_MISMATCH) {
		log_error("ping slave[%s] failed, err: %d", _MasterAddress.c_str(), ret);
		return -1;
	}
	
	return 0;
}

int SyncUnit::RegisterSlave()
{
	DTC::SvrAdminRequest admin_req(&_MasterDtc);
	admin_req.SetAdminCode(DTC::RegisterHB);
	admin_req.SetHotBackupID((uint64_t) _JournalId);

	DTC::Result rs;
	admin_req.Execute(rs);
	switch (rs.ResultCode()) {
	case -DTC::EC_INC_SYNC_STAGE:
	{
		log_notice("server report: \"INC-SYNC\"");
		_SearchSyncStep = INC_SYNC;
		return 0;
	}
	case -DTC::EC_FULL_SYNC_STAGE:
	{
		log_notice("server report: \"FULL-SYNC\"");
		_JournalId = rs.HotBackupID();
		log_info
		("registed to master, master[serial=%u, offset=%u]",
			_JournalId.serial,
			_JournalId.offset);
		_SearchSyncStep = FULL_SYNC;
		return 0;
	}
	default:
	{
		log_notice("server report: \"ERR-SYNC\"");
		return -DTC::EC_ERR_SYNC_STAGE;
	}
	}

	return 0;
}

void SyncUnit::DoOnceSync(SearchIndex* index)
{
	int count = 0;
	while(_SearchSyncStep == FULL_SYNC){
		DoOnceFullSync(index);
		count++;
		if (count > 10000) {
			break;
		}
	}

	if (_SearchSyncStep == INC_SYNC) {
		log_info("do once inc sync");
		DoOnceIncSync(index);
	}
}

void SyncUnit::DoOnceFullSync(SearchIndex* index)
{
	DTC::SvrAdminRequest request_m(&_MasterDtc);
	request_m.SetAdminCode(DTC::GetKeyList);

	request_m.Need("key");
	request_m.Need("value");

	log_info("get key list st[%d], lt[%d]", _Start, _Limit);
	request_m.Limit(_Start, _Limit);

	DTC::Result result_m;
	int ret = request_m.Execute(result_m);
	if (DTC::EC_FULL_SYNC_COMPLETE == -ret) {
		_SearchSyncStep = INC_SYNC;
	}
	else if (0 != ret) {
		log_error("full sync data error");
		return;
	}

	log_info("rows number is [%d]", result_m.NumRows());
	for (int i = 0; i < result_m.NumRows(); ++i) {
		ret = result_m.FetchRow();
		if (ret < 0) {
			log_error(
				"fetch key-list from master failed, limit[%d %d], ret=%d, err=%s",
				_Start, _Limit, ret,
				result_m.ErrorMessage());
			return;
		}
		CValue v;			
		v.bin.ptr =	(char *)result_m.BinaryValue("key", &(v.bin.len));	
		
		std::string cache_key(v.bin.ptr+1 , v.bin.len - 1);
		log_debug("dtc cache key is [%s]", cache_key.c_str());

		UpdateOneIndex(cache_key, index);

		
	}
	_Start += _Limit;
}

void SyncUnit::DoOnceIncSync(SearchIndex* index)
{
	DTC::SvrAdminRequest request(&_MasterDtc);
	request.SetAdminCode(DTC::GetUpdateKey);
	request.Need("type");
	request.Need("flag");
	request.Need("key");
	request.Need("value");
	request.SetHotBackupID((uint64_t)_JournalId);	
	request.Limit(0, _Limit);
	DTC::Result result;

	
	int ret = request.Execute(result);
	if (-EBADRQC == ret) {
		log_error("master hot-backup not active yet");
	}

	if (-DTC::EC_BAD_HOTBACKUP_JID == ret) {
		log_error("master report journalID is not match");
	}

	_JournalId = (uint64_t)result.HotBackupID();
	for (int i = 0; i < result.NumRows(); ++i) {
		result.FetchRow();
		if (ret < 0) {
			log_error(
				"fetch key-list from master failed, limit[%d %d], ret=%d, err=%s",
				_Start, _Limit, ret,
				result.ErrorMessage());
			return;
		}
		CValue v;
		v.bin.ptr = (char *)result.BinaryValue("key", &(v.bin.len));
		std::string cache_key(v.bin.ptr + 1, v.bin.len - 1);
		log_debug("dtc cache key is [%s]", cache_key.c_str());
	
		UpdateOneIndex(cache_key, index);
		

	}

}

bool SyncUnit::UpdateOneIndex(const std::string &cache_key, SearchIndex *index)
{
	InvertIndexEntry tmp_invert_index_entry(cache_key);
	if (!tmp_invert_index_entry._IsValid)
		return false;

	DTC::GetRequest get_request(&_MasterDtc);
	get_request.SetKey(cache_key.c_str());
	get_request.Need("doc_id");
	get_request.Need("doc_version");
	get_request.Need("field");
	DTC::Result result_m;
	int ret = get_request.Execute(result_m);
	if (ret != 0) {
		log_error("get value error , errno[%d]", ret);
		return false;
	}
	
	log_info("fetch [%d] rows", result_m.NumRows());
	for (int i = 0; i < result_m.NumRows(); ++i) {
		result_m.FetchRow();
		tmp_invert_index_entry._InvertIndexDocId = result_m.StringValue("doc_id");
		tmp_invert_index_entry._InvertIndexField = result_m.IntValue("field");
		tmp_invert_index_entry._InvertIndexDocVersion= result_m.IntValue("doc_version");
		uint32_t appid = 0;
		istringstream is(tmp_invert_index_entry._InvertIndexAppid);
		is >> appid;
		if (DBManager::Instance()->IsFieldSupportRange(appid, tmp_invert_index_entry._InvertIndexField) == false) {
			continue;
		}
		index->InsertIndex(tmp_invert_index_entry);
	}
	return true;
}


