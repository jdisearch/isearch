/*
 * =====================================================================================
 *
 *       Filename:  sync_unit.h
 *
 *    Description:  sync_unit class definition.
 *
 *        Version:  1.0
 *        Created:  13/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SYNC_UNIT_H
#define __SYNC_UNIT_H

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "async_file.h"
#include "dtcapi.h"

enum E_FULL_SYNC_ERROR_ID
{
	E_FULL_SYNC_NORMAL_EXIT,
	E_FULL_SYNC_MS_SWITCH = -100,
	E_FULL_SYNC_DTC_ERROR,
	E_FULL_SYNC_INDEXGEN_CONNECT_ERROR
};

struct RawFormat
{
	unsigned char m_uchDataType; // 数据类型EnumDataType
	uint32_t m_uiDataSize;		 // 数据总大小
	uint32_t m_uiRowCnt;		 // 行数
	uint8_t m_uchGetCount;		 // get次数
	uint16_t m_LastAccessHour;	 // 最近访问时间
	uint16_t m_LastUpdateHour;	 // 最近更新时间
	uint16_t m_CreateHour;		 // 创建时间
} __attribute__((packed));

class CFullSync
{
public:
	CFullSync(DTC::Server* pServer);
	~CFullSync();

	/// *******************************
	/// start fullSync process
	/// *******************************
	int Run();

	/// *******************************
	/// set max fetch limit per query
	/// *******************************
	void SetLimit(int iLimit) {
		// Here we need sync one by one
		m_iLimit = iLimit;
	}

	/// *******************************
	/// get error message
	/// *******************************
	const char* ErrorMessage() {
		return m_sErrMsg;
	}

private:
	int m_iLimit;
	DTC::Server* m_pMaster;
	JournalID m_oJournalID;
	char m_sErrMsg[256];
};

class CFullSyncUnit {
public:
	CFullSyncUnit();
	~CFullSyncUnit();

	bool Run(DTC::Server* m , int limit);

private:
	CFullSync* m_pFullSync;
};

#endif
