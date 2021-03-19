/*
 * =====================================================================================
 *
 *       Filename:  col_expand.h
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
#ifndef __DTC_COL_EXPAND_H_
#define __DTC_COL_EXPAND_H_

#include "namespace.h"
#include "global.h"
#include "singleton.h"

DTC_BEGIN_NAMESPACE

#define COL_EXPAND_BUFF_SIZE (1024 * 1024)
#define COL_EXPAND_BUFF_NUM 2

struct _col_expand
{
	bool expanding;
	unsigned char curTable;
	char tableBuff[COL_EXPAND_BUFF_NUM][COL_EXPAND_BUFF_SIZE];
};
typedef struct _col_expand COL_EXPAND_T;

class DTCColExpand
{
public:
	DTCColExpand();
	~DTCColExpand();

	static DTCColExpand *Instance() { return Singleton<DTCColExpand>::Instance(); }
	static void Destroy() { Singleton<DTCColExpand>::Destroy(); }

	int Init();
	int Attach(MEM_HANDLE_T handle, int forceFlag);

	bool is_expanding();
	bool expand(const char *table, int len);
	int try_expand(const char *table, int len);
	bool expand_done();
	int cur_table_idx();
	int reload_table();

	const MEM_HANDLE_T Handle() const { return M_HANDLE(_colExpand); }
	const char *Error() const { return _errmsg; }

private:
	COL_EXPAND_T *_colExpand;
	char _errmsg[256];
};

DTC_END_NAMESPACE

#endif
