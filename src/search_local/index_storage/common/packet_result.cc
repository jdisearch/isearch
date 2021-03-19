/*
 * =====================================================================================
 *
 *       Filename:  packet_result.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "result.h"
#include "decode.h"

ResultWriter::ResultWriter(const DTCFieldSet *fs, unsigned int st, unsigned int cnt)
{
	if (cnt == 0)
		st = 0;
	limitStart = st;
	limitNext = st + cnt;
	const int nf = fs == NULL ? 0 : fs->num_fields();
	fieldSet = nf ? fs : NULL;
	totalRows = 0;
	numRows = 0;
}

int ResultWriter::set_rows(unsigned int rows)
{
	if (fieldSet == NULL /* count_only */ ||
		(numRows == 0 && rows <= limitStart /* Not in Range */))
	{
		totalRows = rows;
		numRows = limitNext == 0 ? rows : rows <= limitStart ? 0 : rows <= limitNext ? rows - limitStart : limitNext - limitStart;
	}
	else if (is_full() == 0)
		return -1;
	else
		totalRows = rows;

	return 0;
}

ResultPacket::ResultPacket(const DTCFieldSet *fs, unsigned int st, unsigned int cnt) : ResultWriter(fs, st, cnt)
{
	const int nf = fs == NULL ? 0 : fs->num_fields();
	int len = 5 + 1 + nf;
	bc = (BufferChain *)MALLOC(1024);
	if (bc == NULL)
		throw(int) - ENOMEM;
	bc->nextBuffer = NULL;
	bc->usedBytes = len;
	bc->totalBytes = 1024 - sizeof(BufferChain);
	rowDataBegin = bc->usedBytes;
	char *p = bc->data + 5;
	*p++ = nf;
	if (nf)
	{
		for (int i = 0; i < nf; i++)
			*p++ = fs->field_id(i);
	}
}

void ResultPacket::Clean()
{
	ResultWriter::Clean();
	if (bc)
		bc->Clean();
}

/* pool, bc should exist */
int ResultPacket::Set(const DTCFieldSet *fs, unsigned int st, unsigned int ct)
{
	ResultWriter::Set(fs, st, ct);
	const int nf = fs == NULL ? 0 : fs->num_fields();
	int len = 5 + 1 + nf;
	if (bc == NULL)
		return -1;
	bc->nextBuffer = NULL;
	bc->usedBytes = len;
	rowDataBegin = bc->usedBytes;
	char *p = bc->data + 5;
	*p++ = nf;
	if (nf)
	{
		for (int i = 0; i < nf; i++)
			*p++ = fs->field_id(i);
	}

	return 0;
}

/* resultPacket is just a buff */
ResultPacket::~ResultPacket(void)
{
	FREE_IF(bc);
}

static int expand(BufferChain *&bc, int addsize)
{
	int expectsize = bc->usedBytes + addsize;
	if (bc->totalBytes >= expectsize)
		return 0;

	// add header and round to 8 byte aligned
	expectsize += sizeof(BufferChain);
	expectsize |= 7;
	expectsize += 1;

	int sparsesize = expectsize;
	if (sparsesize > addsize * 16)
		sparsesize = addsize * 16;

	sparsesize += expectsize;
	if (REALLOC(bc, sparsesize) != NULL)
	{
		bc->totalBytes = sparsesize - sizeof(BufferChain);
		return 0;
	}
	if (REALLOC(bc, expectsize) != NULL)
	{
		bc->totalBytes = expectsize - sizeof(BufferChain);
		return 0;
	}
	return -1;
}

int ResultPacket::append_row(const RowValue &r)
{
	int ret = 0;
	totalRows++;
	if (limitNext > 0)
	{
		if (totalRows <= limitStart || totalRows > limitNext)
			return 0;
	}
	if (fieldSet)
	{
		int len = 0;
		for (int i = 0; i < fieldSet->num_fields(); i++)
		{
			const int id = fieldSet->field_id(i);
			len += encoded_bytes_data_value(r.field_value(id), r.field_type(id));
		}

#if 0
	    BufferChain *c = (BufferChain *)REALLOC(bc,
		    sizeof(BufferChain)+ bc->usedBytes + len);

	    if(c==NULL) return -ENOMEM;
	    bc = c;
#else
		if (expand(bc, len) != 0)
			return -ENOMEM;
#endif

		char *p = bc->data + bc->usedBytes;
		bc->usedBytes += len;

		for (int i = 0; i < fieldSet->num_fields(); i++)
		{
			const int id = fieldSet->field_id(i);
			p = encode_data_value(p, r.field_value(id), r.field_type(id));
		}
		ret = 1;
	}
	numRows = totalRows - limitStart;
	return ret;
}

int ResultPacket::merge_no_limit(const ResultWriter *rp0)
{
	const ResultPacket &rp = *(const ResultPacket *)rp0;

	int expectedsize = bc->usedBytes + (rp.bc->usedBytes - rp.rowDataBegin);
	if (bc->totalBytes < expectedsize)
	{
		BufferChain *c = (BufferChain *)REALLOC(bc, sizeof(BufferChain) + expectedsize + 1024);
		if (c == NULL)
			return -ENOMEM;
		bc = c;
		bc->totalBytes = expectedsize + 1024;
	}

	char *p = bc->data + bc->usedBytes;
	memcpy(p, rp.bc->data + rp.rowDataBegin, rp.bc->usedBytes - rp.rowDataBegin);

	bc->usedBytes += rp.bc->usedBytes - rp.rowDataBegin;
	totalRows += rp.totalRows;
	numRows += rp.numRows;

	return 0;
}
