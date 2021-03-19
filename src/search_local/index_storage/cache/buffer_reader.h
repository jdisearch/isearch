/*
 * =====================================================================================
 *
 *       Filename:  buffer_reader.h
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
#ifndef __CACHE_READER_H
#define __CACHE_READER_H

#include "reader_interface.h"
#include "buffer_pool.h"
#include "table_def.h"
#include "raw_data_process.h"

class BufferReader : public ReaderInterface, public DTCBufferPool
{
private:
	Node stClrHead;
	Node stDirtyHead;
	int iInDirtyLRU;
	Node stCurNode;
	unsigned char uchRowFlags;
	RawData *pstItem;
	DataProcess *pstDataProcess;
	int notFetch;
	int curRowIdx;
	char error_message[200];

public:
	BufferReader(void);
	~BufferReader(void);

	int cache_open(int shmKey, int keySize, DTCTableDefinition *pstTab);

	const char *err_msg() { return error_message; }
	int begin_read();
	int read_row(RowValue &row);
	int end();
	int key_flags(void) const { return stCurNode.is_dirty(); }
	int key_flag_dirty(void) const { return stCurNode.is_dirty(); }
	int row_flags(void) const { return uchRowFlags; }
	int row_flag_dirty(void) const { return uchRowFlags & OPER_DIRTY; }
	int fetch_node();
	int num_rows();
};

inline int BufferReader::end()
{
	return (iInDirtyLRU == 0) && (notFetch == 0) && (stCurNode == stClrHead);
}

#endif
