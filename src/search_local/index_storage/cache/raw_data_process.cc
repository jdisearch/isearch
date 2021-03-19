/*
 * =====================================================================================
 *
 *       Filename:  raw_data_process.cc
 *
 *    Description:  raw data process interface
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raw_data_process.h"
#include "global.h"
#include "log.h"
#include "sys_malloc.h"
#include "task_pkey.h"
#include "buffer_flush.h"
#include "relative_hour_calculator.h"

DTC_USING_NAMESPACE

RawDataProcess::RawDataProcess(Mallocator *pstMalloc, DTCTableDefinition *pstTab, DTCBufferPool *pstPool, const UpdateMode *pstUpdateMode) : m_stRawData(pstMalloc), m_pstTab(pstTab), m_pMallocator(pstMalloc), m_pstPool(pstPool)
{
	memcpy(&m_stUpdateMode, pstUpdateMode, sizeof(m_stUpdateMode));
	nodeSizeLimit = 0;
	history_datasize = statmgr.get_sample(DATA_SIZE_HISTORY_STAT);
	history_rowsize = statmgr.get_sample(ROW_SIZE_HISTORY_STAT);
}

RawDataProcess::~RawDataProcess()
{
}

int RawDataProcess::init_data(Node *pstNode, RawData *pstAffectedRows, const char *ptrKey)
{
	int iRet;

	iRet = m_stRawData.Init(ptrKey, 0);
	if (iRet != 0)
	{
		log_error("raw-data init error: %d,%s", iRet, m_stRawData.get_err_msg());
		return (-1);
	}
	pstNode->vd_handle() = m_stRawData.get_handle();

	if (pstAffectedRows != NULL)
	{
		iRet = pstAffectedRows->Init(ptrKey, 0);
		if (iRet != 0)
		{
			log_error("raw-data init error: %d,%s", iRet, pstAffectedRows->get_err_msg());
			return (-2);
		}
	}

	return (0);
}

int RawDataProcess::attach_data(Node *pstNode, RawData *pstAffectedRows)
{
	int iRet;

	iRet = m_stRawData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		log_error("raw-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stRawData.get_err_msg());
		return (-1);
	}

	if (pstAffectedRows != NULL)
	{
		iRet = pstAffectedRows->Init(m_stRawData.Key(), 0);
		if (iRet != 0)
		{
			log_error("raw-data init error: %d,%s", iRet, pstAffectedRows->get_err_msg());
			return (-2);
		}
	}

	return (0);
}

int RawDataProcess::get_all_rows(Node *pstNode, RawData *pstRows)
{
	int iRet;

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = attach_data(pstNode, pstRows);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return (-1);
	}

	pstRows->set_refrence(&m_stRawData);
	if (pstRows->copy_all() != 0)
	{
		log_error("copy data error: %d,%s", iRet, pstRows->get_err_msg());
		return (-2);
	}

	return (0);
}

int RawDataProcess::expand_node(TaskRequest &stTask, Node *pstNode)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	// no need to check expand status as checked in CCacheProces

	// save node to stack as new version
	iRet = attach_data(pstNode, NULL);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return -1;
	}
	unsigned int uiTotalRows = m_stRawData.total_rows();
	stpNodeTab = m_stRawData.get_node_table_def();
	stpTaskTab = TableDefinitionManager::Instance()->get_new_table_def();
	if (stpTaskTab == stpNodeTab)
	{
		log_notice("expand one node which is already new version, pay attention, treat as success");
		return 0;
	}
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	RawData stNewTmpRawData(&g_stSysMalloc, 1);
	iRet = stNewTmpRawData.Init(m_stRawData.Key(), m_stRawData.data_size());
	if (iRet != 0)
	{
		log_error("init raw-data struct error, ret = %d, err = %s", iRet, stNewTmpRawData.get_err_msg());
		return -2;
	}
	for (unsigned int i = 0; i < uiTotalRows; ++i)
	{
		unsigned char uchRowFlags;
		if (m_stRawData.decode_row(*stpNodeRow, uchRowFlags, 0) != 0)
		{
			log_error("raw-data decode row error: %d, %s", iRet, m_stRawData.get_err_msg());
			return -1;
		}
		stpTaskRow->default_value();
		stpTaskRow->Copy(stpNodeRow);
		iRet = stNewTmpRawData.insert_row(*stpTaskRow, m_stUpdateMode.m_uchInsertOrder ? true : false, false);
		if (0 != iRet)
		{
			log_error("insert row to raw-data error: ret = %d, err = %s", iRet, stNewTmpRawData.get_err_msg());
			return -2;
		}
	}

	// allocate new with new version
	RawData stTmpRawData(m_pMallocator);
	iRet = stTmpRawData.Init(stNewTmpRawData.Key(), stNewTmpRawData.data_size());
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(stTmpRawData.need_size(), *pstNode) == 0)
			iRet = stTmpRawData.Init(stNewTmpRawData.Key(), stNewTmpRawData.data_size() - stNewTmpRawData.data_start());
	}

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", stTmpRawData.get_err_msg());
		stTmpRawData.Destroy();
		return -3;
	}

	stTmpRawData.set_refrence(&stNewTmpRawData);
	iRet = stTmpRawData.copy_all();
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", stTmpRawData.get_err_msg());
		stTmpRawData.Destroy();
		return -3;
	}

	// purge old
	m_stRawData.Destroy();
	pstNode->vd_handle() = stTmpRawData.get_handle();
	return 0;
}

int RawDataProcess::destroy_data(Node *pstNode)
{
	int iRet;

	iRet = m_stRawData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		log_error("raw-data attach error: %d,%s", iRet, m_stRawData.get_err_msg());
		return (-1);
	}
	m_llRowsInc += 0LL - m_stRawData.total_rows();

	m_stRawData.Destroy();
	pstNode->vd_handle() = INVALID_HANDLE;

	return (0);
}

int RawDataProcess::replace_data(Node *pstNode, RawData *pstRawData)
{
	int iRet;

	log_debug("replace_data start ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	RawData tmpRawData(m_pMallocator);

	iRet = tmpRawData.Init(pstRawData->Key(), pstRawData->data_size() - pstRawData->data_start());
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(tmpRawData.need_size(), *pstNode) == 0)
			iRet = tmpRawData.Init(pstRawData->Key(), pstRawData->data_size() - pstRawData->data_start());
	}

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", tmpRawData.get_err_msg());
		tmpRawData.Destroy();
		return (-2);
	}

	tmpRawData.set_refrence(pstRawData);
	iRet = tmpRawData.copy_all();
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", tmpRawData.get_err_msg());
		tmpRawData.Destroy();
		return (-3);
	}

	if (pstNode->vd_handle() != INVALID_HANDLE)
		destroy_data(pstNode);
	pstNode->vd_handle() = tmpRawData.get_handle();
	m_llRowsInc += pstRawData->total_rows();
	if (tmpRawData.total_rows() > 0)
	{
		log_debug("replace_data,  stat history datasize, size is %u", tmpRawData.data_size());
		history_datasize.push(tmpRawData.data_size());
		history_rowsize.push(tmpRawData.total_rows());
	}
	return (0);
}

int RawDataProcess::get_expire_time(DTCTableDefinition *t, Node *pstNode, uint32_t &expire)
{
	int iRet = 0;

	iRet = attach_data(pstNode, NULL);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return iRet;
	}
	iRet = m_stRawData.get_expire_time(t, expire);
	if (iRet != 0)
	{
		log_error("raw data get expire time error: %d", iRet);
		return iRet;
	}
	return 0;
}

int RawDataProcess::dirty_rows_in_node(TaskRequest &stTask, Node *pstNode)
{
	int iRet = 0;
	int dirty_rows = 0;

	iRet = attach_data(pstNode, NULL);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return iRet;
	}

	unsigned char uchRowFlags;
	unsigned int uiTotalRows = m_stRawData.total_rows();

	DTCTableDefinition *t = m_stRawData.get_node_table_def();
	RowValue stRow(t);
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		iRet = m_stRawData.decode_row(stRow, uchRowFlags, 0);
		if (iRet != 0)
		{
			log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
			return (-4);
		}

		if (uchRowFlags & OPER_DIRTY)
			dirty_rows++;
	}

	return dirty_rows;
}

// pstAffectedRows is always NULL
int RawDataProcess::delete_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	log_debug("delete_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = attach_data(pstNode, pstAffectedRows);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return (iRet);
	}

	if (pstAffectedRows != NULL)
		pstAffectedRows->set_refrence(&m_stRawData);

	stpNodeTab = m_stRawData.get_node_table_def();
	stpTaskTab = stTask.table_definition();
	RowValue stNodeRow(stpNodeTab);
	RowValue stTaskRow(stpTaskTab);
	if (stpNodeTab == stpTaskTab)
	{
		stpNodeRow = &stTaskRow;
		stpTaskRow = &stTaskRow;
	}
	else
	{
		stpNodeRow = &stNodeRow;
		stpTaskRow = &stTaskRow;
	}

	int iAffectRows = 0;
	unsigned char uchRowFlags;
	unsigned int uiTotalRows = m_stRawData.total_rows();
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		iRet = m_stRawData.decode_row(*stpNodeRow, uchRowFlags, 0);
		if (iRet != 0)
		{
			log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
			return (-4);
		}
		if (stpNodeTab != stpTaskTab)
		{
			stpTaskRow->Copy(stpNodeRow);
		}
		if (stTask.compare_row(*stpTaskRow) != 0)
		{ //符合del条件
			if (pstAffectedRows != NULL)
			{ // copy row
				iRet = pstAffectedRows->copy_row();
				if (iRet != 0)
				{
					log_error("raw-data copy row error: %d,%s", iRet, pstAffectedRows->get_err_msg());
				}
			}
			iRet = m_stRawData.delete_cur_row(*stpNodeRow);
			if (iRet != EC_NO_MEM)
				pstNode->vd_handle() = m_stRawData.get_handle();
			if (iRet != 0)
			{
				log_error("raw-data delete row error: %d,%s", iRet, m_stRawData.get_err_msg());
				return (-5);
			}
			iAffectRows++;
			m_llRowsInc--;
			if (uchRowFlags & OPER_DIRTY)
				m_llDirtyRowsInc--;
		}
	}
	if (iAffectRows > 0)
	{
		if (stTask.resultInfo.affected_rows() == 0 ||
			(stTask.request_condition() && stTask.request_condition()->has_type_timestamp()))
		{
			stTask.resultInfo.set_affected_rows(iAffectRows);
		}
		m_stRawData.strip_mem();
	}

	if (m_stRawData.total_rows() > 0)
	{
		log_debug("stat history datasize, size is %u", m_stRawData.data_size());
		history_datasize.push(m_stRawData.data_size());
		history_rowsize.push(m_stRawData.total_rows());
		m_stRawData.update_last_access_time_by_hour();
		m_stRawData.update_last_update_time_by_hour();
	}
	return (0);
}

int RawDataProcess::get_data(TaskRequest &stTask, Node *pstNode)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	log_debug("get_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;
	int laid = stTask.flag_no_cache() ? -1 : stTask.table_definition()->lastacc_field_id();

	iRet = m_stRawData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		log_error("raw-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stRawData.get_err_msg());
		return (-1);
	}

	unsigned int uiTotalRows = m_stRawData.total_rows();
	stTask.prepare_result(); //准备返回结果对象
	if (stTask.all_rows() && (stTask.count_only() || !stTask.in_range((int)uiTotalRows, 0)))
	{
		if (stTask.is_batch_request())
		{
			if ((int)uiTotalRows > 0)
				stTask.add_total_rows((int)uiTotalRows);
		}
		else
		{
			stTask.set_total_rows((int)uiTotalRows);
		}
	}
	else
	{
		stpNodeTab = m_stRawData.get_node_table_def();
		stpTaskTab = stTask.table_definition();
		RowValue stNodeRow(stpNodeTab);
		RowValue stTaskRow(stpTaskTab);
		if (stpNodeTab == stpTaskTab)
		{
			stpNodeRow = &stTaskRow;
			stpTaskRow = &stTaskRow;
		}
		else
		{
			stpNodeRow = &stNodeRow;
			stpTaskRow = &stTaskRow;
		}
		unsigned char uchRowFlags;
		for (unsigned int i = 0; i < uiTotalRows; i++) //逐行拷贝数据
		{
			stTask.update_key(*stpNodeRow); // use stpNodeRow is fine, as just modify key field
			if ((iRet = m_stRawData.decode_row(*stpNodeRow, uchRowFlags, 0)) != 0)
			{
				log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
				return (-2);
			}
			// this pointer compare is ok, as these two is both come from tabledefmanager. if they mean same, they are same object.
			if (stpNodeTab != stpTaskTab)
			{
				stpTaskRow->Copy(stpNodeRow);
			}
			if (stTask.compare_row(*stpTaskRow) == 0) //如果不符合查询条件
				continue;

			if (stpTaskTab->expire_time_field_id() > 0)
				stpTaskRow->update_expire_time();
			//当前行添加到task中
			if (stTask.append_row(stpTaskRow) > 0 && laid > 0)
			{
				m_stRawData.update_lastacc(stTask.Timestamp());
			}
			if (stTask.all_rows() && stTask.result_full())
			{
				stTask.set_total_rows((int)uiTotalRows);
				break;
			}
		}
	}
	/*更新访问时间和查找操作计数*/
	m_stRawData.update_last_access_time_by_hour();
	m_stRawData.inc_select_count();
	log_debug("node[id:%u] ,Get Count is %d, last_access_time is %d, create_time is %d", pstNode->node_id(),
			  m_stRawData.get_select_op_count(), m_stRawData.get_last_access_time_by_hour(), m_stRawData.get_create_time_by_hour());
	return (0);
}

