/*
 * =====================================================================================
 *
 *       Filename:  formater.cc
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
#include <stdarg.h>

#include "formater.h"

TableFormater::TableFormater()
{
}

TableFormater::~TableFormater()
{
}

void TableFormater::new_row()
{
	table.resize(table.size() + 1);
}

void TableFormater::clear_row()
{
	table.resize(table.size() - 1);
}

void TableFormater::Cell(const char *format, ...)
{
	cell_t cell;
	if (format && format[0])
	{
		va_list ap;
		cell.off = buf.size();
		va_start(ap, format);
		cell.len = buf.vbprintf(format, ap);
		va_end(ap);
	}

	unsigned int c = table.back().size();
	if (c >= width.size())
		width.push_back(cell.len);
	if (width[c] < cell.len)
		width[c] = cell.len;

	table.back().push_back(cell);
}
void TableFormater::CellV(const char *format, ...)
{
	cell_t cell;
	if (format && format[0])
	{
		va_list ap;
		cell.off = buf.size();
		va_start(ap, format);
		cell.len = buf.vbprintf(format, ap);
		va_end(ap);
	}

	unsigned int c = table.back().size();
	if (c >= width.size())
		width.push_back(0);

	table.back().push_back(cell);
}

void TableFormater::Dump(FILE *fp, int fmt)
{
	unsigned i, j;
	char del = '\t';

	switch (fmt)
	{
	case FORMAT_ALIGNED:
		for (i = 0; i < table.size(); i++)
		{
			row_t &r = table[i];
			for (j = 0; j < r.size(); j++)
			{
				if (width[j] == 0)
					continue;
				cell_t &c = r[j];

				if (j != 0)
					fputc(' ', fp);
				if (c.len < width[j])
					fprintf(fp, "%*s", width[j] - c.len, "");
				fprintf(fp, "%.*s", (int)(c.len), buf.c_str() + c.off);
			}
			fputc('\n', fp);
		}
		break;

	case FORMAT_TABBED:
		del = '\t';
		goto out;
	case FORMAT_COMMA:
		del = ',';
		goto out;
	out:
		for (i = 0; i < table.size(); i++)
		{
			row_t &r = table[i];
			for (j = 0; j < r.size(); j++)
			{
				cell_t &c = r[j];
				if (j != 0)
					fputc(del, fp);
				fprintf(fp, "%.*s", (int)(c.len), buf.c_str() + c.off);
			}
			for (; j < width.size(); j++)
				if (j != 0)
					fputc(del, fp);
			fputc('\n', fp);
		}
		break;
	}
}
