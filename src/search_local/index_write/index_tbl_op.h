/*
 * =====================================================================================
 *
 *       Filename:  index_tbl_op.h
 *
 *    Description:  IndexConf class definition.
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

#ifndef INDEX_TBL_OP_H
#define INDEX_TBL_OP_H

#include "log.h"
#include "dtcapi.h"
#include "split_tool.h"
#include "index_conf.h"
#include "json/json.h"
#include "dtc_tools.h"
#include "comm.h"

class CIndexTableManager
{
public:
	int InitServer(const SDTCHost &dtchost);
	bool DeleteIndex(std::string word, const std::string& doc_id, uint32_t doc_version, uint32_t field);
	int delete_snapshot_dtc(const string &doc_id, const uint32_t& appid);
	int delete_hanpin_index(string key, string doc_id);
	int get_snapshot_active_doc(const UserTableContent &fields, int &trans_version);
	int do_insert_index(std::map<std::string, WordProperty>& word_map, const InsertParam& insert_param, Json::Value &res);
	int insert_index_dtc(const WordProperty& word_property, const InsertParam& insert_param, Json::Value &res);
	int do_insert_intelligent(string key, string doc_id, string word, const vector<IntelligentInfo> & info_vec, int doc_version);
	int update_sanpshot_dtc(const UserTableContent &fields,int trans_version,int &affected_rows);
	int update_sanpshot_dtc(uint32_t appid, string doc_id, int trans_version);
	int update_snapshot_version(const UserTableContent &fields,int trans_version,int &affected_rows);
	int insert_snapshot_version(const UserTableContent &fields,int doc_version);
	int update_docid_index_dtc(const string & invert_keys, const string & doc_id, uint32_t appid, int doc_version);
	int insert_docid_index_dtc(const string & invert_keys, const string & doc_id, uint32_t appid, int doc_version);
	int insert_union_index_dtc(const string & union_key, const string & doc_id, uint32_t appid, int doc_version);
	int delete_docid_index_dtc(const string & key, const string & doc_id);
	bool GetIndexData(const std::string& doc_id, uint32_t old_doc_version, map<uint32_t, vector<string> > &res);
	bool delete_index(std::string word, const std::string& doc_id, uint32_t doc_version, uint32_t field);
	bool delete_intelligent(std::string key, std::string doc_id, uint32_t trans_version);

	int Dump_Original_Request(const Json::Value& _requst);
private:
	DTC::Server server;
};

extern CIndexTableManager g_IndexInstance;
extern CIndexTableManager g_delIndexInstance;
extern CIndexTableManager g_hanpinIndexInstance;
extern CIndexTableManager g_originalIndexInstance;

class DeleteItem {
public:
	friend class DeleteTask;
	DeleteItem() :_Next(NULL) {}
	bool operator==(const DeleteItem& a) {
		return this->word == a.word &&
			this->doc_id == a.doc_id &&
			this->doc_version == a.doc_version &&
			this->field == a.field;
	}
private:
	std::string word;
	std::string doc_id;
	uint32_t doc_version;
	uint32_t field;
	DeleteItem *_Next;
};

class DeleteTask{
public:
	static DeleteTask& GetInstance() {
		static DeleteTask instance;
		return instance;
	}

	bool Initialize();
	void RegisterInfo(const std::string& word, const std::string& doc_id, uint32_t doc_version, uint32_t field);

private:
	pthread_t _ReportThread;
	pthread_cond_t _NotEmpty;
	pthread_mutex_t _Mutex;
	DeleteItem *_InfoHead;
	DeleteItem *_InfoTail;
	bool _StopFlag;

private:
	static void *ProcessCycle(void *arg);
	DeleteTask() {
		pthread_mutex_init(&_Mutex, NULL);
		pthread_cond_init(&_NotEmpty, NULL);
		pthread_create(&_ReportThread, NULL, ProcessCycle, NULL);
		_StopFlag = false;
	}
	~DeleteTask() {
		_StopFlag = true;
		pthread_cond_signal(&_NotEmpty);
		pthread_join(_ReportThread, NULL);
	}
	void Coalesce(DeleteItem *head, std::vector<DeleteItem>& temp_result);
	
	void PushReportItem(DeleteItem* item) {
		pthread_mutex_lock(&_Mutex);
		if (_InfoHead == NULL) {
			_InfoHead = _InfoTail = item;
		}
		else {
			_InfoTail->_Next = item;
			_InfoTail = item;
		}

		pthread_cond_signal(&_NotEmpty);
		pthread_mutex_unlock(&_Mutex);
	}
};

#endif