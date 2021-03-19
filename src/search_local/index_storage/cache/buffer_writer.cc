/*
 * =====================================================================================
 *
 *       Filename:  buffer_writer.cc
 *
 *    Description:  
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

#include "task_pkey.h"
#include "buffer_writer.h"
#include "pt_malloc.h"
#include "sys_malloc.h"
#include "log.h"

BufferWriter::BufferWriter(void) : DTCBufferPool(NULL)
{
	pstItem = NULL;
	iRowIdx = 0;
	iIsFull = 0;
	memset(achPackKey, 0, sizeof(achPackKey));
}

BufferWriter::~BufferWriter(void)
{
	if (pstItem != NULL)
		delete pstItem;
	pstItem = NULL;
}

int BufferWriter::cache_open(CacheInfo *pstInfo, DTCTableDefinition *pstTab)
{
	int iRet;

	iRet = DTCBufferPool::cache_open(pstInfo);
	if (iRet != E_OK)
	{
		log_error("cache open error: %d, %s", iRet, Error());
		return -1;
	}

	pstItem = new RawData(&g_stSysMalloc, 1);
	if (pstItem == NULL)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "new RawData error: %m");
		return -2;
	}

	UpdateMode stUpdateMod;
	stUpdateMod.m_iAsyncServer = pstInfo->syncUpdate ? MODE_SYNC : MODE_ASYNC;
	stUpdateMod.m_iUpdateMode = pstInfo->syncUpdate ? MODE_SYNC : MODE_ASYNC;
	stUpdateMod.m_iInsertMode = pstInfo->syncUpdate ? MODE_SYNC : MODE_ASYNC;
	stUpdateMod.m_uchInsertOrder = 0;

	if (pstTab->index_fields() > 0)
	{
#if HAS_TREE_DATA
		pstDataProcess = new TreeDataProcess(DTCBinMalloc::Instance(), pstTab, this, &stUpdateMod);
#else
		log_error("tree index not supported, index field num[%d]", pstTab->index_fields());
		return -1;
#endif
	}
	else
		pstDataProcess = new RawDataProcess(DTCBinMalloc::Instance(), pstTab, this, &stUpdateMod);
	if (pstDataProcess == NULL)
	{
		log_error("create %s error: %m", pstTab->index_fields() > 0 ? "TreeDataProcess" : "RawDataProcess");
		return -3;
	}

	return 0;
}

int BufferWriter::begin_write()
{
	iRowIdx = 0;

	return 0;
}

int BufferWriter::full()
{
	return (iIsFull);
}

int BufferWriter::AllocNode(const RowValue &row)
{
	int iRet;

	iRet = TaskPackedKey::build_packed_key(row.table_definition(), row.field_value(0), sizeof(achPackKey), achPackKey);
	if (iRet != 0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "build packed key error: %d", iRet);
		return -1;
	}

	stCurNode = cache_allocate(achPackKey);
	if (!stCurNode)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "cache alloc node error");
		iIsFull = 1;
		return -2;
	}

	iRet = pstItem->Init(row.table_definition()->key_fields() - 1, row.table_definition()->key_format(), achPackKey, 0);
	if (iRet != 0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "raw data init error: %s", pstItem->get_err_msg());
		cache_purge(achPackKey);
		return -3;
	}

	return 0;
}

int BufferWriter::write_row(const RowValue &row)
{
	int iRet;

	if (iRowIdx == 0)
	{
		if (AllocNode(row) != 0)
			return -1;
	}

	iRet = pstItem->insert_row(row, false, false);
	if (iRet != 0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "insert row error: %s", pstItem->get_err_msg());
		cache_purge(achPackKey);
		return -2;
	}

	iRowIdx++;
	return 0;
}

int BufferWriter::commit_node()
{
	int iRet;

	if (iRowIdx < 1)
		return 0;

	const MemHead *pstHead = DTCBinMalloc::Instance()->get_head_info();
	if (pstHead->m_hTop + pstItem->data_size() + MINSIZE >= pstHead->m_tSize)
	{
		iIsFull = 1;
		cache_purge(achPackKey);
		return -1;
	}

	iRet = pstDataProcess->replace_data(&stCurNode, pstItem);
	if (iRet != 0)
	{
		snprintf(szErrMsg, sizeof(szErrMsg), "write data into cache error");
		cache_purge(achPackKey);
		return -2;
	}

	iRowIdx = 0;
	memset(achPackKey, 0, sizeof(achPackKey));
	pstItem->Destroy();
	return 0;
}

int BufferWriter::rollback_node()
{
	pstItem->Destroy();
	cache_purge(achPackKey);
	memset(achPackKey, 0, sizeof(achPackKey));

	return 0;
}
