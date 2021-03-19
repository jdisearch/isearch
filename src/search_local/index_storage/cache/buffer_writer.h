/*
 * =====================================================================================
 *
 *       Filename:  buffer_writer.h
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
#ifndef __CACHE_WRITER_H
#define __CACHE_WRITER_H

#include "buffer_pool.h"
#include "table_def.h"
#include "writer_interface.h"
#include "raw_data_process.h"

class BufferWriter : public WriterInterface, public DTCBufferPool
{
private:
	RawData *pstItem;
	DataProcess *pstDataProcess;
	int iIsFull;
	int iRowIdx;
	Node stCurNode;
	char achPackKey[MAX_KEY_LEN + 1];
	char szErrMsg[200];

protected:
	int AllocNode(const RowValue &row);

public:
	BufferWriter(void);
	~BufferWriter(void);

	int cache_open(CacheInfo *pstInfo, DTCTableDefinition *pstTab);

	const char *err_msg() { return szErrMsg; }
	int begin_write();
	int full();
	int write_row(const RowValue &row);
	int commit_node();
	int rollback_node();
};

#endif
