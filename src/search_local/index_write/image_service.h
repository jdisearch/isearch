/*
 * =====================================================================================
 *
 *       Filename:  image_service.h
 *
 *    Description:  CTaskImage class definition.
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

#ifndef IMAGE_SERVICE_H_
#define IMAGE_SERVICE_H_
#include <set>
#include <vector>
#include <sstream>
#include "request_base.h"
#include "index_conf.h"
#include "dtcapi.h"
#include "comm.h"
#include "split_manager.h"
using namespace std;


class CPollThread;
class CTaskRequest;
class SplitManager;
class DTCServers;

class CTaskImage : public CTaskDispatcher<CTaskRequest>
{
private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;
	DTC::DTCServers index_servers;

private:
	int insert_index_dtc(DTC::Server* dtcServer,string key,struct item &it,u_int8_t field_type,int doc_version,Json::Value &res);
	int do_insert_index(DTC::Server* dtcServer, map<string, item> &word_map, map<string, item> &title_map,uint64_t app_id,int doc_version,Json::Value &res);
	void do_stat_word_freq(vector<vector<string> > &strss, string &doc_id, uint32_t appid, map<string, item> &word_map,Json::Value &res);
	int get_snapshot_active_doc(const UserTableContent &fields,int &active,Json::Value &res);
	int delete_snapshot_dtc(string &doc_id,uint32_t appid,Json::Value &res);
	int insert_snapshot_dtc(const UserTableContent &fields,int &doc_version,Json::Value &res);
	int update_sanpshot_dtc(const UserTableContent &fields,int doc_version,Json::Value &res);

public:
	CTaskImage(CPollThread * o);
	virtual ~CTaskImage();
	int index_gen_process(Json::Value &req,Json::Value &res);
	int pre_process(void);

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p)
	{
		output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};



#endif /* IMAGE_SERVICE_H_ */