// pstAffectedRows is always NULL
int RawDataProcess::append_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool isDirty, bool setrows)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	iRet = attach_data(pstNode, pstAffectedRows);
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_warning("attach data error: %d", iRet);
		return (iRet);
	}

	stpNodeTab = m_stRawData.get_node_table_def();
	stpTaskTab = stTask.table_definition();
	RowValue stTaskRow(stpTaskTab);
	RowValue stNodeRow(stpNodeTab);
	stpTaskRow = &stTaskRow;
	stpTaskRow->default_value();
	stTask.update_row(*stpTaskRow);

	if (stpTaskTab->auto_increment_field_id() >= stpTaskTab->key_fields() && stTask.resultInfo.insert_id())
	{
		const int iFieldID = stpTaskTab->auto_increment_field_id();
		const uint64_t iVal = stTask.resultInfo.insert_id();
		stpTaskRow->field_value(iFieldID)->Set(iVal);
	}

	if (stpNodeTab == stpTaskTab)
	{
		stpNodeRow = stpTaskRow;
	}
	else
	{
		stpNodeRow = &stNodeRow;
		stpNodeRow->default_value();
		stpNodeRow->Copy(stpTaskRow);
	}

	log_debug("append_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	unsigned int uiTotalRows = m_stRawData.total_rows();
	if (uiTotalRows > 0)
	{
		if ((isDirty || setrows) && stTask.table_definition()->key_as_uniq_field())
		{
			snprintf(m_szErr, sizeof(m_szErr), "duplicate key error");
			return (-1062);
		}
		RowValue stOldRow(stpNodeTab); //一行数据
		if (setrows && stTask.table_definition()->key_part_of_uniq_field())
		{
			for (unsigned int i = 0; i < uiTotalRows; i++)
			{ //逐行拷贝数据
				unsigned char uchRowFlags;
				if (m_stRawData.decode_row(stOldRow, uchRowFlags, 0) != 0)
				{
					log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
					return (-1);
				}

				if (stpNodeRow->Compare(stOldRow, stpNodeTab->uniq_fields_list(),
										stpNodeTab->uniq_fields()) == 0)
				{
					snprintf(m_szErr, sizeof(m_szErr), "duplicate key error");
					return (-1062);
				}
			}
		}
	}

	if (pstAffectedRows != NULL && pstAffectedRows->insert_row(*stpNodeRow, false, isDirty) != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data insert row error: %s", pstAffectedRows->get_err_msg());
		return (-1);
	}

	// insert clean row
	iRet = m_stRawData.insert_row(*stpNodeRow, m_stUpdateMode.m_uchInsertOrder ? true : false, isDirty);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stRawData.need_size(), *pstNode) == 0)
			iRet = m_stRawData.insert_row(*stpNodeRow, m_stUpdateMode.m_uchInsertOrder ? true : false, isDirty);
	}
	if (iRet != EC_NO_MEM)
		pstNode->vd_handle() = m_stRawData.get_handle();
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data insert row error: %s", m_stRawData.get_err_msg());
		/*标记加入黑名单*/
		stTask.push_black_list_size(m_stRawData.need_size());
		return (-2);
	}

	if (stTask.resultInfo.affected_rows() == 0 || setrows == true)
		stTask.resultInfo.set_affected_rows(1);
	m_llRowsInc++;
	if (isDirty)
		m_llDirtyRowsInc++;
	log_debug("stat history datasize, size is %u", m_stRawData.data_size());
	history_datasize.push(m_stRawData.data_size());
	history_rowsize.push(m_stRawData.total_rows());
	m_stRawData.update_last_access_time_by_hour();
	m_stRawData.update_last_update_time_by_hour();
	log_debug("node[id:%u] ，Get Count is %d, create_time is %d, last_access_time is %d, last_update_time is %d ", pstNode->node_id(),
			  m_stRawData.get_select_op_count(), m_stRawData.get_create_time_by_hour(),
			  m_stRawData.get_last_access_time_by_hour(), m_stRawData.get_last_update_time_by_hour());
	return (0);
}

