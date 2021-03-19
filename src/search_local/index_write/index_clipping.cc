/*
 * =====================================================================================
 *
 *       Filename:  index_clipping.cc
 *
 *    Description:  IndexClipping class definition.
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

#include <iostream>
#include <string>
#include "log.h"
#include "index_clipping.h"

IndexClipping::IndexClipping(DTC::Server* server) {
	indexSet.clear();
	snapshot_server = server;
}

IndexClipping::~IndexClipping() {
}

static int get_snapshot_execute(DTC::Server* dtc_server,string &doc_id,uint32_t appid,int doc_version,DTC::Result &rst,int top){
	DTC::GetRequest getReq(dtc_server);
	int ret = 0;

	ret = getReq.SetKey(doc_id.c_str());
	ret = getReq.EQ("doc_version",doc_version);
	ret = getReq.EQ("appid",appid);
	ret = getReq.EQ("active",1);
	ret = getReq.EQ("top",top);

	ret = getReq.Execute(rst);
	return ret;
}

bool IndexClipping::is_active_doc(string &doc_id,uint32_t appid,int doc_version,int top){
	int ret;
	DTC::Result rst;
	ret = get_snapshot_execute(snapshot_server,doc_id,appid,doc_version,rst,top);
	if (ret != 0) {
		if (ret == -110) {
			rst.Reset();
			ret = get_snapshot_execute(snapshot_server,doc_id,appid,doc_version,rst,top);
			if (ret != 0) {
				log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
				return true;//not clipping
			}
		}
		else {
			log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
			return true;//not clipping
		}
	}
	struct index_item item;
	if (rst.NumRows() <= 0) {
		return false;
	}
	else {
		return true;
	}

	return true;
}

bool IndexClipping::do_delete_index_dtc(DTC::Server* dtc_server, string key, const struct index_item& item){
	int ret = 0;

	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(key.c_str());
	ret = deleteReq.EQ("doc_id", item.doc_id.c_str());
	ret = deleteReq.EQ("created_time", item.created_time);
	ret = deleteReq.EQ("field", item.field);
	ret = deleteReq.EQ("doc_version",item.doc_version);

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return false;
	}
	log_debug("delete key = %s doc_id = %s doc_verson = %d field = %d createdtime = %d",key.c_str(),item.doc_id.c_str(),item.doc_version,item.field,item.created_time);

	return true;
}

bool IndexClipping::do_delete_top_index_dtc(DTC::Server* dtc_server,string key, const struct index_item&item){
	int ret = 0;

	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(key.c_str());
	ret = deleteReq.EQ("doc_id", item.doc_id.c_str());
	ret = deleteReq.EQ("created_time", item.created_time);
	ret = deleteReq.EQ("doc_version",item.doc_version);

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return false;
	}
	log_debug("delete key = %s doc_id = %s doc_verson = %d createdtime = %d",key.c_str(),item.doc_id.c_str(),item.doc_version,item.created_time);


	return true;
}

bool IndexClipping::do_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit){
	if(indexSet.size() <= ((rows_limit * 80) / 100)){
		indexSet.clear();
		return true;
	}
	uint64_t slipping_count = indexSet.size() - ((rows_limit * 80) / 100);
	set<struct index_item>::iterator it = indexSet.begin();
	for(uint count = 0;it != indexSet.end() && count < slipping_count;it++){
		if(!do_delete_index_dtc(dtc_server,key,*it))
			log_error("do delete dtc error!!");
		count ++;
	}
	indexSet.clear();
	return true;
}

bool IndexClipping::do_top_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit){
	if(indexSet.size() <= ((rows_limit * 80) / 100)){
		indexSet.clear();
		return true;
	}
	uint64_t slipping_count = indexSet.size() - ((rows_limit * 80) / 100);
	set<struct index_item>::iterator it = indexSet.begin();
	for(uint count = 0;it != indexSet.end() && count < slipping_count;it++){
		if(!do_delete_top_index_dtc(dtc_server,key,*it))
			log_error("do delete dtc error!!");
		count ++;
	}
	indexSet.clear();
	return true;
}

static int get_index_dtc_execute(DTC::Server* dtc_server,string key, DTC::Result &rst){
	int ret = 0;
	DTC::GetRequest getReq(dtc_server);

	ret = getReq.SetKey(key.c_str());

	ret = getReq.Need("created_time");
	ret = getReq.Need("doc_id");
	ret = getReq.Need("field");
	ret = getReq.Need("word_freq");
	ret = getReq.Need("doc_version");
	ret = getReq.Execute(rst);
	return ret;
}

bool IndexClipping::get_rows_and_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit){
	log_debug("get_rows_and_index_clipping start!");
	int ret;
	pair<set<struct index_item>::iterator,bool> ret_p;
	DTC::Result rst;
	ret = get_index_dtc_execute(dtc_server,key,rst);
	if (ret != 0) {
		if (ret == -110) {
			rst.Reset();
			ret = get_index_dtc_execute(dtc_server,key,rst);
			if (ret != 0) {
				log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
				return false;
			}
		}
		else {
			log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
			return false;
		}
	}
	int cnt = rst.NumRows();
	if (rst.NumRows() <= 0) {
		log_debug("no data in this node");
		return false;
	}
	else {
		for (int i = 0; i < cnt; i++) {
			rst.FetchRow();
			struct index_item item;
			item.created_time = rst.IntValue("created_time");
			item.doc_id = rst.StringValue("doc_id");
			item.field = rst.IntValue("field");
			item.freq = rst.IntValue("word_freq");
			item.doc_version = rst.IntValue("doc_version");
			ret_p = indexSet.insert(item);
			if(ret_p.second == false){
				if(!do_delete_index_dtc(dtc_server,key,item))
					log_error("do delete dtc error!");
			}
		}

	}
	return do_index_clipping(dtc_server,key,rows_limit);
}

static int get_top_index_dtc_execute(DTC::Server* dtc_server,string key, DTC::Result &rst){
	int ret = 0;
	DTC::GetRequest getReq(dtc_server);

	ret = getReq.SetKey(key.c_str());

	ret = getReq.Need("created_time");
	ret = getReq.Need("doc_id");
	ret = getReq.Need("doc_version");
	ret = getReq.Need("end_time");
	ret = getReq.Execute(rst);
	return ret;
}

bool IndexClipping::get_rows_and_top_index_clipping(DTC::Server* dtc_server,string key,uint32_t rows_limit){
	log_debug("get_rows_and_top_index_clipping start!");
	int ret;
	time_t now_time = time(NULL);
	pair<set<struct index_item>::iterator,bool> ret_p;
	DTC::Result rst;
	ret = get_top_index_dtc_execute(dtc_server,key,rst);
	if (ret != 0) {
		if (ret == -110) {
			rst.Reset();
			ret = get_top_index_dtc_execute(dtc_server,key,rst);
			if (ret != 0) {
				log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
				return false;
			}
		}
		else {
			log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
			return false;
		}
	}
	int cnt = rst.NumRows();
	struct index_item item;
	if (rst.NumRows() <= 0) {

	}
	else {
		for (int i = 0; i < cnt; i++) {
			rst.FetchRow();
			item.created_time = rst.IntValue("created_time");
			item.doc_id = rst.StringValue("doc_id");
			item.freq = 0;
			item.field = 0;
			item.doc_version = rst.IntValue("doc_version");
			item.end_time = rst.IntValue("end_time");
			if(item.end_time < now_time){
				if(!do_delete_top_index_dtc(dtc_server,key,item))
					log_error("do delete dtc error!");
				continue;
			}
			ret_p = indexSet.insert(item);
			if(ret_p.second == false){
				if(!do_delete_top_index_dtc(dtc_server,key,item))
					log_error("do delete dtc error!");
			}
		}

	}
	return do_top_index_clipping(dtc_server,key,rows_limit);
}
