/*
 * =====================================================================================
 *
 *       Filename:  raw_data_process.h
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

#ifndef RAW_DATA_PROCESS_H
#define RAW_DATA_PROCESS_H

#include "buffer_def.h"
#include "protocol.h"
#include "value.h"
#include "field.h"
#include "section.h"
#include "table_def.h"
#include "task_request.h"
#include "stat_dtc.h"
#include "raw_data.h"
#include "node.h"
#include "data_process.h"
#include "buffer_pool.h"
#include "namespace.h"
#include "stat_manager.h"

DTC_BEGIN_NAMESPACE

class TaskRequest;
class DTCFlushRequest;

class RawDataProcess
	: public DataProcess
{
private:
	RawData m_stRawData;
	DTCTableDefinition *m_pstTab;
	Mallocator *m_pMallocator;
	DTCBufferPool *m_pstPool;
	UpdateMode m_stUpdateMode;
	int64_t m_llRowsInc;
	int64_t m_llDirtyRowsInc;
	char m_szErr[200];

	unsigned int nodeSizeLimit; // -DEBUG-

	/*对历史节点数据的采样统计，放在高端内存操作管理的地方，便于收敛统计点 , modify by tomchen 2014.08.27*/
	StatSample history_datasize;
	StatSample history_rowsize;

protected:
	int init_data(Node *pstNode, RawData *pstAffectedRows, const char *ptrKey);
	int attach_data(Node *pstNode, RawData *pstAffectedRows);
	int destroy_data(Node *pstNode);

private:
	int encode_to_private_area(RawData &, RowValue &, unsigned char);

public:
	RawDataProcess(Mallocator *pstMalloc, DTCTableDefinition *pstTab, DTCBufferPool *pstPool, const UpdateMode *pstUpdateMode);

	~RawDataProcess();

	void set_limit_node_size(int node_size) { nodeSizeLimit = node_size; } // -DEBUG-

	const char *get_err_msg() { return m_szErr; }
	void set_insert_mode(EUpdateMode iMode) { m_stUpdateMode.m_iInsertMode = iMode; }
	void set_insert_order(int iOrder) { m_stUpdateMode.m_uchInsertOrder = iOrder; }

	void change_mallocator(Mallocator *pstMalloc)
	{
		log_debug("oring mallc: %p, new mallc: %p", m_pMallocator, pstMalloc);
		m_pMallocator = pstMalloc;
		m_stRawData.change_mallocator(pstMalloc);
	}

	/* expire time for nodb mode */
	int get_expire_time(DTCTableDefinition *t, Node *node, uint32_t &expire);

	/*count dirty row, cache process will use it when buffer_delete_rows in task->all_rows case*/
	int dirty_rows_in_node(TaskRequest &stTask, Node *node);

	/*************************************************
	  Description:	查询本次操作增加的行数（可以为负数）
	  Input:		
	  Output:		
	  Return:		行数
	*************************************************/
	int64_t rows_inc() { return m_llRowsInc; }

	/*************************************************
	  Description:	查询本次操作增加的脏行数（可以为负数）
	  Input:		
	  Output:		
	  Return:		行数
	*************************************************/
	int64_t dirty_rows_inc() { return m_llDirtyRowsInc; }

	/*************************************************
	  Description:	查询node里的所有数据
	  Input:		pstNode	node节点
	  Output:		pstRows	保存数据的结构
	  Return:		0为成功，非0失败
	*************************************************/
	int get_all_rows(Node *pstNode, RawData *pstRows);

	/*************************************************
	  Description:	扩展node的列
	  Input:		pstNode	node节点
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int expand_node(TaskRequest &stTask, Node *pstNode);

	/*************************************************
	  Description:	用pstRows的数据替换cache里的数据
	  Input:		pstRows	新数据
				pstNode	node节点
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int replace_data(Node *pstNode, RawData *pstRawData);

	/*************************************************
	  Description:	根据task请求删除数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		pstAffectedRows	保存被删除的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	int delete_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows);

	/*************************************************
	  Description:	根据task请求查询数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		stTask	保存查找到的数据
	  Return:		0为成功，非0失败
	*************************************************/
	int get_data(TaskRequest &stTask, Node *pstNode);

	/*************************************************
	  Description:	根据task请求添加一行数据
	  Input:		stTask	task请求
				pstNode	node节点
				isDirty	是否脏数据
	  Output:		pstAffectedRows	保存被删除的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	int append_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool isDirty, bool uniq);

	/*************************************************
	  Description:	用task的数据替换cache里的数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int replace_data(TaskRequest &stTask, Node *pstNode);

	/*************************************************
	  Description:	用task的数据替换cache里的数据
	  Input:		stTask	task请求
				pstNode	node节点
				async	是否异步操作
	  Output:		pstAffectedRows	保存被更新后的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	int replace_rows(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false);

	/*************************************************
	  Description:	根据task请求更新cache数据
	  Input:		stTask	task请求
				pstNode	node节点
				async	是否异步操作
	  Output:		pstAffectedRows	保存被更新后的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	int update_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false);

	/*************************************************
	  Description:	将node节点的脏数据组成若干个flush请求
	  Input:		pstNode	node节点
	  Output:		pstFlushReq	保存flush请求
				uiFlushRowsCnt	被flush的行数
	  Return:		0为成功，非0失败
	*************************************************/
	int flush_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt);

	/*************************************************
	  Description:	删除cache里的数据，如果有脏数据会生成flush请求
	  Input:		pstNode	node节点
	  Output:		pstFlushReq	保存flush请求
				uiFlushRowsCnt	被flush的行数
	  Return:		0为成功，非0失败
	*************************************************/
	int purge_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt);
};

DTC_END_NAMESPACE

#endif
