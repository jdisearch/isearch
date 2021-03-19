/*
 * =====================================================================================
 *
 *       Filename:  tree_data_process.cc
 *
 *    Description:  tree data process interface.
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

#include "tree_data_process.h"
#include "global.h"
#include "log.h"
#include "sys_malloc.h"

DTC_USING_NAMESPACE

TreeDataProcess::TreeDataProcess(Mallocator *pstMalloc, DTCTableDefinition *pstTab, DTCBufferPool *pstPool, const UpdateMode *pstUpdateMode) : m_stTreeData(pstMalloc), m_pstTab(pstTab), m_pMallocator(pstMalloc), m_pstPool(pstPool)
{
	memcpy(&m_stUpdateMode, pstUpdateMode, sizeof(m_stUpdateMode));
	nodeSizeLimit = 0;
	history_rowsize = statmgr.get_sample(ROW_SIZE_HISTORY_STAT);
}

TreeDataProcess::~TreeDataProcess()
{
}

int TreeDataProcess::get_expire_time(DTCTableDefinition *t, Node *pstNode, uint32_t &expire)
{
	int iRet = 0;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (iRet);
	}

	iRet = m_stTreeData.get_expire_time(t, expire);
	if (iRet != 0)
	{
		log_error("tree data get expire time error: %d", iRet);
		return iRet;
	}
	return 0;
}

int TreeDataProcess::replace_data(Node *pstNode, RawData *pstRawData)
{
	int iRet;

	log_debug("Replace TreeData start ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	TreeData tmpTreeData(m_pMallocator);

	iRet = tmpTreeData.Init(pstRawData->Key());
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(tmpTreeData.need_size(), *pstNode) == 0)
			iRet = tmpTreeData.Init(pstRawData->Key());
	}

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "root-data init error: %s", tmpTreeData.get_err_msg());
		tmpTreeData.Destroy();
		return (-2);
	}

	iRet = tmpTreeData.copy_tree_all(pstRawData);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(tmpTreeData.need_size(), *pstNode) == 0)
			iRet = tmpTreeData.copy_tree_all(pstRawData);
	}

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "root-data init error: %s", tmpTreeData.get_err_msg());
		tmpTreeData.Destroy();
		return (-2);
	}

	if (pstNode->vd_handle() != INVALID_HANDLE)
		destroy_data(pstNode);
	pstNode->vd_handle() = tmpTreeData.get_handle();

	if (tmpTreeData.total_rows() > 0)
	{
		history_rowsize.push(tmpTreeData.total_rows());
	}
	return (0);
}

int TreeDataProcess::append_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool isDirty, bool setrows)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (iRet);
	}

	stpNodeTab = m_stTreeData.get_node_table_def();
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

	log_debug("AppendTreeData start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	unsigned int uiTotalRows = m_stTreeData.total_rows();
	if (uiTotalRows > 0)
	{
		if ((isDirty || setrows) && stTask.table_definition()->key_as_uniq_field())
		{
			snprintf(m_szErr, sizeof(m_szErr), "duplicate key error");
			return (-1062);
		}
		if (setrows && stTask.table_definition()->key_part_of_uniq_field())
		{
			iRet = m_stTreeData.compare_tree_data(stpNodeRow);
			if (iRet < 0)
			{
				log_error("tree-data decode row error: %d,%s", iRet, m_stTreeData.get_err_msg());
				return iRet;
			}
			else if (iRet == 0)
			{
				snprintf(m_szErr, sizeof(m_szErr), "duplicate key error");
				return (-1062);
			}
		}
	}

	// insert clean row
	iRet = m_stTreeData.insert_row(*stpNodeRow, KeyCompare, isDirty);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
			iRet = m_stTreeData.insert_row(*stpNodeRow, KeyCompare, isDirty);
	}
	if (iRet != EC_NO_MEM)
		pstNode->vd_handle() = m_stTreeData.get_handle();
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "tree-data insert row error: %s,%d", m_stTreeData.get_err_msg(), iRet);
		/*标记加入黑名单*/
		stTask.push_black_list_size(m_stTreeData.need_size());
		return (-2);
	}

	if (stTask.resultInfo.affected_rows() == 0 || setrows == true)
		stTask.resultInfo.set_affected_rows(1);
	m_llRowsInc++;
	if (isDirty)
		m_llDirtyRowsInc++;
	history_rowsize.push(m_stTreeData.total_rows());
	return (0);
}

int TreeDataProcess::get_data(TaskRequest &stTask, Node *pstNode)
{
	int iRet;
	log_debug("Get TreeData start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (-1);
	}

	iRet = m_stTreeData.get_tree_data(stTask);
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "get tree data error");
		log_error("tree-data get[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return iRet;
	}

	/*更新访问时间和查找操作计数*/
	log_debug("node[id:%u] ,Get Count is %d", pstNode->node_id(), m_stTreeData.total_rows());
	return (0);
}