int RawDataProcess::replace_data(TaskRequest &stTask, Node *pstNode)
{
	log_debug("replace_data start! ");
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow;

	int iRet;
	int try_purge_count = 0;
	uint64_t all_rows_size = 0;
	int laid = stTask.flag_no_cache() || stTask.count_only() ? -1 : stTask.table_definition()->lastacc_field_id();
	int matchedCount = 0;
	int limitStart = 0;
	int limitStop = 0x10000000;

	stpTaskTab = stTask.table_definition();
	if (DTCColExpand::Instance()->is_expanding())
		stpNodeTab = TableDefinitionManager::Instance()->get_new_table_def();
	else
		stpNodeTab = TableDefinitionManager::Instance()->get_cur_table_def();
	RowValue stNodeRow(stpNodeTab);
	stpNodeRow = &stNodeRow;
	stpNodeRow->default_value();

	if (laid > 0 && stTask.requestInfo.limit_count() > 0)
	{
		limitStart = stTask.requestInfo.limit_start();
		if (stTask.requestInfo.limit_start() > 0x10000000)
		{
			laid = -1;
		}
		else if (stTask.requestInfo.limit_count() < 0x10000000)
		{
			limitStop = limitStart + stTask.requestInfo.limit_count();
		}
	}

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	if (pstNode->vd_handle() != INVALID_HANDLE)
	{
		iRet = destroy_data(pstNode);
		if (iRet != 0)
			return (-1);
	}

	iRet = m_stRawData.Init(stTask.packed_key(), 0);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stRawData.need_size(), *pstNode) == 0)
			iRet = m_stRawData.Init(m_pstTab->key_fields() - 1, m_pstTab->key_format(), stTask.packed_key(), 0);
	}
	if (iRet != EC_NO_MEM)
		pstNode->vd_handle() = m_stRawData.get_handle();

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", m_stRawData.get_err_msg());
		/*标记加入黑名单*/
		stTask.push_black_list_size(m_stRawData.need_size());
		m_pstPool->purge_node(stTask.packed_key(), *pstNode);
		return (-2);
	}

	if (stTask.result != NULL)
	{
		ResultSet *pstResultSet = stTask.result;
		for (int i = 0; i < pstResultSet->total_rows(); i++)
		{
			RowValue *pstRow = pstResultSet->_fetch_row();
			if (pstRow == NULL)
			{
				log_debug("%s!", "call fetch_row func error");
				m_pstPool->purge_node(stTask.packed_key(), *pstNode);
				m_stRawData.Destroy();
				return (-3);
			}

			if (laid > 0 && stTask.compare_row(*pstRow))
			{
				if (matchedCount >= limitStart && matchedCount < limitStop)
				{
					(*pstRow)[laid].s64 = stTask.Timestamp();
				}
				matchedCount++;
			}

			if (stpTaskTab != stpNodeTab)
			{
				stpNodeRow->Copy(pstRow);
			}
			else
			{
				stpNodeRow = pstRow;
			}

			/* 插入当前行 */
			iRet = m_stRawData.insert_row(*stpNodeRow, false, false);

			/* 如果内存空间不足，尝试扩大最多两次 */
			if (iRet == EC_NO_MEM)
			{

				/* 预测整个Node的数据大小 */
				all_rows_size = m_stRawData.need_size() - m_stRawData.data_start();
				all_rows_size *= pstResultSet->total_rows();
				all_rows_size /= (i + 1);
				all_rows_size += m_stRawData.data_start();

				if (try_purge_count >= 2)
				{
					goto ERROR_PROCESS;
				}

				/* 尝试次数 */
				++try_purge_count;
				if (m_pstPool->try_purge_size((size_t)all_rows_size, *pstNode) == 0)
					iRet = m_stRawData.insert_row(*stpNodeRow, false, false);
			}
			if (iRet != EC_NO_MEM)
				pstNode->vd_handle() = m_stRawData.get_handle();

			/* 当前行操作成功 */
			if (0 == iRet)
				continue;
		ERROR_PROCESS:
			snprintf(m_szErr, sizeof(m_szErr), "raw-data insert row error: ret=%d,err=%s, cnt=%d",
					 iRet, m_stRawData.get_err_msg(), try_purge_count);
			/*标记加入黑名单*/
			stTask.push_black_list_size(all_rows_size);
			m_pstPool->purge_node(stTask.packed_key(), *pstNode);
			m_stRawData.Destroy();
			return (-4);
		}

		m_llRowsInc += pstResultSet->total_rows();
	}

	m_stRawData.update_last_access_time_by_hour();
	m_stRawData.update_last_update_time_by_hour();
	log_debug("node[id:%u], handle[" UINT64FMT "] ,data-size[%u],  Get Count is %d, create_time is %d, last_access_time is %d, Update time is %d",
			  pstNode->node_id(),
			  pstNode->vd_handle(),
			  m_stRawData.data_size(),
			  m_stRawData.get_select_op_count(),
			  m_stRawData.get_create_time_by_hour(),
			  m_stRawData.get_last_access_time_by_hour(),
			  m_stRawData.get_last_update_time_by_hour());

	history_datasize.push(m_stRawData.data_size());
	history_rowsize.push(m_stRawData.total_rows());
	return (0);
}

