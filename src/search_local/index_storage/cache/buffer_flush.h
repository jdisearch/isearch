/*
 * =====================================================================================
 *
 *       Filename:  buffer_flush.h
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
#ifndef __H_CACHE_FLUSH_H__
#define __H_CACHE_FLUSH_H__

#include "timer_list.h"
#include "lqueue.h"
#include "task_request.h"
#include "buffer_process.h"
#include "log.h"

class BufferProcess;

class DTCFlushRequest
{
private:
    BufferProcess *owner;
    int numReq;
    int badReq;
    TaskRequest *wait;

public:
    friend class BufferProcess;
    DTCFlushRequest(BufferProcess *, const char *key);
    ~DTCFlushRequest(void);

    const DTCTableDefinition *table_definition(void) const { return owner->table_definition(); }

    int flush_row(const RowValue &);
    void complete_row(TaskRequest *req, int index);
    int Count(void) const { return numReq; }
};

#endif
