/*
 * =====================================================================================
 *
 *       Filename:  formater.h
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
#include <vector>

#include "buffer.h"

class TableFormater
{
public:
	enum
	{
		FORMAT_ALIGNED,
		FORMAT_TABBED,
		FORMAT_COMMA,
	};
	TableFormater();
	~TableFormater();
	void new_row(void);
	void clear_row(void);
	void Cell(const char *fmt, ...) __attribute__((__format__(printf, 2, 3)));
	void CellV(const char *fmt, ...) __attribute__((__format__(printf, 2, 3)));
	void Dump(FILE *, int fmt = FORMAT_ALIGNED);

private:
	struct cell_t
	{
		unsigned int off;
		unsigned int len;
		cell_t() : off(0), len(0) {}
		~cell_t() {}
	};
	typedef std::vector<cell_t> row_t;

	class buffer buf;
	std::vector<row_t> table;
	std::vector<unsigned int> width;
};