// The correct replace behavior:
// 	If conflict rows found, delete them all
// 	Insert new row
// 	Affected rows is total deleted and inserted rows
// Implementation hehavior:
// 	If first conflict row found, update it, and increase affected rows to 2 (1 delete + 1 insert)
// 	delete other fonflict row, increase affected 1 per row
// 	If no rows found, insert it and set affected rows to 1
int RawDataProcess::replace_rows(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	log_debug("replace_rows start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	if (pstNode->vd_handle() == INVALID_HANDLE)
	{
		iRet = init_data(pstNode, pstAffectedRows, stTask.packed_key());
		if (iRet != 0)
		{
			log_error("init data error: %d", iRet);
			if (pstNode->vd_handle() == INVALID_HANDLE)
				m_pstPool->purge_node(stTask.packed_key(), *pstNode);
			return (iRet);
		}
	}
	else
	{
		iRet = attach_data(pstNode, pstAffectedRows);
		if (iRet != 0)
		{
			log_error("attach data error: %d", iRet);
			return (iRet);
		}
	}

	unsigned char uchRowFlags;
	uint64_t ullAffectedrows = 0;
	unsigned int uiTotalRows = m_stRawData.total_rows();
	if (pstAffectedRows != NULL)
		pstAffectedRows->set_refrence(&m_stRawData);

	stpNodeTab = m_stRawData.get_node_table_def();
	stpTaskTab = stTask.table_definition();
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	stNewRow.default_value();
	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	stTask.update_row(*stpTaskRow); //获取Replace的行
	if (stpNodeTab != stpTaskTab)
		stpNodeRow->Copy(stpTaskRow);
	else
		stpNodeRow = stpTaskRow;

	RowValue stRow(stpNodeTab); //一行数据
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{ //逐行拷贝数据
		if (m_stRawData.decode_row(stRow, uchRowFlags, 0) != 0)
		{
			log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
			return (-1);
		}

		if (stTask.table_definition()->key_as_uniq_field() == false &&
			stNewRow.Compare(stRow, stTask.table_definition()->uniq_fields_list(),
							 stTask.table_definition()->uniq_fields()) != 0)
			continue;

		if (ullAffectedrows == 0)
		{
			if (pstAffectedRows != NULL && pstAffectedRows->insert_row(*stpNodeRow, false, async) != 0)
			{
				log_error("raw-data copy row error: %d,%s", iRet, pstAffectedRows->get_err_msg());
				return (-2);
			}

			ullAffectedrows = 2;
			iRet = m_stRawData.replace_cur_row(*stpNodeRow, async); // 加进cache
		}
		else
		{
			ullAffectedrows++;
			iRet = m_stRawData.delete_cur_row(*stpNodeRow); // 加进cache
		}
		if (iRet == EC_NO_MEM)
		{
			if (m_pstPool->try_purge_size(m_stRawData.need_size(), *pstNode) == 0)
				iRet = m_stRawData.replace_cur_row(*stpNodeRow, async);
		}
		if (iRet != EC_NO_MEM)
			pstNode->vd_handle() = m_stRawData.get_handle();
		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "raw-data replace row error: %d, %s",
					 iRet, m_stRawData.get_err_msg());
			/*标记加入黑名单*/
			stTask.push_black_list_size(m_stRawData.need_size());
			return (-3);
		}
		if (uchRowFlags & OPER_DIRTY)
			m_llDirtyRowsInc--;
		if (async)
			m_llDirtyRowsInc++;
	}

	if (ullAffectedrows == 0)
	{															 // 找不到匹配的行，insert一行
		iRet = m_stRawData.insert_row(*stpNodeRow, false, async); // 加进cache
		if (iRet == EC_NO_MEM)
		{
			if (m_pstPool->try_purge_size(m_stRawData.need_size(), *pstNode) == 0)
				iRet = m_stRawData.insert_row(*stpNodeRow, false, async);
		}
		if (iRet != EC_NO_MEM)
			pstNode->vd_handle() = m_stRawData.get_handle();

		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "raw-data replace row error: %d, %s",
					 iRet, m_stRawData.get_err_msg());
			/*标记加入黑名单*/
			stTask.push_black_list_size(m_stRawData.need_size());
			return (-3);
		}
		m_llRowsInc++;
		ullAffectedrows++;
		if (async)
			m_llDirtyRowsInc++;
	}

	if (async == true || setrows == true)
	{
		stTask.resultInfo.set_affected_rows(ullAffectedrows);
	}
	else if (ullAffectedrows != stTask.resultInfo.affected_rows())
	{
		//如果cache更新纪录数和helper更新的纪录数不相等
		log_debug("unequal affected rows, cache[%lld], helper[%lld]",
				  (long long)ullAffectedrows,
				  (long long)stTask.resultInfo.affected_rows());
	}

	log_debug("stat history datasize, size is %u", m_stRawData.data_size());
	history_datasize.push(m_stRawData.data_size());
	history_rowsize.push(m_stRawData.total_rows());
	m_stRawData.update_last_access_time_by_hour();
	m_stRawData.update_last_update_time_by_hour();
	log_debug("node[id:%u], create_time is %d, last_access_time is %d, Update Time is %d ",
			  pstNode->node_id(), m_stRawData.get_create_time_by_hour(), m_stRawData.get_last_access_time_by_hour(), m_stRawData.get_last_update_time_by_hour());
	return (0);
}