int TreeDataProcess::expand_node(TaskRequest &stTask, Node *pstNode)
{
	return 0;
}

int TreeDataProcess::dirty_rows_in_node(TaskRequest &stTask, Node *pstNode)
{
	int iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (-1);
	}

	return m_stTreeData.dirty_rows_in_node();
}

int TreeDataProcess::attach_data(Node *pstNode, RawData *pstAffectedRows)
{
	int iRet;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (-1);
	}

	if (pstAffectedRows != NULL)
	{
		iRet = pstAffectedRows->Init(m_stTreeData.Key(), 0);
		if (iRet != 0)
		{
			log_error("tree-data init error: %d,%s", iRet, pstAffectedRows->get_err_msg());
			return (-2);
		}
	}

	return (0);
}

int TreeDataProcess::get_all_rows(Node *pstNode, RawData *pstRows)
{
	int iRet = 0;

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = attach_data(pstNode, pstRows);
	if (iRet != 0)
	{
		log_error("attach data error: %d", iRet);
		return (-1);
	}

	iRet = m_stTreeData.copy_raw_all(pstRows);
	if (iRet != 0)
	{
		log_error("copy data error: %d,%s", iRet, m_stTreeData.get_err_msg());
		return (-2);
	}

	return (0);
}

int TreeDataProcess::delete_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows)
{
	int iRet;
	log_debug("Delete TreeData start! ");

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (-1);
	}

	int start = m_stTreeData.total_rows();

	iRet = m_stTreeData.delete_tree_data(stTask);
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "get tree data error");
		log_error("tree-data get[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return iRet;
	}

	int iAffectRows = start - m_stTreeData.total_rows();
	if (iAffectRows > 0)
	{
		if (stTask.resultInfo.affected_rows() == 0 ||
			(stTask.request_condition() && stTask.request_condition()->has_type_timestamp()))
		{
			stTask.resultInfo.set_affected_rows(iAffectRows);
		}
	}

	m_llRowsInc = m_stTreeData.rows_inc();
	m_llDirtyRowsInc = m_stTreeData.dirty_rows_inc();

	log_debug("node[id:%u] ,Get Count is %d", pstNode->node_id(), m_stTreeData.total_rows());
	return (0);
}

int TreeDataProcess::replace_data(TaskRequest &stTask, Node *pstNode)
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

	iRet = m_stTreeData.Init(stTask.packed_key());
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
			iRet = m_stTreeData.Init(m_pstTab->key_fields() - 1, m_pstTab->key_format(), stTask.packed_key());
	}
	if (iRet != EC_NO_MEM)
		pstNode->vd_handle() = m_stTreeData.get_handle();

	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data init error: %s", m_stTreeData.get_err_msg());
		/*标记加入黑名单*/
		stTask.push_black_list_size(m_stTreeData.need_size());
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
				m_stTreeData.Destroy();
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
			iRet = m_stTreeData.insert_row(*stpNodeRow, KeyCompare, false);

			/* 如果内存空间不足，尝试扩大最多两次 */
			if (iRet == EC_NO_MEM)
			{

				if (try_purge_count >= 2)
				{
					goto ERROR_PROCESS;
				}

				/* 尝试次数 */
				++try_purge_count;
				if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
					iRet = m_stTreeData.insert_row(*stpNodeRow, KeyCompare, false);
			}
			if (iRet != EC_NO_MEM)
				pstNode->vd_handle() = m_stTreeData.get_handle();

			/* 当前行操作成功 */
			if (0 == iRet)
				continue;
		ERROR_PROCESS:
			snprintf(m_szErr, sizeof(m_szErr), "raw-data insert row error: ret=%d,err=%s, cnt=%d",
					 iRet, m_stTreeData.get_err_msg(), try_purge_count);
			/*标记加入黑名单*/
			stTask.push_black_list_size(all_rows_size);
			m_pstPool->purge_node(stTask.packed_key(), *pstNode);
			m_stTreeData.Destroy();
			return (-4);
		}

		m_llRowsInc += pstResultSet->total_rows();
	}

	history_rowsize.push(m_stTreeData.total_rows());

	return (0);
}

