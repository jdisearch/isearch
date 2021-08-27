#include "sync_unit.h"
#include "log.h"
#include "comm.h"
#include "config_center_parser.h"
#include "system_state.h"
#include "indexgen_reporter.h"

CFullSync::CFullSync(DTC::Server* pServer)
	: m_iLimit(1)
	, m_pMaster(pServer)
	, m_oJournalID()
{
	bzero(m_sErrMsg, sizeof(m_sErrMsg));
} 

CFullSync::~CFullSync()
{ }

int CFullSync::Run()
{
	/* 先关闭连接，防止fd重路 */
	m_pMaster->Close();

	m_oJournalID = CComm::registor.JournalId();
	struct timeval now;
	gettimeofday(&now, NULL);
	int iSec = now.tv_sec + 1;

	while (true) {
		usleep(1000000); // 1s
		gettimeofday(&now, NULL);
		if (now.tv_sec >= iSec) {
			if (CComm::registor.CheckMemoryCreateTime()) {
				log_error("detect share memory changed");
			}

			// 主备切换状态判断
			if (SystemState::Instance()->GetSwitchState()) {
				return E_FULL_SYNC_MS_SWITCH;
			}
			iSec += 1;
		}

		DTC::SvrAdminRequest request_m(m_pMaster);
		request_m.SetAdminCode(DTC::GetUpdateKey);

		request_m.Need("type");
		request_m.Need("flag");
		request_m.Need("key");
		request_m.Need("value");
		request_m.SetHotBackupID((uint64_t)m_oJournalID);

		request_m.Limit(0, m_iLimit);

		DTC::Result result_m;
		int ret = request_m.Execute(result_m);
		log_error("aliving....., return:%d", ret);

		if (-DTC::EC_BAD_HOTBACKUP_JID == ret) {
			log_error("master report journalID is not match");
		}

		// 出错，需要重试
		if (0 != ret) {
			log_error("fetch key-list from master failed, limit[%d], ret=%d, err=%s",
			m_iLimit, ret, result_m.ErrorMessage());
			
			snprintf(m_sErrMsg, sizeof(m_sErrMsg),
				 "fetch key-list from master failed, limit[%d], ret=%d, err=%s",
				  m_iLimit, ret, result_m.ErrorMessage());
			usleep(100);
			continue;
		}
		// 同步到备机
		for (int i = 0; i < result_m.NumRows(); ++i) {
			ret = result_m.FetchRow();
			if (ret < 0) {
				snprintf(m_sErrMsg, sizeof(m_sErrMsg),
					 "fetch key-list from master failed, limit[%d], ret=%d, err=%s",
					  m_iLimit, ret, result_m.ErrorMessage());

				// dtc可以运行失败，但bitmapsvr是不允许失败的
				return E_FULL_SYNC_DTC_ERROR;
			}

			int iKeyLen = 0;
			int iKey = *(int *)result_m.BinaryValue("key", iKeyLen);

			int iValueLen = 0;
			char* pValue = (char *)result_m.BinaryValue("value", iValueLen);

			RawFormat* pRawFormat = (RawFormat*)pValue;
			log_debug("DataType:%c,DataSize:%d,RowCnt:%d,GetCount:%d,LastAccessHour:%d,LastUpdateHour:%d,CreateHour:%d",
			pRawFormat->m_uchDataType,pRawFormat->m_uiDataSize,
			pRawFormat->m_uiRowCnt,pRawFormat->m_uchGetCount,
			pRawFormat->m_LastAccessHour,pRawFormat->m_LastUpdateHour,pRawFormat->m_CreateHour
			);
			pValue += sizeof(RawFormat) + iKeyLen + sizeof(char);

			int iConLength = *(int*)pValue;
			pValue += sizeof(int);
			std::string sValue(pValue,iConLength);
			log_error("key:%d,keylen:%d,value:%s,contentlen:%d" ,iKey,iKeyLen, sValue.c_str(), iConLength);

			if (IndexgenReporter::Instance()->HandleAndReporte2IG(sValue))
			{
				log_error("fullSync fetch key:%d failed", iKey);
				return E_FULL_SYNC_INDEXGEN_CONNECT_ERROR;
			}

			// 成功，则更新控制文件中的journalID
			m_oJournalID = (uint64_t)result_m.HotBackupID();
			CComm::registor.JournalId() = m_oJournalID;
		}
	}

	return E_FULL_SYNC_NORMAL_EXIT;
}

//***************************分割线***************************
CFullSyncUnit::CFullSyncUnit()
	: m_pFullSync(NULL)
{ } 

CFullSyncUnit::~CFullSyncUnit()
{
	DELETE(m_pFullSync);
}

bool CFullSyncUnit::Run(DTC::Server* m , int limit)
{
		log_notice("\"MEMORY-FULL-SYNC\" is start");
		
		if (NULL == m_pFullSync)
		{
			m_pFullSync = new CFullSync(m);
			if (!m_pFullSync) {
			log_error
			    ("full-sync is __NOT__ complete, plz fixed, err: create CFullSync obj failed");
			return false;
			}
		}

		m_pFullSync->SetLimit(limit);

		int iRet = m_pFullSync->Run();

		log_notice("\"MEMORY-FULL-SYNC\" is stop ,errorid:%d" , iRet);
		return (E_FULL_SYNC_MS_SWITCH == iRet);

}
