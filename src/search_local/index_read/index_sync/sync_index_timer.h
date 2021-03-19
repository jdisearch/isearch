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

#ifndef  __SYNC_INDEX_TIMER_H__
#define __SYNC_INDEX_TIMER_H__
#include "timerlist.h"
#include "config.h"
#include "log.h"
#include <string>
#include <sstream>
#include <math.h>
#define CACHE_KEY_SPILIT_CHAR "#"
#define INVERT_INDEX_FLAG "00"

const double eps = 1e-6;

class CTimerObject;
class SearchIndex;
class SyncUnit;

struct InvertIndexEntry {
	InvertIndexEntry() { _IsValid = false; }
	InvertIndexEntry(const std::string& original_key) {
		_IsValid = false;
		std::string::size_type pos1, pos2;
		pos1 = 0;
		std::string tmp_result[2];
		for (int i = 0; i < 2; i++) {
			pos2 = original_key.find(CACHE_KEY_SPILIT_CHAR, pos1);
			if (std::string::npos == pos2) return ;
			tmp_result[i] = original_key.substr(pos1, pos2 - pos1);
			pos1 = pos2 + 1;
		}
		if (0 != tmp_result[1].compare(INVERT_INDEX_FLAG)) {
			return;
		}
		_InvertIndexAppid = tmp_result[0];
		std::istringstream iss(original_key.substr(pos1));
		iss >> _InvertIndexKey;
		_IsValid = true;
	}
	
	InvertIndexEntry(std::string appid, int field, double key){
		_InvertIndexAppid = appid;
		_InvertIndexField = field;
		_InvertIndexKey = key;
	}

	InvertIndexEntry(const InvertIndexEntry& src) {
		this->_InvertIndexKey = src._InvertIndexKey;
		this->_InvertIndexDocId = src._InvertIndexDocId;
		this->_InvertIndexAppid = src._InvertIndexAppid;
		this->_InvertIndexField = src._InvertIndexField;
		this->_InvertIndexDocVersion = src._InvertIndexDocVersion;
	    this->_OriginalKey = src._OriginalKey;
		this->_IsValid = src._IsValid;
	}

	InvertIndexEntry& operator=(const InvertIndexEntry& src) {
		this->_InvertIndexKey = src._InvertIndexKey;
		this->_InvertIndexDocId = src._InvertIndexDocId;
		this->_InvertIndexAppid = src._InvertIndexAppid;
		this->_InvertIndexField = src._InvertIndexField;
		this->_InvertIndexDocVersion = src._InvertIndexDocVersion;
		this->_OriginalKey = src._OriginalKey;
		this->_IsValid = src._IsValid;
		return *this;
	}
	
	bool operator<(const InvertIndexEntry& src) const  {
		if (fabs(this->_InvertIndexKey - src._InvertIndexKey) < eps) {
			return this->_InvertIndexDocId.compare(src._InvertIndexDocId) < 0;
		}
		return 	this->_InvertIndexKey + eps < src._InvertIndexKey;
	}

	bool LE(const InvertIndexEntry& src) const  {
		 return (this->_InvertIndexKey + eps < src._InvertIndexKey) || fabs(this->_InvertIndexKey - src._InvertIndexKey) < eps;
	}

	bool LT(const InvertIndexEntry& src) const {
		return (this->_InvertIndexKey + eps < src._InvertIndexKey);
	}

	bool EQ(const InvertIndexEntry& src) const{
		return fabs(this->_InvertIndexKey - src._InvertIndexKey) < eps;
	}


	std::string ToString() const {
		std::ostringstream oss;
		oss << "key\t [" << _InvertIndexKey << "]\t";
		oss << "doc_id\t [" << _InvertIndexDocId << "]";
		return oss.str();
	}

	double _InvertIndexKey;
	std::string _InvertIndexDocId;
	std::string _InvertIndexAppid;
	int _InvertIndexField;
	int _InvertIndexDocVersion;
	std::string _OriginalKey;
	bool _IsValid;
};

class SearchIndex;
class SyncIndexTimer : private CTimerObject  {
public:
	SyncIndexTimer(CTimerList *t) :_Timer(t) {};
	virtual ~SyncIndexTimer(void) {
	
	};
	virtual void TimerNotify(void);
	bool StartSyncTask();
	void DumpIndex();
	SearchIndex *GetSearchIndex() {
		return _SearchIndex;
	}
private:
	CTimerList *_Timer; 
	SyncUnit *_IndexSynUnit;
	CConfig* _DTCCacheConfig;
	SearchIndex *_SearchIndex;
private:
	bool InitConfig();
};






#endif // ! __SYNC_INDEX_TIMER_H__
