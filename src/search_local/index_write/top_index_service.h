/*
 * =====================================================================================
 *
 *       Filename:  top_index_service.h
 *
 *    Description:  class definition.
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

#ifndef TOP_INDEX_SERVICE_H_
#define TOP_INDEX_SERVICE_H_

#include <set>
#include <vector>
#include <sstream>
#include "request_base.h"
#include "index_conf.h"
#include "dtcapi.h"
#include "split_manager.h"
using namespace std;

class CPollThread;
class CTaskRequest;
class SplitManager;
class DTCServers;

class CTaskTopIndex : public CTaskDispatcher<CTaskRequest>
{
    private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;
	DTC::DTCServers index_servers;

    private:
	int insert_top_index_dtc(string key,const UserTableContent &fields,int doc_version,Json::Value &res);
	int do_insert_top_index(const UserTableContent &fields,int doc_version, set<string> &word_set,Json::Value &res);
	int get_snapshot_active_doc(const UserTableContent &fields,int &doc_version,Json::Value &res);
	int delete_snapshot_dtc(const string &doc_id,uint32_t appid,Json::Value &res);
	int insert_snapshot_dtc(const UserTableContent &fields,int &doc_version,Json::Value &res);
	int do_split_sp_words(string &str, string &doc_id, uint32_t appid, set<string> &word_set,Json::Value &res);
	int update_sanpshot_dtc(const UserTableContent &fields,int doc_version,Json::Value &res);

    public:
	CTaskTopIndex(CPollThread * o);
	virtual ~CTaskTopIndex();
	int pre_process(void);
	int top_index_process(Json::Value &req,Json::Value &res);

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p)
	{
	    output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};



#endif /* TOP_INDEX_SERVICE_H_ */
