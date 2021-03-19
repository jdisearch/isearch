/*
 * =====================================================================================
 *
 *       Filename:  sync_index_timer.h
 *
 *    Description:  sync index timer class definition.
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

#include "sync_index_timer.h"
#include "sync_unit.h"
#include "sequence_search_index.h"
#include "config.h"

const char *cache_configfile = "../conf/cache.conf";



void SyncIndexTimer::TimerNotify(void)
{
	_IndexSynUnit->DoOnceSync(_SearchIndex);
}

bool SyncIndexTimer::StartSyncTask()
{
	if (!InitConfig()) return false;
	_IndexSynUnit = new SyncUnit(_DTCCacheConfig);
	if (_IndexSynUnit == NULL || _IndexSynUnit->ConnectDtcServer() != 0) {
		log_error("can not connect dtc server");
		return false;
	}

	const char *search_type = _DTCCacheConfig->GetStrVal("search_cache", "SearchIndexType");
	if (search_type == NULL) {
		log_error("has no search_type ");
		return false;
	}
	log_debug("search_type: %s.", search_type);

	if(strcmp(search_type, "SequenceSet") == 0){
		if (_IndexSynUnit->RegisterSlave()) {
			log_error("register slave error");
			return false;
		}
		_SearchIndex = new SequenceSetIndex();
		if (NULL == _SearchIndex) {
			log_error("no new memory for search index");
			return false;
		}
		_IndexSynUnit->DoOnceSync(_SearchIndex);

		if(_Timer != NULL){
			AttachTimer(_Timer);
		}
	} else if(strcmp(search_type, "SearchRocksDB") == 0){
		_SearchIndex = new SearchRocksDBIndex();
		if (NULL == _SearchIndex) {
			log_error("no new memory for search index");
			return false;
		}
		if(_SearchIndex->Init() == false){
			log_error("_SearchIndex init error.");
			return false;
		}
	} else {
		log_error("search_type is wrong");
		return true;
	}
	
	return true;
}

void SyncIndexTimer::DumpIndex()
{
	_SearchIndex->DumpIndex();
}

bool SyncIndexTimer::InitConfig()
{

	_DTCCacheConfig = new CConfig();
	if (cache_configfile == NULL || _DTCCacheConfig->ParseConfig(cache_configfile, "search_cache")) {
		log_error("no cache config or config file [%s] is error", cache_configfile);
		return false;
	}


	return true;
}