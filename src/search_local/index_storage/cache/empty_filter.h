/*
 * =====================================================================================
 *
 *       Filename:  empty_filter.h
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
#ifndef __DTC_EMPTY_FILTER_H
#define __DTC_EMPTY_FILTER_H

#include "namespace.h"
#include "singleton.h"
#include "global.h"

DTC_BEGIN_NAMESPACE

#define DF_ENF_TOTAL 0	/* 0 = unlimited */
#define DF_ENF_STEP 512 /* byte */
#define DF_ENF_MOD 30000

struct _enf_table
{
	MEM_HANDLE_T t_handle;
	uint32_t t_size;
};
typedef struct _enf_table ENF_TABLE_T;

struct _empty_node_filter
{
	uint32_t enf_total; // 占用的总内存
	uint32_t enf_step;	// 表增长步长
	uint32_t enf_mod;	// 分表算子

	ENF_TABLE_T enf_tables[0]; // 位图表
};
typedef struct _empty_node_filter ENF_T;

class EmptyNodeFilter
{
public:
	void SET(uint32_t key);
	void CLR(uint32_t key);
	int ISSET(uint32_t key);

public:
	/* 0 = use default value */
	int Init(uint32_t total = 0, uint32_t step = 0, uint32_t mod = 0);
	int Attach(MEM_HANDLE_T);
	int Detach(void);

public:
	EmptyNodeFilter();
	~EmptyNodeFilter();
	static EmptyNodeFilter *Instance() { return Singleton<EmptyNodeFilter>::Instance(); }
	static void Destroy() { Singleton<EmptyNodeFilter>::Destroy(); }
	const char *Error() const { return _errmsg; }
	const MEM_HANDLE_T Handle() const { return M_HANDLE(_enf); }

private:
	/* 计算表id */
	uint32_t Index(uint32_t key) { return key % _enf->enf_mod; }
	/* 计算表中的位图偏移 */
	uint32_t Offset(uint32_t key) { return key / _enf->enf_mod; }

private:
	ENF_T *_enf;
	char _errmsg[256];
};

DTC_END_NAMESPACE

#endif
