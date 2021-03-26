/*
 * =====================================================================================
 *
 *       Filename:  order_op.h
 *
 *    Description:  COrderOp class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */#ifndef __ORDER_OP_H__
#define __ORDER_OP_H__
#include <map>
#include <set>
#include <iterator>
#include <algorithm>
#include "utils/max_heap.h"
#include "json/json.h"


template<typename T>
class COrderOp {
public:

	COrderOp(FIELDTYPE type, int is_search_after, uint32_t sort_type) {
		_OrderFieldType = type;
		_SearchAfter = is_search_after;
		_SortType = sort_type;
		_MaxHeap = new CMaxHeap<T>();
	};
	~COrderOp(){
		if(_MaxHeap != NULL){
			delete _MaxHeap;
		}
	}
	void Process(const std::map<std::string, T>& score_map, T last_value, OrderOpCond order_op_cond, Json::Value& response, DocManager *doc_manager);


private:
	FIELDTYPE _OrderFieldType;
	int _SearchAfter;
	uint32_t _SortType;
	std::set<DocIdEntry<T> > _ScoreSet;
	std::vector<DocIdEntry<T> > _ScoreVec;
	CMaxHeap<T>* _MaxHeap;

private:
	void ProcessSearchAfter(std::set<DocIdEntry<T> >& score_set, DocIdEntry<T>& last_value, int count, Json::Value& response, DocManager *doc_manager);
	void ProcessSearchAfter(std::vector<DocIdEntry<T> >& score_vec, DocIdEntry<T>& last_value, int count, Json::Value& response);
};

template<typename T>
void COrderOp<T>::Process(const std::map<std::string, T>& score_map, T last_value, OrderOpCond order_op_cond, Json::Value& response, DocManager *doc_manager)
{
	DocIdEntry<T> last_entry;
	for (typename std::map<std::string, T>::const_iterator it = score_map.begin(); it != score_map.end(); it++) {
		DocIdEntry<T> doc_entry(it->first, it->second, _OrderFieldType, _SortType);
		if(order_op_cond.has_extra_filter){
			_ScoreSet.insert(doc_entry);
		} else {
			_ScoreVec.push_back(doc_entry);
		}
		if(it->second == last_value && it->first == order_op_cond.last_id){
			last_entry = doc_entry;
		}
	}
	
	if (_SearchAfter) {
		if(order_op_cond.has_extra_filter){
			ProcessSearchAfter(_ScoreSet, last_entry, order_op_cond.count, response, doc_manager);
		} else {
			ProcessSearchAfter(_ScoreVec, last_entry, order_op_cond.count, response);
		}	
	}
	else {
		if(order_op_cond.has_extra_filter){
			int sequence = -1;
			typename std::set<DocIdEntry<T> >::iterator set_it = _ScoreSet.begin();
			for(; set_it != _ScoreSet.end(); set_it++){
				if(doc_manager->CheckDocByExtraFilterKey((*set_it).DocId) == false){
					continue;
				}
				sequence++;
				if (sequence < (int)order_op_cond.limit_start) {
					continue;
				}
				if(sequence > (int)(order_op_cond.limit_start + order_op_cond.count - 1)){ // 数量达到提前终止
					break;
				}
				Json::Value doc_info;
				doc_info["doc_id"] = Json::Value((*set_it).DocId.c_str());
				doc_info["score"] = Json::Value((*set_it).Score);
				response["result"].append(doc_info);
			}
		} else {
			std::vector<DocIdEntry<T> > res;
			res = _MaxHeap->getNumbers(_ScoreVec, order_op_cond.limit_start + order_op_cond.count); // limit_start = page_size*(page_index-1); count = page_size

			// 适配单机版，如果page_index不为1，需要对res进行排序，返回limit_start开始的page_size个数据
			sort(res.begin(), res.end());
			int sequence = -1;
			typename std::vector<DocIdEntry<T> >::iterator vec_it = res.begin();
			for(; vec_it != res.end(); vec_it++){
				sequence++;
				if (sequence < (int)order_op_cond.limit_start) {
					continue;
				}
				if(sequence > (int)(order_op_cond.limit_start + order_op_cond.count - 1)){ // 数量达到提前终止
					break;
				}
				Json::Value doc_info;
				doc_info["doc_id"] = Json::Value((*vec_it).DocId.c_str());
				doc_info["score"] = Json::Value((*vec_it).Score);
				response["result"].append(doc_info);
			}
		}
	}
}

template<typename T>
void COrderOp<T>::ProcessSearchAfter(std::set<DocIdEntry<T> >& score_set, DocIdEntry<T>& last_value, int count, Json::Value& response, DocManager *doc_manager)
{
	typename std::set<DocIdEntry<T> >::iterator set_it;
	if(_SortType == SORT_FIELD_ASC){
		set_it = std::upper_bound(score_set.begin(), score_set.end(), last_value);
	} else {
		set_it = std::upper_bound(score_set.begin(), score_set.end(), last_value);
	}
	if (set_it != score_set.end()) {
		int c = 0;
		for (; set_it != score_set.end() && c < count; set_it++) {
			if(doc_manager->CheckDocByExtraFilterKey((*set_it).DocId) == false){
				continue;
			}
			c++;
			Json::Value doc_info;
			doc_info["doc_id"] = Json::Value((*set_it).DocId.c_str());
			doc_info["score"] = Json::Value((*set_it).Score);
			response["result"].append(doc_info);
		}
	}
}

template<typename T>
void COrderOp<T>::ProcessSearchAfter(std::vector<DocIdEntry<T> >& score_vec, DocIdEntry<T>& last_value, int count, Json::Value& response)
{
	std::vector<DocIdEntry<T> > res;
	res = _MaxHeap->getNumbers(score_vec, last_value, count);
	sort(res.begin(), res.end());
	typename std::vector<DocIdEntry<T> >::iterator vec_it = res.begin();
	int c = 0;
	for (; vec_it != res.end() && c < count; vec_it++,c++) {
		Json::Value doc_info;
		doc_info["doc_id"] = Json::Value((*vec_it).DocId.c_str());
		doc_info["score"] = Json::Value((*vec_it).Score);
		response["result"].append(doc_info);
	}
}


#endif


