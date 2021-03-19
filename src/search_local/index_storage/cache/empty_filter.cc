/*
 * =====================================================================================
 *
 *       Filename:  empty_filter.cc
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
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "pt_malloc.h"
#include "empty_filter.h"
#include "bitsop.h"

EmptyNodeFilter::EmptyNodeFilter() : _enf(0)
{
	memset(_errmsg, 0x0, sizeof(_errmsg));
}

EmptyNodeFilter::~EmptyNodeFilter()
{
}

int EmptyNodeFilter::ISSET(uint32_t key)
{
	uint32_t bitoff = Offset(key);
	uint32_t tableid = Index(key);

	if (_enf->enf_tables[tableid].t_size < bitoff / CHAR_BIT + 1)
		return 0;

	return ISSET_B(bitoff, M_POINTER(void, _enf->enf_tables[tableid].t_handle));
}

void EmptyNodeFilter::SET(uint32_t key)
{
	uint32_t bitoff = Offset(key);
	uint32_t tableid = Index(key);

	if (_enf->enf_tables[tableid].t_size < bitoff / CHAR_BIT + 1)
	{
		/* 按step的整数倍来increase table*/
		int incbyte = bitoff / CHAR_BIT + 1 - _enf->enf_tables[tableid].t_size;
		int how = (incbyte + _enf->enf_step - 1) / _enf->enf_step;
		size_t size = _enf->enf_tables[tableid].t_size + how * _enf->enf_step;

		_enf->enf_tables[tableid].t_handle = M_REALLOC(_enf->enf_tables[tableid].t_handle, size);
		if (_enf->enf_tables[tableid].t_handle == INVALID_HANDLE)
		{
			/* realloc 失败后，不会重试*/
			return;
		}

		_enf->enf_tables[tableid].t_size = size;
	}

	return SET_B(bitoff, M_POINTER(void, _enf->enf_tables[tableid].t_handle));
}

void EmptyNodeFilter::CLR(uint32_t key)
{
	uint32_t bitoff = Offset(key);
	uint32_t tableid = Index(key);

	if (_enf->enf_tables[tableid].t_size < bitoff / CHAR_BIT + 1)
		/* 超出表范围，return*/
		return;

	return CLR_B(bitoff, M_POINTER(void, _enf->enf_tables[tableid].t_handle));
}

int EmptyNodeFilter::Init(uint32_t total, uint32_t step, uint32_t mod)
{
	mod = mod ? mod : DF_ENF_MOD;
	step = step ? step : DF_ENF_STEP;
	total = total ? total : DF_ENF_TOTAL;

	/* allocate header */
	uint32_t size = sizeof(ENF_T);
	size += sizeof(ENF_TABLE_T) * mod;

	MEM_HANDLE_T v = M_CALLOC(size);
	if (INVALID_HANDLE == v)
	{
		snprintf(_errmsg, sizeof(_errmsg),
				 "calloc %u bytes mem failed, %s", size, M_ERROR());
		return -1;
	}

	_enf = M_POINTER(ENF_T, v);

	_enf->enf_total = total;
	_enf->enf_step = step;
	_enf->enf_mod = mod;

	return 0;
}

int EmptyNodeFilter::Attach(MEM_HANDLE_T v)
{
	if (INVALID_HANDLE == v)
	{
		snprintf(_errmsg, sizeof(_errmsg),
				 "attach Empty-Node Filter failed, memory handle = 0");
		return -1;
	}

	_enf = M_POINTER(ENF_T, v);
	return 0;
}

int EmptyNodeFilter::Detach(void)
{
	_enf = 0;
	_errmsg[0] = 0;

	return 0;
}