int TreeDataProcess::replace_rows(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false)
{
	int iRet;
	log_debug("Replace TreeData start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	if (pstNode)
	{
		iRet = m_stTreeData.Attach(pstNode->vd_handle());
		if (iRet != 0)
		{
			log_error("attach tree data error: %d", iRet);
			return (iRet);
		}
	}
	else
	{
		iRet = m_stTreeData.Init(stTask.packed_key());
		if (iRet == EC_NO_MEM)
		{
			if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
				iRet = m_stTreeData.Init(stTask.packed_key());
		}

		if (iRet != 0)
		{
			log_error("tree-data replace[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
			return iRet;
		}

		pstNode->vd_handle() = m_stTreeData.get_handle();
	}

	unsigned char uchRowFlags;
	iRet = m_stTreeData.replace_tree_data(stTask, pstNode, pstAffectedRows, async, uchRowFlags, setrows);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
			iRet = m_stTreeData.replace_tree_data(stTask, pstNode, pstAffectedRows, async, uchRowFlags, setrows);
	}

	if (iRet != 0)
	{
		log_error("tree-data replace[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return iRet;
	}

	if (uchRowFlags & OPER_DIRTY)
		m_llDirtyRowsInc--;
	if (async)
		m_llDirtyRowsInc++;

	uint64_t ullAffectedRows = m_stTreeData.get_affectedrows();
	if (ullAffectedRows == 0) //insert
	{
		DTCTableDefinition *stpTaskTab;
		RowValue *stpNewRow;
		stpTaskTab = stTask.table_definition();
		RowValue stNewRow(stpTaskTab);
		stNewRow.default_value();
		stpNewRow = &stNewRow;
		stTask.update_row(*stpNewRow);								  //获取Replace的行
		iRet = m_stTreeData.insert_row(*stpNewRow, KeyCompare, async); // 加进cache
		if (iRet == EC_NO_MEM)
		{
			if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
				iRet = m_stTreeData.insert_row(*stpNewRow, KeyCompare, async);
		}
		if (iRet != EC_NO_MEM)
			pstNode->vd_handle() = m_stTreeData.get_handle();

		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "raw-data replace row error: %d, %s",
					 iRet, m_stTreeData.get_err_msg());
			/*标记加入黑名单*/
			stTask.push_black_list_size(m_stTreeData.need_size());
			return (-3);
		}
		m_llRowsInc++;
		ullAffectedRows++;
		if (async)
			m_llDirtyRowsInc++;
	}
	if (async == true || setrows == true)
	{
		stTask.resultInfo.set_affected_rows(ullAffectedRows);
	}
	else if (ullAffectedRows != stTask.resultInfo.affected_rows())
	{
		//如果cache更新纪录数和helper更新的纪录数不相等
		log_debug("unequal affected rows, cache[%lld], helper[%lld]",
				  (long long)ullAffectedRows,
				  (long long)stTask.resultInfo.affected_rows());
	}

	return 0;
}

int TreeDataProcess::update_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false)
{
	int iRet;
	log_debug("Update TreeData start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		log_error("attach tree data error: %d", iRet);
		return (iRet);
	}

	m_stTreeData.set_affected_rows(0);

	iRet = m_stTreeData.update_tree_data(stTask, pstNode, pstAffectedRows, async, setrows);
	if (iRet == EC_NO_MEM)
	{
		if (m_pstPool->try_purge_size(m_stTreeData.need_size(), *pstNode) == 0)
			iRet = m_stTreeData.update_tree_data(stTask, pstNode, pstAffectedRows, async, setrows);
	}

	if (iRet != 0)
	{
		log_error("tree-data update[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return iRet;
	}

	uint64_t ullAffectedRows = m_stTreeData.get_affectedrows();
	m_llDirtyRowsInc = m_stTreeData.dirty_rows_inc();

	if (async == true || setrows == true)
	{
		stTask.resultInfo.set_affected_rows(ullAffectedRows);
	}
	else if (ullAffectedRows != stTask.resultInfo.affected_rows())
	{
		//如果cache更新纪录数和helper更新的纪录数不相等
		log_debug("unequal affected rows, cache[%lld], helper[%lld]",
				  (long long)ullAffectedRows,
				  (long long)stTask.resultInfo.affected_rows());
	}

	return (0);
}

int TreeDataProcess::flush_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt)
{
	int iRet;

	log_debug("flush_data start! ");

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	iRet = m_stTreeData.Attach(pstNode->vd_handle());
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach data error");
		log_error("tree-data attach[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return (-1);
	}

	iRet = m_stTreeData.flush_tree_data(pstFlushReq, pstNode, uiFlushRowsCnt);
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "flush tree data error");
		log_error("tree-data flush[handle:" UINT64FMT "] error: %d,%s", pstNode->vd_handle(), iRet, m_stTreeData.get_err_msg());
		return iRet;
	}

	m_llDirtyRowsInc = m_stTreeData.dirty_rows_inc();

	return (0);
}

int TreeDataProcess::purge_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt)
{
	int iRet;

	log_debug("purge_data start! ");

	iRet = flush_data(pstFlushReq, pstNode, uiFlushRowsCnt);
	if (iRet != 0)
	{
		return (iRet);
	}
	m_llRowsInc = 0LL - m_stTreeData.total_rows();

	return 0;
}

int TreeDataProcess::destroy_data(Node *pstNode)
{
	if (pstNode->vd_handle() == INVALID_HANDLE)
		return 0;
	TreeData treeData(m_pMallocator);
	treeData.Attach(pstNode->vd_handle());
	treeData.Destroy();
	pstNode->vd_handle() = INVALID_HANDLE;
	return 0;
}