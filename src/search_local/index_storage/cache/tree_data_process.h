/*
 * =====================================================================================
 *
 *       Filename:  tree_data_process.h
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

#ifndef TREE_DATA_PROCESS_H
#define TREE_DATA_PROCESS_H

#include "buffer_def.h"
#include "protocol.h"
#include "value.h"
#include "field.h"
#include "section.h"
#include "table_def.h"
#include "task_request.h"
#include "stat_dtc.h"
#include "tree_data.h"
#include "node.h"
#include "data_process.h"
#include "buffer_pool.h"
#include "namespace.h"
#include "stat_manager.h"
#include "data_chunk.h"

DTC_BEGIN_NAMESPACE

class TaskRequest;
class DTCFlushRequest;

class TreeDataProcess
    : public DataProcess
{
private:
  TreeData m_stTreeData;
  DTCTableDefinition *m_pstTab;
  Mallocator *m_pMallocator;
  DTCBufferPool *m_pstPool;
  UpdateMode m_stUpdateMode;
  int64_t m_llRowsInc;
  int64_t m_llDirtyRowsInc;
  char m_szErr[200];

  unsigned int nodeSizeLimit; // -DEBUG-

  StatSample history_datasize;
  StatSample history_rowsize;

protected:
  int attach_data(Node *pstNode, RawData *pstAffectedRows);

public:
  void change_mallocator(Mallocator *pstMalloc)
  {
    log_debug("oring mallc: %p, new mallc: %p", m_pMallocator, pstMalloc);
    m_pMallocator = pstMalloc;
    m_stTreeData.change_mallocator(pstMalloc);
  }

  TreeDataProcess(Mallocator *pstMalloc, DTCTableDefinition *pstTab, DTCBufferPool *pstPool, const UpdateMode *pstUpdateMode);
  ~TreeDataProcess();

  const char *get_err_msg() { return m_szErr; }
  void set_insert_mode(EUpdateMode iMode) {}
  void set_insert_order(int iOrder) {}

  /*************************************************
    Description: get expire time
    Output:   
    *************************************************/
  int get_expire_time(DTCTableDefinition *t, Node *pstNode, uint32_t &expire);

  /*************************************************
    Description: 
    Output:   
    *************************************************/
  int expand_node(TaskRequest &stTask, Node *pstNode);

  /*************************************************
    Description: 
    Output:   
    *************************************************/
  int dirty_rows_in_node(TaskRequest &stTask, Node *pstNode);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int64_t rows_inc() { return m_llRowsInc; };

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int64_t dirty_rows_inc() { return m_llDirtyRowsInc; }

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int get_all_rows(Node *pstNode, RawData *pstRows);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int delete_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int replace_data(TaskRequest &stTask, Node *pstNode);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int replace_rows(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int update_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int flush_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt);

  /*************************************************
    Description: 
    Output: 
    *************************************************/
  int purge_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt);

  /*************************************************
    Description: append data in t-tree
    Output:   
    *************************************************/
  int append_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool isDirty, bool setrows);

  /*************************************************
    Description: replace data in t-tree
    Output:   
    *************************************************/
  int replace_data(Node *pstNode, RawData *pstRawData);

  /*************************************************
    Description: get data in t-tree
    Output:   
    *************************************************/
  int get_data(TaskRequest &stTask, Node *pstNode);

  /*************************************************
    Description: destroy t-tree
    Output:   
    *************************************************/
  int destroy_data(Node *pstNode);
};

DTC_END_NAMESPACE

#endif