/*
 * encode到私有内存，防止replace，update引起重新rellocate导致value引用了过期指针
 */
int RawDataProcess::encode_to_private_area(RawData &raw, RowValue &value, unsigned char value_flag)
{
	int ret = raw.Init(m_stRawData.Key(), raw.calc_row_size(value, m_pstTab->key_fields() - 1));
	if (0 != ret)
	{
		log_error("init raw-data struct error, ret=%d, err=%s", ret, raw.get_err_msg());
		return -1;
	}

	ret = raw.insert_row(value, false, false);
	if (0 != ret)
	{
		log_error("insert row to raw-data error: ret=%d, err=%s", ret, raw.get_err_msg());
		return -2;
	}

	raw.rewind();

	ret = raw.decode_row(value, value_flag, 0);
	if (0 != ret)
	{
		log_error("decode raw-data to row error: ret=%d, err=%s", ret, raw.get_err_msg());
		return -3;
	}

	return 0;
}

int RawDataProcess::update_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	log_debug("update_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = attach_data(pstNode, pstAffectedRows);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return (iRet);
	}

	unsigned char uchRowFlags;
	uint64_t ullAffectedrows = 0;
	unsigned int uiTotalRows = m_stRawData.total_rows();
	if (pstAffectedRows != NULL)
		pstAffectedRows->set_refrence(&m_stRawData);

	RowValue stRow(stTask.table_definition()); //一行数据

	stpNodeTab = m_stRawData.get_node_table_def();
	stpTaskTab = stTask.table_definition();
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	if (stpNodeTab == stpTaskTab)
		stpNodeRow = stpTaskRow;

	for (unsigned int i = 0; i < uiTotalRows; i++)
	{ //逐行拷贝数据
		if (m_stRawData.decode_row(*stpNodeRow, uchRowFlags, 0) != 0)
		{
			log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
			return (-1);
		}

		if (stpNodeTab != stpTaskTab)
			stpTaskRow->Copy(stpNodeRow);

		//如果不符合查询条件
		if (stTask.compare_row(*stpTaskRow) == 0)
			continue;

		stTask.update_row(*stpTaskRow); //修改数据
		ullAffectedrows++;

		if (stpNodeTab != stpTaskTab)
			stpNodeRow->Copy(stpTaskRow);

		if (pstAffectedRows != NULL && pstAffectedRows->insert_row(*stpNodeRow, false, async) != 0)
		{
			log_error("raw-data copy row error: %d,%s", iRet, pstAffectedRows->get_err_msg());
			return (-2);
		}

		// 在私有区间decode
		RawData stTmpRows(&g_stSysMalloc, 1);
		if (encode_to_private_area(stTmpRows, *stpNodeRow, uchRowFlags))
		{
			log_error("encode rowvalue to private rawdata area failed");
			return -3;
		}

		iRet = m_stRawData.replace_cur_row(*stpNodeRow, async); // 加进cache
		if (iRet == EC_NO_MEM)
		{
			if (m_pstPool->try_purge_size(m_stRawData.need_size(), *pstNode) == 0)
				iRet = m_stRawData.replace_cur_row(*stpNodeRow, async);
		}
		if (iRet != EC_NO_MEM)
			pstNode->vd_handle() = m_stRawData.get_handle();
		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "raw-data replace row error: %d, %s",
					 iRet, m_stRawData.get_err_msg());
			/*标记加入黑名单*/
			stTask.push_black_list_size(m_stRawData.need_size());
			return (-6);
		}

		if (uchRowFlags & OPER_DIRTY)
			m_llDirtyRowsInc--;
		if (async)
			m_llDirtyRowsInc++;
	}

	if (async == true || setrows == true)
	{
		stTask.resultInfo.set_affected_rows(ullAffectedrows);
	}
	else if (ullAffectedrows != stTask.resultInfo.affected_rows())
	{
		//如果cache更新纪录数和helper更新的纪录数不相等
		log_debug("unequal affected rows, cache[%lld], helper[%lld]",
				  (long long)ullAffectedrows,
				  (long long)stTask.resultInfo.affected_rows());
	}
	log_debug("stat history datasize, size is %u", m_stRawData.data_size());
	history_datasize.push(m_stRawData.data_size());
	history_rowsize.push(m_stRawData.total_rows());
	m_stRawData.update_last_access_time_by_hour();
	m_stRawData.update_last_update_time_by_hour();
	log_debug("node[id:%u], create_time is %d, last_access_time is %d, UpdateTime is %d",
			  pstNode->node_id(), m_stRawData.get_create_time_by_hour(), m_stRawData.get_last_access_time_by_hour(), m_stRawData.get_last_update_time_by_hour());
	return (0);
}

