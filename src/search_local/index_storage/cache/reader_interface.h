/*
 * =====================================================================================
 *
 *       Filename:  reader_interface.h
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
#ifndef __READER_INTERFACE_H
#define __READER_INTERFACE_H

#include "field.h"

class ReaderInterface
{
public:
	ReaderInterface() {}
	virtual ~ReaderInterface() {}

	virtual const char *err_msg() = 0;
	virtual int begin_read() { return 0; }
	virtual int read_row(RowValue &row) = 0;
	virtual int end() = 0;
};

#endif
