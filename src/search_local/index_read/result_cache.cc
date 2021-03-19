/*
 * =====================================================================================
 *
 *       Filename:  result_cache.h
 *
 *    Description:  result cache class definition.
 *
 *        Version:  1.0
 *        Created:  22/05/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "result_cache.h"
#include <sstream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include "search_conf.h"
#include "result_cache.h"
#include "log.h"
#include "cachelist_unit.h"
#include "memcheck.h"
#include "data_manager.h"
#include "db_manager.h"

using namespace std;

CCacheListUnit *cachelist = NULL;
CTimerList     *cachelist_timer = NULL;

CCacheListUnit *indexcachelist = NULL;
CTimerList	   *index_cachelist_time = NULL;

CacheOperator::CacheOperator(CPollThread *thread, int interval) : m_Owner(thread)
{
	this->m_interval = interval;
	if (m_Owner) {
		m_Timer = m_Owner->GetTimerList(this->m_interval);
	}

	if (cachelist_timer == NULL){
		cachelist_timer = m_Timer;
	} else {
		index_cachelist_time = m_Timer;
	}
}

int CacheOperator::Init(int expired, int max_slot) {
	NEW(CCacheListUnit(cachelist_timer), cachelist);
	if (NULL == cachelist || cachelist->init_list(max_slot, 0, expired))
	{
		log_error("init cachelist failed");
		return -1;
	}

	cachelist->start_list_expired_task();
	return 0;
}

int CacheOperator::InitIndex(int expired, int max_slot) {
	NEW(CCacheListUnit(index_cachelist_time), indexcachelist);
	if (NULL == indexcachelist || indexcachelist->init_list(max_slot, 0, expired))
	{
		log_error("init indexcachelist failed");
		return -1;
	}
	indexcachelist->start_list_expired_task();
	return 0;
}

CacheOperator::~CacheOperator() 
{
	if (cachelist != NULL)
		DELETE(cachelist);
	else 
		DELETE(indexcachelist);
}

UpdateOperator::UpdateOperator(CPollThread *thread, int interval, std::string appFieldFile) : m_Owner(thread)
{
	m_updateTimer = NULL;
	this->m_interval = interval;
	this->m_appFieldFile = appFieldFile;
	if (m_Owner) {
		m_updateTimer = m_Owner->GetTimerList(this->m_interval);
	}
} 

void UpdateOperator::TimerNotify(void)
{
	log_debug("update table info start");
	static long lastModifyTime = 0;
	FILE * fp = fopen(m_appFieldFile.c_str(), "r");
	if(NULL == fp){
		log_debug("open file[%s] error.", m_appFieldFile.c_str());
		AttachTimer(m_updateTimer);
		return ;
	}
	int fd = fileno(fp);
	struct stat buf;
	fstat(fd, &buf);
	long modifyTime = buf.st_mtime;
	fclose(fp);
	if(modifyTime != lastModifyTime){
		lastModifyTime = modifyTime;
		DBManager::Instance()->ReplaceAppFieldInfo(m_appFieldFile);
	} else {
		log_debug("file[%s] not change.", m_appFieldFile.c_str());
	}
	AttachTimer(m_updateTimer);
	return ;
}

UpdateOperator::~UpdateOperator()
{
}