int RawDataProcess::flush_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt)
{
	int iRet;

	log_debug("flush_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = attach_data(pstNode, NULL);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return (iRet);
	}

	unsigned char uchRowFlags;
	unsigned int uiTotalRows = m_stRawData.total_rows();

	uiFlushRowsCnt = 0;
	DTCValue astKey[m_pstTab->key_fields()];
	TaskPackedKey::unpack_key(m_pstTab, m_stRawData.Key(), astKey);
	RowValue stRow(m_pstTab); //一行数据
	for (int i = 0; i < m_pstTab->key_fields(); i++)
		stRow[i] = astKey[i];

	for (unsigned int i = 0; pstNode->is_dirty() && i < uiTotalRows; i++)
	{ //逐行拷贝数据
		if (m_stRawData.decode_row(stRow, uchRowFlags, 0) != 0)
		{
			log_error("raw-data decode row error: %d,%s", iRet, m_stRawData.get_err_msg());
			return (-1);
		}

		if ((uchRowFlags & OPER_DIRTY) == false)
			continue;

		if (pstFlushReq && pstFlushReq->flush_row(stRow) != 0)
		{
			log_error("flush_data() invoke flushRow() failed.");
			return (-2);
		}
		m_stRawData.set_cur_row_flag(uchRowFlags & ~OPER_DIRTY);
		m_llDirtyRowsInc--;
		uiFlushRowsCnt++;
	}

	return (0);
}

int RawDataProcess::purge_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt)
{
	int iRet;

	log_debug("purge_data start! ");

	iRet = flush_data(pstFlushReq, pstNode, uiFlushRowsCnt);
	if (iRet != 0)
	{
		return (iRet);
	}
	m_llRowsInc = 0LL - m_stRawData.total_rows();

	return (0);
}
