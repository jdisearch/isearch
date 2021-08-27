/*
 * =====================================================================================
 *
 *       Filename:  max_heap.h
 *
 *    Description:  max heap class definition.
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

#ifndef __MAX_HEAP_H__
#define __MAX_HEAP_H__

#include "../comm.h"
#include <math.h>
#include <iomanip>
#include <iostream>
#include <sstream>

template <typename Target, typename Source>
Target lexical_cast(Source arg) {
	std::stringstream interpreter;
	Target result;
	if (!(interpreter << arg) || !(interpreter >> result) ||
		!(interpreter >> std::ws).eof()) {
		return Target();
	}
	return result;
}

template<typename T>
struct DocIdEntry {
	std::string DocId;
	T Score;
	FIELDTYPE FieldType;
	uint32_t  SortType;

	DocIdEntry(){
	}

	DocIdEntry(const std::string& doc_id, T score, FIELDTYPE field_type, uint32_t sort_type) {
		DocId = doc_id;
		Score = score;
		FieldType  = field_type;
		SortType = sort_type;
	}

	DocIdEntry(const DocIdEntry& src) {
		this->DocId = src.DocId;
		this->Score = src.Score;
		this->FieldType = src.FieldType;
		this->SortType = src.SortType;
	}

	DocIdEntry& operator=(const DocIdEntry& src) {
		this->DocId = src.DocId;
		this->Score = src.Score;
		this->FieldType = src.FieldType;
		this->SortType = src.SortType;
		return *this;
	}

	bool operator<(const DocIdEntry &other) const {
		bool tmp, tmp1, tmp2;
		if (FieldType == FIELDTYPE_INT) {
			tmp = Score == other.Score;
			tmp1 = Score < other.Score;
			tmp2 = Score > other.Score;
		}
		else if (FieldType == FIELDTYPE_STRING) {
			std::string lhs = lexical_cast<std::string, T>(Score);
			std::string rhs = lexical_cast<std::string, T>(other.Score);
			tmp = lhs.compare(rhs) == 0;
			tmp1 = lhs.compare(rhs) < 0;
			tmp2 = lhs.compare(rhs) > 0;
		}
		else {
			double lhs = lexical_cast<double, T>(Score);
			double rhs = lexical_cast<double, T>(other.Score);
			tmp = fabs(lhs -rhs) < 0.0000001;
			tmp1 = lhs- rhs < 0;
			tmp2 = lhs- rhs > 0;
		}

		if (SortType == SORT_FIELD_ASC) {
			return (tmp == true) ? (DocId.compare(other.DocId) < 0) : (tmp1);
		}
		else {
			return (tmp == true) ? (DocId.compare(other.DocId) > 0) : (tmp2);
		}
	}

	bool operator==(const DocIdEntry &other) const {
		if (FieldType == FIELDTYPE_INT) {
			uint64_t lhs = lexical_cast<uint64_t, T>(Score);
			uint64_t rhs = lexical_cast<uint64_t, T>(other.Score);
			return  lhs == rhs;
		}
		else if (FieldType == FIELDTYPE_STRING) {
			std::string lhs = lexical_cast<std::string, T>(Score);
			std::string rhs = lexical_cast<std::string, T>(other.Score);
			return lhs.compare(rhs) == 0;
		}
		else {
			double lhs = lexical_cast<double, T>(Score);
			double rhs = lexical_cast<double, T>(other.Score);
			return fabs(lhs - rhs) < 0.0000001;
		}
	}
};

template<typename T>
class CMaxHeap {
public:
	std::vector<DocIdEntry<T> > getNumbers(vector<DocIdEntry<T> >& arr, const DocIdEntry<T>& last_value, int k);
	std::vector<DocIdEntry<T> > getNumbers(vector<DocIdEntry<T> >& arr, int k);

private:
	void buildMaxHeap(vector<DocIdEntry<T> >& v);
	void emplacePeek(vector<DocIdEntry<T> >& v, const DocIdEntry<T>& val);
	void downAdjust(vector<DocIdEntry<T> >& v, int index);
};


template<typename T>
std::vector<DocIdEntry<T> > CMaxHeap<T>::getNumbers(vector<DocIdEntry<T> >& arr, const DocIdEntry<T>& last_value, int k)
{
	if(k == 0){
		return vector<DocIdEntry<T> >();
	}
	vector<DocIdEntry<T> > max_heap_vec;
	int i = 0;
	for(; i < (int)arr.size(); ++i){
		if(last_value < arr[i]){
			max_heap_vec.push_back(arr[i]);
		}
		if((int)max_heap_vec.size() >= k){
			break;
		}
	}

	if (max_heap_vec.empty()){
		return max_heap_vec;
	}
	
	buildMaxHeap(max_heap_vec);
	for(int i = k; i < (int)arr.size(); ++i){
		// 出现比堆顶元素小且大于last_value的值, 置换堆顶元素, 并调整堆
		// 注意：由于DocIdEntry的operator<重载实现中已经考虑了SortType，因此这里的小于不一定是直观上数字的小于
		if(arr[i] < max_heap_vec[0] && last_value < arr[i]){
			emplacePeek(max_heap_vec, arr[i]);
		}
	}
	return max_heap_vec;
}

template<typename T>
std::vector<DocIdEntry<T> > CMaxHeap<T>::getNumbers(vector<DocIdEntry<T> >& arr, int k)
{
	if(k == 0){
		return vector<DocIdEntry<T> >();
	}
	vector<DocIdEntry<T> > max_heap_vec;
	for(int i = 0; i < (int)arr.size(); ++i){ // 如果arr.size()比k小，则将arr全部赋值给max_heap_vec
		max_heap_vec.push_back(arr[i]);
		if((int)max_heap_vec.size() >= k){
			break;
		}
	}
	
	if (max_heap_vec.empty()){
		return max_heap_vec;
	}

	buildMaxHeap(max_heap_vec);
	for(int i = k; i < (int)arr.size(); ++i){
		// 出现比堆顶元素小的值, 置换堆顶元素, 并调整堆
		if(arr[i] < max_heap_vec[0]){
			emplacePeek(max_heap_vec, arr[i]);
		}
	}
	return max_heap_vec;
}

template<typename T>
void CMaxHeap<T>::buildMaxHeap(vector<DocIdEntry<T> >& v){
	// 所有非叶子节点从后往前依次下沉
	for(int i = (v.size()-2) / 2; i >= 0; --i){
		downAdjust(v, i);
	}
}

template<typename T>
void CMaxHeap<T>::emplacePeek(vector<DocIdEntry<T> >& v, const DocIdEntry<T>& val){
	v[0] = val;
	downAdjust(v, 0);
}

template<typename T>
void CMaxHeap<T>::downAdjust(vector<DocIdEntry<T> >& v, int index){
	DocIdEntry<T> parent = v[index];
	// 左孩子节点索引
	int child_index = 2*index + 1;
	while(child_index < (int)v.size()){
		// 判断是否存在右孩子, 并选出较大的节点
		if(child_index + 1 < (int)v.size() &&  v[child_index] < v[child_index+1]){
			child_index += 1;
		}
		// 判断父节点和子节点的大小关系
		if(v[child_index] < parent || v[child_index] == parent){
			break;
		}
		// 较大节点上浮
		v[index] = v[child_index];
		index = child_index;
		child_index = 2*index + 1;
	}
	v[index] = parent;
}

#endif