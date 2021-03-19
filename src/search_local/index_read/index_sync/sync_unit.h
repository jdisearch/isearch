/*
 * =====================================================================================
 *
 *       Filename:  sync_unit.h
 *
 *    Description:  sync uint class definition.
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

#ifndef  __SYNC_UNIT_H__
#define __SYNC_UNIT_H__

#include <string>
#include "dtcapi.h"
#include "journal_id.h"
#include "sequence_search_index.h"

class CConfig;
class CTableDefinition;

typedef enum  {
	FULL_SYNC,
	INC_SYNC
}SyncStage;

 
class SyncUnit {
public:
	SyncUnit(CConfig *cache_config):
		_CacheConfig(cache_config),
		_Start(0), 
		_Limit(10000)
	{};
	int ConnectDtcServer();
	int RegisterSlave();
	void DoOnceSync(SearchIndex *index);
	
private:
	CConfig *_CacheConfig;
	std::string _MasterAddress;
	std::string _SlaveAddress;
	DTC::Server _MasterDtc;
	CJournalID _JournalId;
	SyncStage _SearchSyncStep;
	unsigned int _Start;
	unsigned int _Limit;
private:
	void DoOnceFullSync(SearchIndex *index);
	void DoOnceIncSync(SearchIndex *index);
	bool UpdateOneIndex(const std::string& index_key, SearchIndex *index);


	

};


#endif
