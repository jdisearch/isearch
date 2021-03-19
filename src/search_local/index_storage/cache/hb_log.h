/*
 * =====================================================================================
 *
 *       Filename:  hb_log.h
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
#ifndef __DTC_HB_LOG_H
#define __DTC_HB_LOG_H

#include "logger.h"
#include "journal_id.h"
#include "task_request.h"
#include "field.h"
#include "raw_data.h"
#include "admin_tdef.h"
#include "sys_malloc.h"
#include "table_def.h"

class BinlogWriter;
class BinlogReader;

class HBLog
{
public:
    //传入编解码的表结构
    HBLog(DTCTableDefinition *tbl);
    ~HBLog();

    int Init(const char *path, const char *prefix, uint64_t total, off_t max_size);
    int Seek(const JournalID &);

    JournalID get_reader_jid(void);
    JournalID get_writer_jid(void);

    //不带value，只写更新key
    int write_update_key(DTCValue key, int type);

    //将多条log记录编码进TaskReqeust
    int task_append_all_rows(TaskRequest &, int limit);

    //提供给LRUBitUnit来记录lru变更
    int write_lru_hb_log(TaskRequest &task);
    int write_update_log(TaskRequest &task);
    int write_update_key(DTCValue key, DTCValue v, int type);

private:
    DTCTableDefinition *_tabledef;
    BinlogWriter *_log_writer;
    BinlogReader *_log_reader;
};

#endif
