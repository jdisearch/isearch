/*
 * =====================================================================================
 *
 *       Filename:  buffer_reader.cc
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
#include "buffer_reader.h"
#include "log.h"
#include "sys_malloc.h"

BufferReader::BufferReader(void) : DTCBufferPool(NULL)
{
	pstItem = NULL;
	pstDataProcess = NULL;
	iInDirtyLRU = 1;
	notFetch = 1;
}

BufferReader::~BufferReader(void)
{
	if (pstItem != NULL)
		delete pstItem;
	pstItem = NULL;
}

int BufferReader::cache_open(int shmKey, int keySize, DTCTableDefinition *pstTab)
{
	int iRet;

	CacheInfo stInfo;
	memset(&stInfo, 0, sizeof(stInfo));
	stInfo.ipcMemKey = shmKey;
	stInfo.keySize = keySize;
	stInfo.readOnly = 1;

	iRet = DTCBufferPool::cache_open(&stInfo);
	if (iRet != E_OK)
		return -1;

	pstItem = new RawData(&g_stSysMalloc, 1);
	if (pstItem == NULL)
	{
		snprintf(error_message, sizeof(error_message), "new RawData error: %m");
		return -1;
	}

	UpdateMode stUpdateMod;
	stUpdateMod.m_iAsyncServer = MODE_SYNC;
	stUpdateMod.m_iUpdateMode = MODE_SYNC;
	stUpdateMod.m_iInsertMode = MODE_SYNC;
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
		return -1;
	}

	return 0;
}

int BufferReader::begin_read()
{
	stDirtyHead = dirty_lru_head();
	stClrHead = clean_lru_head();
	if (!dirty_lru_empty())
	{
		iInDirtyLRU = 1;
		stCurNode = stDirtyHead;
	}
	else
	{
		iInDirtyLRU = 0;
		stCurNode = stClrHead;
	}
	return 0;
}

int BufferReader::fetch_node()
{

	pstItem->Destroy();
	if (!stCurNode)
	{
		snprintf(error_message, sizeof(error_message), "begin read first!");
		return -1;
	}
	if (end())
	{
		snprintf(error_message, sizeof(error_message), "reach end of cache");
		return -2;
	}
	notFetch = 0;

	curRowIdx = 0;
	if (iInDirtyLRU)
	{
		while (stCurNode != stDirtyHead && is_time_marker(stCurNode))
			stCurNode = stCurNode.Next();
		if (stCurNode != stDirtyHead && !is_time_marker(stCurNode))
		{
			if (pstDataProcess->get_all_rows(&stCurNode, pstItem) != 0)
			{
				snprintf(error_message, sizeof(error_message), "get node's data error");
				return -3;
			}
			return (0);
		}

		iInDirtyLRU = 0;
		stCurNode = stClrHead.Next();
	}

	stCurNode = stCurNode.Next();
	if (stCurNode != stClrHead)
	{
		if (pstDataProcess->get_all_rows(&stCurNode, pstItem) != 0)
		{
			snprintf(error_message, sizeof(error_message), "get node's data error");
			return -3;
		}
	}
	else
	{
		snprintf(error_message, sizeof(error_message), "reach end of cache");
		return -2;
	}

	return (0);
}

int BufferReader::num_rows()
{
	if (pstItem == NULL)
		return (-1);

	return pstItem->total_rows();
}

int BufferReader::read_row(RowValue &row)
{
	while (notFetch || curRowIdx >= (int)pstItem->total_rows())
	{
		if (fetch_node() != 0)
			return -1;
	}

	TaskPackedKey::unpack_key(row.table_definition(), pstItem->Key(), row.field_value(0));

	if (pstItem->decode_row(row, uchRowFlags, 0) != 0)
		return -2;

	curRowIdx++;

	return 0;
}
