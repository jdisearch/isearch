/*
 * =====================================================================================
 *
 *       Filename:  sequence_search_index.h
 *
 *    Description:  sequence search index definition.
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

#ifndef  __SEQUENCE_SEARCH_INDEX_H__
#define __SEQUENCE_SEARCH_INDEX_H__

#include "sync_index_timer.h"
#include "skiplist.h"
#include "../comm.h"
#include "uds_client.h"
#include <vector>
#include <set>
#include <pthread.h>
#include <string>
#include <map>
#include <sstream>
#include "rocksdb_direct_context.h"

class SearchIndex {
public:
	virtual bool Init() = 0;
	virtual void GetRangeIndex(uint32_t range_type, InvertIndexEntry &begin_key, InvertIndexEntry &end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGELE(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGE(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexLE(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGTLT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGTLE(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGELT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexGT(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetRangeIndexLT(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry) = 0;
	virtual void GetScoreByCacheSet(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version) = 0;
	virtual void GetScoreByCacheSetSearchAfter(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version) = 0;
	virtual void GetRangeIndexInTerminal(RANGTYPE range_type, const InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, const TerminalQryCond& query_cond, std::vector<InvertIndexEntry>& entry) = 0;
	virtual bool InsertIndex(const InvertIndexEntry& entry) = 0;
	virtual bool DeleteIndex(const InvertIndexEntry& entry) = 0;
	virtual void DumpIndex() = 0;
	virtual  ~SearchIndex() {};
protected:
	std::string GetIndexAppidField(const InvertIndexEntry& entry) {
		std::ostringstream oss_index;
		oss_index << entry._InvertIndexAppid << "_" << entry._InvertIndexField;
		return oss_index.str();
	};
};


class SequenceSetIndex : public SearchIndex {
public:
	SequenceSetIndex();
	virtual bool Init(){
		return true;
	}
	virtual void GetRangeIndex(uint32_t range_type, InvertIndexEntry &begin_key, InvertIndexEntry &end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGELE( InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGE(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexLE(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGTLT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGTLE(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGELT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGT(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexLT(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetScoreByCacheSet(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version);
	virtual void GetScoreByCacheSetSearchAfter(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version);
	virtual void GetRangeIndexInTerminal(RANGTYPE range_type, const InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, const TerminalQryCond& query_cond, std::vector<InvertIndexEntry>& entry);
	virtual bool InsertIndex(const InvertIndexEntry& entry);
	virtual bool DeleteIndex(const InvertIndexEntry& entry);
	virtual void DumpIndex();
	virtual ~SequenceSetIndex() {
		pthread_rwlock_destroy(_RwMutex); 
		delete _RwMutex;
	};

	
private:
	pthread_rwlock_t* _RwMutex;
	std::map<std::string, std::set<InvertIndexEntry> > _SearchIndexs;
};

struct TableField{
	int index;
    int type;  
};


class CConfig;
class SearchRocksDBIndex : public SearchIndex {
public:
	SearchRocksDBIndex();
	virtual ~SearchRocksDBIndex() {
		if(uds_client != NULL){
			delete uds_client;
		}
	};
	virtual bool Init();
	virtual void GetRangeIndex(uint32_t range_type, InvertIndexEntry &begin_key, InvertIndexEntry &end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGELE( InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGE(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexLE(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGTLT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGTLE(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGELT(InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexGT(InvertIndexEntry& begin_key, std::vector<InvertIndexEntry>& entry);
	virtual void GetRangeIndexLT(InvertIndexEntry& end_key, std::vector<InvertIndexEntry>& entry);

	virtual void GetScoreByCacheSet(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version);
	virtual void GetScoreByCacheSetSearchAfter(const CacheQueryInfo &query_info, const set<string>& valid_docs, SkipList& skipList, map<string, uint32_t>& valid_version);
	virtual void GetRangeIndexInTerminal(RANGTYPE range_type, const InvertIndexEntry& begin_key, const InvertIndexEntry& end_key, const TerminalQryCond& query_cond, std::vector<InvertIndexEntry>& entry);
	virtual bool InsertIndex(const InvertIndexEntry& entry);
	virtual bool DeleteIndex(const InvertIndexEntry& entry);
	virtual void DumpIndex();

private:
	int getFieldIndex(const char *fieldName);
	void buildTableFieldMap(CConfig* _DTCTableConfig);
	void setEntry(DirectRequestContext& direct_request_context, std::vector<InvertIndexEntry>& entry);
	void setQueryCond(QueryCond& query_cond, int field_index, int cond_opr, int cond_val);
private:
	UDSClient* uds_client;
	std::map<string, struct TableField* > table_field_map;
	std::vector<struct TableField* > table_field_vec;
};


#endif // ! __SEQUENCE_SEARCH_INDEX_H__



