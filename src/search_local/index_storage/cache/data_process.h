/*
 * =====================================================================================
 *
 *       Filename:  data_process.h
 *
 *    Description:  data processing interface(abstract class) definition.
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

#ifndef DATA_PROCESS_H
#define DATA_PROCESS_H

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

#include "namespace.h"
DTC_BEGIN_NAMESPACE

enum EUpdateMode
{
	MODE_SYNC = 0,
	MODE_ASYNC,
	MODE_FLUSH
};

typedef struct
{
	EUpdateMode m_iAsyncServer;
	EUpdateMode m_iUpdateMode;
	EUpdateMode m_iInsertMode;
	unsigned char m_uchInsertOrder;
} UpdateMode;

class DTCFlushRequest;
class DataProcess
{
public:
	DataProcess() {}
	virtual ~DataProcess() {}

	virtual const char *get_err_msg() = 0;
	virtual void set_insert_mode(EUpdateMode iMode) = 0;
	virtual void set_insert_order(int iOrder) = 0;

	/*************************************************
	  Description:	查询本次操作增加的行数（可以为负数）
	  Input:		
	  Output:		
	  Return:		行数
	*************************************************/
	virtual int64_t rows_inc() = 0;

	/*************************************************
	  Description:	查询本次操作增加的脏行数（可以为负数）
	  Input:		
	  Output:		
	  Return:		行数
	*************************************************/
	virtual int64_t dirty_rows_inc() = 0;

	/*************************************************
	  Description:	查询node里的所有数据
	  Input:		pstNode	node节点
	  Output:		pstRows	保存数据的结构
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int get_all_rows(Node *pstNode, RawData *pstRows) = 0;

	/*************************************************
	  Description:	用pstRows的数据替换cache里的数据
	  Input:		pstRows	新数据
				pstNode	node节点
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int replace_data(Node *pstNode, RawData *pstRawData) = 0;

	/*************************************************
	  Description:	根据task请求删除数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		pstAffectedRows	保存被删除的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int delete_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows) = 0;

	/*************************************************
	  Description:	根据task请求查询数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		stTask	保存查找到的数据
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int get_data(TaskRequest &stTask, Node *pstNode) = 0;

	/*************************************************
	  Description:	根据task请求添加一行数据
	  Input:		stTask	task请求
				pstNode	node节点
				isDirty	是否脏数据
	  Output:		pstAffectedRows	保存被删除的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int append_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool isDirty, bool uniq) = 0;

	/*************************************************
	  Description:	用task的数据替换cache里的数据
	  Input:		stTask	task请求
				pstNode	node节点
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int replace_data(TaskRequest &stTask, Node *pstNode) = 0;

	/*************************************************
	  Description:	用task的数据替换cache里的数据
	  Input:		stTask	task请求
				pstNode	node节点
				async	是否异步操作
	  Output:		pstAffectedRows	保存被更新后的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int replace_rows(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false) = 0;

	/*************************************************
	  Description:	根据task请求更新cache数据
	  Input:		stTask	task请求
				pstNode	node节点
				async	是否异步操作
	  Output:		pstAffectedRows	保存被更新后的数据（为NULL时不保存）
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int update_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows = false) = 0;

	/*************************************************
	  Description:	将node节点的脏数据组成若干个flush请求
	  Input:		pstNode	node节点
	  Output:		pstFlushReq	保存flush请求
				uiFlushRowsCnt	被flush的行数
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int flush_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt) = 0;

	/*************************************************
	  Description:	删除cache里的数据，如果有脏数据会生成flush请求
	  Input:		pstNode	node节点
	  Output:		pstFlushReq	保存flush请求
				uiFlushRowsCnt	被flush的行数
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int purge_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt) = 0;

	/*************************************************
    Description: 	get cache expire time
    Input:			pstNode node
    Output:
    Return:   
    *************************************************/
	virtual int get_expire_time(DTCTableDefinition *t, Node *pstNode, uint32_t &expire) = 0;

	/*************************************************
    Description: 	
    Input:			
    Output:
    Return:   
    *************************************************/
	virtual int expand_node(TaskRequest &stTask, Node *pstNode) = 0;

	/*************************************************
    Description: 	
    Input:			
    Output:
    Return:   
    *************************************************/
	virtual void change_mallocator(Mallocator *pstMalloc) = 0;

	/*************************************************
    Description: 	
    Input:			
    Output:
    Return:   
    *************************************************/
	virtual int dirty_rows_in_node(TaskRequest &stTask, Node *node) = 0;
};

DTC_END_NAMESPACE

#endif
