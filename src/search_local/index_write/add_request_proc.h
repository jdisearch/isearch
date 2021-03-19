/*
 * =====================================================================================
 *
 *       Filename:  add_request_proc.h
 *
 *    Description:  AddReqProc class definition.
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

#ifndef ADD_REQUEST_PROC_H
#define ADD_REQUEST_PROC_H

#include "log.h"
#include "json/json.h"
#include "comm.h"

class UserTableContent;
class SplitManager;
class AddReqProc
{
public:
	AddReqProc();
	AddReqProc(const Json::Value& jf, InsertParam& insert_param);
	~AddReqProc();

	int do_insert_index(UserTableContent& content_fields);

private:
	void do_stat_word_freq(vector<vector<string> > &strss, map<string, item> &word_map, string extend);
	void do_stat_word_freq(vector<string> &strss, map<string, item> &word_map);
	int deal_index_tag(struct table_info *tbinfo, string field_name);
	int roll_back();

private:
	Json::Value json_field;
	uint32_t app_id;
	uint32_t doc_version;
	uint32_t trans_version;
	string doc_id;
	string lng;
	string lat;
	vector<string> lng_arr;
	vector<string> lat_arr;
	vector<string> intelligent_keys;
	Json::Value snapshot_content;
	Json::Value docid_index_map;
};

#endif