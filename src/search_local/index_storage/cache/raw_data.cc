/*
 * =====================================================================================
 *
 *       Filename:  raw_data.cc
 *
 *    Description:  raw data fundamental operation
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
#include <stdlib.h>
#include <string.h>

#include "raw_data.h"
#include "global.h"
#include "relative_hour_calculator.h"

#ifndef likely
#if __GCC_MAJOR >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#define GET_VALUE(x, t)                                     \
	do                                                      \
	{                                                       \
		if (unlikely(m_uiOffset + sizeof(t) > _size))       \
			goto ERROR_RET;                                 \
		x = (typeof(x)) * (t *)(m_pchContent + m_uiOffset); \
		m_uiOffset += sizeof(t);                            \
	} while (0)

#define GET_VALUE_AT_OFFSET(x, t, offset)               \
	do                                                  \
	{                                                   \
		if (unlikely(offset + sizeof(t) > _size))       \
			goto ERROR_RET;                             \
		x = (typeof(x)) * (t *)(m_pchContent + offset); \
	} while (0)

#define SET_VALUE(x, t)                               \
	do                                                \
	{                                                 \
		if (unlikely(m_uiOffset + sizeof(t) > _size)) \
			goto ERROR_RET;                           \
		*(t *)(m_pchContent + m_uiOffset) = x;        \
		m_uiOffset += sizeof(t);                      \
	} while (0)

#define SET_VALUE_AT_OFFSET(x, t, offset)         \
	do                                            \
	{                                             \
		if (unlikely(offset + sizeof(t) > _size)) \
			goto ERROR_RET;                       \
		*(t *)(m_pchContent + offset) = x;        \
	} while (0)

#define SET_BIN_VALUE(p, len)                                 \
	do                                                        \
	{                                                         \
		if (unlikely(m_uiOffset + sizeof(int) + len > _size)) \
			goto ERROR_RET;                                   \
		*(int *)(m_pchContent + m_uiOffset) = len;            \
		m_uiOffset += sizeof(int);                            \
		if (likely(len != 0))                                 \
			memcpy(m_pchContent + m_uiOffset, p, len);        \
		m_uiOffset += len;                                    \
	} while (0)

#define CHECK_SIZE(s)                         \
	do                                        \
	{                                         \
		if (unlikely(m_uiOffset + s > _size)) \
			goto ERROR_RET;                   \
	} while (0)

#define SKIP_SIZE(s)                          \
	do                                        \
	{                                         \
		if (unlikely(m_uiOffset + s > _size)) \
			goto ERROR_RET;                   \
		m_uiOffset += s;                      \
	} while (0)
const int BTYE_MAX_VALUE = 255;
RawData::RawData(Mallocator *pstMalloc, int iAutoDestroy)
{
	m_uiDataSize = 0;
	m_uiRowCnt = 0;
	m_iKeySize = 0;
	m_iLAId = -1;
	m_iExpireId = -1;
	m_iTableIdx = -1;
	m_uiKeyStart = 0;
	m_uiDataStart = 0;
	m_uiOffset = 0;
	m_uiLAOffset = 0;
	m_uiRowOffset = 0;
	m_uiGetCountOffset = 0;
	m_uiTimeStampOffSet = 0;
	m_uchGetCount = 0;
	m_CreateHour = 0;
	m_LastAccessHour = 0;
	m_LastUpdateHour = 0;
	m_uchKeyIdx = -1;
	m_pchContent = NULL;
	m_uiNeedSize = 0;
	_mallocator = pstMalloc;
	_handle = INVALID_HANDLE;
	_autodestroy = iAutoDestroy;
	_size = 0;
	m_pstRef = NULL;
	memset(m_szErr, 0, sizeof(m_szErr));
}

RawData::~RawData()
{
	if (_autodestroy)
	{
		Destroy();
	}
	_handle = INVALID_HANDLE;
	_size = 0;
}

int RawData::Init(uint8_t uchKeyIdx, int iKeySize, const char *pchKey, ALLOC_SIZE_T uiDataSize, int laId, int expireId, int nodeIdx)
{
	int ks = iKeySize != 0 ? iKeySize : 1 + *(unsigned char *)pchKey;

	/*|1字节:类型|4字节:数据大小|4字节: 行数| 1字节 : Get次数| 2字节: 最后访问时间| 2字节 : 最后更新时间|2字节: 最后创建时间 |key|*/
	uiDataSize += 2 + sizeof(uint32_t) * 2 + sizeof(uint16_t) * 3 + ks;

	_handle = INVALID_HANDLE;
	_size = 0;

	_handle = _mallocator->Malloc(uiDataSize);
	if (_handle == INVALID_HANDLE)
	{
		snprintf(m_szErr, sizeof(m_szErr), "malloc error");
		m_uiNeedSize = uiDataSize;
		return (EC_NO_MEM);
	}
	_size = _mallocator->chunk_size(_handle);

	m_uiDataSize = 2 + sizeof(uint32_t) * 2 + sizeof(uint16_t) * 3 + ks;
	m_uiRowCnt = 0;
	m_uchKeyIdx = uchKeyIdx;
	m_iKeySize = iKeySize;
	m_iLAId = laId;
	m_iExpireId = expireId;

	m_pchContent = Pointer<char>();
	m_uiOffset = 0;
	m_uiLAOffset = 0;
	if (nodeIdx != -1)
	{
		m_iTableIdx = nodeIdx;
	}
	if (m_iTableIdx != 0 && m_iTableIdx != 1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "node idx error");
		return -100;
	}
	SET_VALUE(((m_iTableIdx << 7) & 0x80) + DATA_TYPE_RAW, unsigned char);
	SET_VALUE(m_uiDataSize, uint32_t);
	SET_VALUE(m_uiRowCnt, uint32_t);

	m_uiGetCountOffset = m_uiOffset;
	m_uchGetCount = 1;
	SET_VALUE(m_uchGetCount, uint8_t);
	m_uiTimeStampOffSet = m_uiOffset;
	init_timp_stamp();
	SKIP_SIZE(3 * sizeof(uint16_t));
	m_uiKeyStart = m_uiOffset;
	if (iKeySize != 0)
	{
		memcpy(m_pchContent + m_uiOffset, pchKey, iKeySize);
		m_uiOffset += iKeySize;
	}
	else
	{
		memcpy(m_pchContent + m_uiOffset, pchKey, ks);
		m_uiOffset += ks;
	}
	m_uiDataStart = m_uiOffset;
	m_uiRowOffset = m_uiDataStart;

	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "set value error");
	return (-100);
}

int RawData::Init(const char *pchKey, ALLOC_SIZE_T uiDataSize)
{
	if (DTCColExpand::Instance()->is_expanding())
		m_iTableIdx = (DTCColExpand::Instance()->cur_table_idx() + 1) % 2;
	else
		m_iTableIdx = DTCColExpand::Instance()->cur_table_idx() % 2;
	if (m_iTableIdx != 0 && m_iTableIdx != 1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, nodeIdx[%d] error", m_iTableIdx);
		return -1;
	}
	_tabledef = TableDefinitionManager::Instance()->get_table_def_by_idx(m_iTableIdx);
	if (_tabledef == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, tabledef[NULL]");
		return -1;
	}

	return Init(_tabledef->key_fields() - 1, _tabledef->key_format(), pchKey, uiDataSize, _tabledef->lastacc_field_id(), _tabledef->expire_time_field_id());
}

int RawData::Attach(MEM_HANDLE_T hHandle)
{
	_handle = hHandle;
	char *p = Pointer<char>();
	m_iTableIdx = (*p >> 7) & 0x01;
	if (m_iTableIdx != 0 && m_iTableIdx != 1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, nodeIdx[%d] error", m_iTableIdx);
		return -1;
	}
	_tabledef = TableDefinitionManager::Instance()->get_table_def_by_idx(m_iTableIdx);
	if (_tabledef == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, tabledef[NULL]");
		return -1;
	}
	return Attach(hHandle, _tabledef->key_fields() - 1, _tabledef->key_format(), _tabledef->lastacc_field_id(), _tabledef->lastcmod_field_id(), _tabledef->expire_time_field_id());
}

/* this function belive that inputted raw data is formatted correclty, but it's not the case sometimes */
int RawData::Attach(MEM_HANDLE_T hHandle, uint8_t uchKeyIdx, int iKeySize, int laid, int lcmodid, int expireid)
{
	int ks = 0;

	_size = _mallocator->chunk_size(hHandle);
	if (unlikely(_size == 0))
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error: %s", _mallocator->get_err_msg());
		return (-1);
	}
	_handle = hHandle;

	m_pchContent = Pointer<char>();
	m_uiOffset = 0;
	m_uiLAOffset = 0;
	unsigned char uchType;
	GET_VALUE(uchType, unsigned char);
	if (unlikely((uchType & 0x7f) != DATA_TYPE_RAW))
	{
		snprintf(m_szErr, sizeof(m_szErr), "invalid data type: %u", uchType);
		return (-2);
	}

	GET_VALUE(m_uiDataSize, uint32_t);
	GET_VALUE(m_uiRowCnt, uint32_t);
	m_uiGetCountOffset = m_uiOffset;
	GET_VALUE(m_uchGetCount, uint8_t);
	m_uiTimeStampOffSet = m_uiOffset;
	attach_time_stamp();
	SKIP_SIZE(3 * sizeof(uint16_t));
	if (unlikely(m_uiDataSize > _size))
	{
		snprintf(m_szErr, sizeof(m_szErr), "raw-data handle[" UINT64FMT "] data size[%u] error, large than chunk size[" UINT64FMT "]", hHandle, m_uiDataSize, _size);
		return (-3);
	}

	m_uchKeyIdx = uchKeyIdx;
	m_uiKeyStart = m_uiOffset;
	m_iKeySize = iKeySize;
	m_iLAId = laid;
	m_iLCmodId = lcmodid;
	m_iExpireId = expireid;

	ks = iKeySize != 0 ? iKeySize : 1 + *(unsigned char *)(m_pchContent + m_uiKeyStart);
	SKIP_SIZE(ks);
	m_uiDataStart = m_uiOffset;
	m_uiRowOffset = m_uiDataStart;

	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "get value error");
	return (-100);
}

int RawData::Destroy()
{
	if (_handle == INVALID_HANDLE)
	{
		_size = 0;
		return 0;
	}

	int iRet = _mallocator->Free(_handle);
	_handle = INVALID_HANDLE;
	_size = 0;
	return (iRet);
}

int RawData::check_size(MEM_HANDLE_T hHandle, uint8_t uchKeyIdx, int iKeySize, int size)
{
	_size = _mallocator->chunk_size(hHandle);
	if (unlikely(_size == 0))
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error: %s", _mallocator->get_err_msg());
		return (-1);
	}
	_handle = hHandle;

	m_pchContent = Pointer<char>();
	m_uiOffset = 0;
	m_uiLAOffset = 0;
	unsigned char uchType;
	GET_VALUE(uchType, unsigned char);
	if (unlikely(uchType != DATA_TYPE_RAW))
	{
		snprintf(m_szErr, sizeof(m_szErr), "invalid data type: %u", uchType);
		return (-2);
	}

	GET_VALUE(m_uiDataSize, uint32_t);
	if (m_uiDataSize != (unsigned int)size)
	{
		snprintf(m_szErr, sizeof(m_szErr), "invalid data type: %u", uchType);
		return -1;
	}

	return 0;
ERROR_RET:
	return -1;
}

int RawData::strip_mem()
{
	ALLOC_HANDLE_T hTmp = _mallocator->ReAlloc(_handle, m_uiDataSize);
	if (hTmp == INVALID_HANDLE)
	{
		snprintf(m_szErr, sizeof(m_szErr), "realloc error");
		m_uiNeedSize = m_uiDataSize;
		return (EC_NO_MEM);
	}
	_handle = hTmp;
	_size = _mallocator->chunk_size(_handle);
	m_pchContent = Pointer<char>();

	return (0);
}

int RawData::decode_row(RowValue &stRow, unsigned char &uchRowFlags, int iDecodeFlag)
{
	if (unlikely(_handle == INVALID_HANDLE || m_pchContent == NULL))
	{
		snprintf(m_szErr, sizeof(m_szErr), "rawdata not init yet");
		return (-1);
	}

	ALLOC_SIZE_T uiOldOffset = m_uiOffset;
	ALLOC_SIZE_T uiOldRowOffset = m_uiRowOffset;
	m_uiLAOffset = 0;
	m_uiRowOffset = m_uiOffset;
	GET_VALUE(uchRowFlags, unsigned char);

	for (int j = m_uchKeyIdx + 1; j <= stRow.num_fields(); j++) //拷贝一行数据
	{
		if (stRow.table_definition()->is_discard(j))
			continue;
		if (j == m_iLAId)
			m_uiLAOffset = m_uiOffset;
		switch (stRow.field_type(j))
		{
		case DField::Signed:
			if (unlikely(stRow.field_size(j) > (int)sizeof(int32_t)))
			{
				GET_VALUE(stRow.field_value(j)->s64, int64_t);
			}
			else
			{
				GET_VALUE(stRow.field_value(j)->s64, int32_t);
			}
			break;

		case DField::Unsigned:
			if (unlikely(stRow.field_size(j) > (int)sizeof(uint32_t)))
			{
				GET_VALUE(stRow.field_value(j)->u64, uint64_t);
			}
			else
			{
				GET_VALUE(stRow.field_value(j)->u64, uint32_t);
			}
			break;

		case DField::Float: //浮点数
			if (likely(stRow.field_size(j) > (int)sizeof(float)))
			{
				GET_VALUE(stRow.field_value(j)->flt, double);
			}
			else
			{
				GET_VALUE(stRow.field_value(j)->flt, float);
			}
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			GET_VALUE(stRow.field_value(j)->bin.len, int);
			stRow.field_value(j)->bin.ptr = m_pchContent + m_uiOffset;
			SKIP_SIZE(stRow.field_value(j)->bin.len);
			break;
		}
		} //end of switch
	}

	if (unlikely(iDecodeFlag & PRE_DECODE_ROW))
	{
		m_uiOffset = uiOldOffset;
		m_uiRowOffset = uiOldRowOffset;
	}

	return (0);

ERROR_RET:
	if (unlikely(iDecodeFlag & PRE_DECODE_ROW))
	{
		m_uiOffset = uiOldOffset;
		m_uiRowOffset = uiOldRowOffset;
	}
	snprintf(m_szErr, sizeof(m_szErr), "get value error");
	return (-100);
}

int RawData::get_expire_time(DTCTableDefinition *t, uint32_t &expire)
{
	expire = 0;
	if (unlikely(_handle == INVALID_HANDLE || m_pchContent == NULL))
	{
		snprintf(m_szErr, sizeof(m_szErr), "rawdata not init yet");
		return (-1);
	}
	if (m_iExpireId == -1)
	{
		expire = 0;
		return 0;
	}
	SKIP_SIZE(sizeof(unsigned char)); //skip flag
	// the first field should be expire time
	for (int j = m_uchKeyIdx + 1; j <= _tabledef->num_fields(); j++)
	{ //拷贝一行数据
		if (j == m_iExpireId)
		{
			expire = *((uint32_t *)(m_pchContent + m_uiOffset));
			break;
		}

		switch (_tabledef->field_type(j))
		{
		case DField::Unsigned:
		case DField::Signed:
			if (_tabledef->field_size(j) > (int)sizeof(int32_t))
				SKIP_SIZE(sizeof(int64_t));
			else
				SKIP_SIZE(sizeof(int32_t));
			;
			break;

		case DField::Float: //浮点数
			if (_tabledef->field_size(j) > (int)sizeof(float))
				SKIP_SIZE(sizeof(double));
			else
				SKIP_SIZE(sizeof(float));
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
			int iLen = 0;
			GET_VALUE(iLen, int);
			SKIP_SIZE(iLen);
			break;
		} //end of switch
	}
	return 0;

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "get expire error");
	return (-100);
}

int RawData::get_lastcmod(uint32_t &lastcmod)
{
	lastcmod = 0;
	if (unlikely(_handle == INVALID_HANDLE || m_pchContent == NULL))
	{
		snprintf(m_szErr, sizeof(m_szErr), "rawdata not init yet");
		return (-1);
	}

	m_uiRowOffset = m_uiOffset;
	SKIP_SIZE(sizeof(unsigned char)); //skip flag

	for (int j = m_uchKeyIdx + 1; j <= _tabledef->num_fields(); j++) //拷贝一行数据
	{
		//id: bug fix skip discard
		if (_tabledef->is_discard(j))
			continue;
		if (j == m_iLCmodId)
			lastcmod = *((uint32_t *)(m_pchContent + m_uiOffset));

		switch (_tabledef->field_type(j))
		{
		case DField::Unsigned:
		case DField::Signed:
			if (_tabledef->field_size(j) > (int)sizeof(int32_t))
				SKIP_SIZE(sizeof(int64_t));
			else
				SKIP_SIZE(sizeof(int32_t));
			;
			break;

		case DField::Float: //浮点数
			if (_tabledef->field_size(j) > (int)sizeof(float))
				SKIP_SIZE(sizeof(double));
			else
				SKIP_SIZE(sizeof(float));
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			int iLen = 0;
			GET_VALUE(iLen, int);
			SKIP_SIZE(iLen);
			break;
		}
		} //end of switch
	}
	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "get timecmod error");
	return (-100);
}

int RawData::set_data_size()
{
	SET_VALUE_AT_OFFSET(m_uiDataSize, uint32_t, 1);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "set data size error");
	return (-100);
}

int RawData::set_row_count()
{
	SET_VALUE_AT_OFFSET(m_uiRowCnt, uint32_t, 5);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "set row count error");
	return (-100);
}

int RawData::expand_chunk(ALLOC_SIZE_T tExpSize)
{
	if (_handle == INVALID_HANDLE)
	{
		snprintf(m_szErr, sizeof(m_szErr), "data not init yet");
		return (-1);
	}

	if (m_uiDataSize + tExpSize > _size)
	{
		ALLOC_HANDLE_T hTmp = _mallocator->ReAlloc(_handle, m_uiDataSize + tExpSize);
		if (hTmp == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "realloc error[%s]", _mallocator->get_err_msg());
			m_uiNeedSize = m_uiDataSize + tExpSize;
			return (EC_NO_MEM);
		}
		_handle = hTmp;
		_size = _mallocator->chunk_size(_handle);
		m_pchContent = Pointer<char>();
	}

	return (0);
}

int RawData::re_alloc_chunk(ALLOC_SIZE_T tSize)
{
	if (tSize > _size)
	{
		ALLOC_HANDLE_T hTmp = _mallocator->ReAlloc(_handle, tSize);
		if (hTmp == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "realloc error");
			m_uiNeedSize = tSize;
			return (EC_NO_MEM);
		}
		_handle = hTmp;
		_size = _mallocator->chunk_size(_handle);
		m_pchContent = Pointer<char>();
	}

	return (0);
}

ALLOC_SIZE_T RawData::calc_row_size(const RowValue &stRow, int keyIdx)
{
	if (keyIdx == -1)
		log_error("RawData may not init yet...");
	ALLOC_SIZE_T tSize = 1;								  // flag
	for (int j = keyIdx + 1; j <= stRow.num_fields(); j++) //拷贝一行数据
	{
		if (stRow.table_definition()->is_discard(j))
			continue;
		switch (stRow.field_type(j))
		{
		case DField::Signed:
		case DField::Unsigned:
			tSize += unlikely(stRow.field_size(j) > (int)sizeof(int32_t)) ? sizeof(int64_t) : sizeof(int32_t);
			break;

		case DField::Float: //浮点数
			tSize += likely(stRow.field_size(j) > (int)sizeof(float)) ? sizeof(double) : sizeof(float);
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			tSize += sizeof(int);
			tSize += stRow.field_value(j)->bin.len;
			break;
		}
		} //end of switch
	}
	if (tSize < 2)
		log_notice("m_uchKeyIdx:%d, stRow.num_fields():%d tSize:%d", keyIdx, stRow.num_fields(), tSize);

	return (tSize);
}

int RawData::encode_row(const RowValue &stRow, unsigned char uchOp, bool expendBuf)
{
	int iRet;

	ALLOC_SIZE_T tSize;
	tSize = calc_row_size(stRow, m_uchKeyIdx);

	if (unlikely(expendBuf))
	{
		iRet = expand_chunk(tSize);
		if (unlikely(iRet != 0))
			return (iRet);
	}

	SET_VALUE(uchOp, unsigned char);

	for (int j = m_uchKeyIdx + 1; j <= stRow.num_fields(); j++) //拷贝一行数据
	{
		if (stRow.table_definition()->is_discard(j))
			continue;
		const DTCValue *const v = stRow.field_value(j);
		switch (stRow.field_type(j))
		{
		case DField::Signed:
			if (unlikely(stRow.field_size(j) > (int)sizeof(int32_t)))
				SET_VALUE(v->s64, int64_t);
			else
				SET_VALUE(v->s64, int32_t);
			break;

		case DField::Unsigned:
			if (unlikely(stRow.field_size(j) > (int)sizeof(uint32_t)))
				SET_VALUE(v->u64, uint64_t);
			else
				SET_VALUE(v->u64, uint32_t);
			break;

		case DField::Float: //浮点数
			if (likely(stRow.field_size(j) > (int)sizeof(float)))
				SET_VALUE(v->flt, double);
			else
				SET_VALUE(v->flt, float);
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			SET_BIN_VALUE(v->bin.ptr, v->bin.len);
			break;
		}
		} //end of switch
	}

	m_uiDataSize += tSize;
	set_data_size();
	m_uiRowCnt++;
	set_row_count();

	return 0;

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "encode row error");
	return (-100);
}

int RawData::insert_row_flag(const RowValue &stRow, bool byFirst, unsigned char uchOp)
{

	uint32_t uiOldSize = m_uiDataSize;

	m_uiOffset = m_uiDataSize;
	int iRet = encode_row(stRow, uchOp);
	uint32_t uiNewRowSize = m_uiDataSize - uiOldSize;
	if (iRet == 0 && byFirst == true && uiNewRowSize > 0 && (uiOldSize - m_uiDataStart) > 0)
	{
		void *pBuf = MALLOC(uiNewRowSize);
		if (pBuf == NULL)
		{
			snprintf(m_szErr, sizeof(m_szErr), "malloc error: %m");
			return (-ENOMEM);
		}
		char *pchDataStart = m_pchContent + m_uiDataStart;
		// save last row
		memmove(pBuf, m_pchContent + uiOldSize, uiNewRowSize);
		// move buf up sz bytes
		memmove(pchDataStart + uiNewRowSize, pchDataStart, uiOldSize - m_uiDataStart);
		// last row as first row
		memcpy(pchDataStart, pBuf, uiNewRowSize);
		FREE(pBuf);
	}

	return (iRet);
}

int RawData::insert_row(const RowValue &stRow, bool byFirst, bool isDirty)
{
	return insert_row_flag(stRow, byFirst, isDirty ? OPER_INSERT : OPER_SELECT);
}

int RawData::insert_n_rows(unsigned int uiNRows, const RowValue *pstRow, bool byFirst, bool isDirty)
{
	int iRet;
	unsigned int i;
	ALLOC_SIZE_T tSize;

	tSize = 0;
	for (i = 0; i < uiNRows; i++)
		tSize += calc_row_size(pstRow[i], m_uchKeyIdx);

	iRet = expand_chunk(tSize); // 先扩大buffer，避免后面insert失败回滚
	if (iRet != 0)
		return (iRet);

	uint32_t uiOldSize = m_uiDataSize;
	m_uiOffset = m_uiDataSize;
	for (i = 0; i < uiNRows; i++)
	{
		iRet = encode_row(pstRow[i], isDirty ? OPER_INSERT : OPER_SELECT);
		if (iRet != 0)
		{
			return (iRet);
		}
	}

	uint32_t uiNewRowSize = m_uiDataSize - uiOldSize;
	if (byFirst == true && uiNewRowSize > 0 && (uiOldSize - m_uiDataStart) > 0)
	{
		void *pBuf = MALLOC(uiNewRowSize);
		if (pBuf == NULL)
		{
			snprintf(m_szErr, sizeof(m_szErr), "malloc error: %m");
			return (-ENOMEM);
		}
		char *pchDataStart = m_pchContent + m_uiDataStart;
		// save last row
		memmove(pBuf, m_pchContent + uiOldSize, uiNewRowSize);
		// move buf up sz bytes
		memmove(pchDataStart + uiNewRowSize, pchDataStart, uiOldSize - m_uiDataStart);
		// last row as first row
		memcpy(pchDataStart, pBuf, uiNewRowSize);
		FREE(pBuf);
	}

	return (0);
}

int RawData::skip_row(const RowValue &stRow)
{
	if (_handle == INVALID_HANDLE || m_pchContent == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "rawdata not init yet");
		return (-1);
	}

	m_uiOffset = m_uiRowOffset;
	if (m_uiOffset >= m_uiDataSize)
	{
		snprintf(m_szErr, sizeof(m_szErr), "already at end of data");
		return (-2);
	}

	SKIP_SIZE(sizeof(unsigned char)); // flag

	for (int j = m_uchKeyIdx + 1; j <= stRow.num_fields(); j++) //拷贝一行数据
	{
		//id: bug fix skip discard
		if (stRow.table_definition()->is_discard(j))
			continue;

		switch (stRow.field_type(j))
		{
		case DField::Unsigned:
		case DField::Signed:
			if (stRow.field_size(j) > (int)sizeof(int32_t))
				SKIP_SIZE(sizeof(int64_t));
			else
				SKIP_SIZE(sizeof(int32_t));
			;
			break;

		case DField::Float: //浮点数
			if (stRow.field_size(j) > (int)sizeof(float))
				SKIP_SIZE(sizeof(double));
			else
				SKIP_SIZE(sizeof(float));
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			int iLen;
			GET_VALUE(iLen, int);
			SKIP_SIZE(iLen);
			break;
		}
		} //end of switch
	}

	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "skip row error");
	return (-100);
}

int RawData::replace_cur_row(const RowValue &stRow, bool isDirty)
{
	int iRet = 0;
	ALLOC_SIZE_T uiOldOffset;
	ALLOC_SIZE_T uiNewRowSize;
	ALLOC_SIZE_T uiCurRowSize;
	ALLOC_SIZE_T uiNextRowsOffset;
	ALLOC_SIZE_T uiNextRowsSize;

	uiOldOffset = m_uiOffset;
	if ((iRet = skip_row(stRow)) != 0)
	{
		goto ERROR_RET;
	}

	unsigned char uchRowFlag;
	GET_VALUE_AT_OFFSET(uchRowFlag, unsigned char, m_uiRowOffset);
	if (isDirty)
		uchRowFlag = OPER_UPDATE;

	uiNewRowSize = calc_row_size(stRow, m_uchKeyIdx);
	uiCurRowSize = m_uiOffset - m_uiRowOffset;
	uiNextRowsOffset = m_uiOffset;
	uiNextRowsSize = m_uiDataSize - m_uiOffset;

	if (uiNewRowSize > uiCurRowSize)
	{
		// enlarge buffer
		MEM_HANDLE_T hTmp = _mallocator->ReAlloc(_handle, m_uiDataSize + uiNewRowSize - uiCurRowSize);
		if (hTmp == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "realloc error");
			m_uiNeedSize = m_uiDataSize + uiNewRowSize - uiCurRowSize;
			iRet = EC_NO_MEM;
			goto ERROR_RET;
		}
		_handle = hTmp;
		_size = _mallocator->chunk_size(_handle);
		m_pchContent = Pointer<char>();

		// move data
		if (uiNextRowsSize > 0)
			memmove(m_pchContent + uiNextRowsOffset + (uiNewRowSize - uiCurRowSize), m_pchContent + uiNextRowsOffset, uiNextRowsSize);

		// copy new row
		m_uiOffset = m_uiRowOffset;
		iRet = encode_row(stRow, uchRowFlag, false);
		if (iRet != 0)
		{
			if (uiNextRowsSize > 0)
				memmove(m_pchContent + uiNextRowsOffset, m_pchContent + uiNextRowsOffset + (uiNewRowSize - uiCurRowSize), uiNextRowsSize);
			iRet = -1;
			goto ERROR_RET;
		}

		m_uiRowCnt--;
		m_uiDataSize -= uiCurRowSize;
	}
	else
	{
		// back up old row
		void *pTmpBuf = MALLOC(uiCurRowSize);
		if (pTmpBuf == NULL)
		{
			snprintf(m_szErr, sizeof(m_szErr), "malloc error: %m");
			return (-ENOMEM);
		}
		memmove(pTmpBuf, m_pchContent + m_uiRowOffset, uiCurRowSize);

		// copy new row
		m_uiOffset = m_uiRowOffset;
		iRet = encode_row(stRow, uchRowFlag, false);
		if (iRet != 0)
		{
			memmove(m_pchContent + m_uiRowOffset, pTmpBuf, uiCurRowSize);
			FREE(pTmpBuf);
			iRet = -1;
			goto ERROR_RET;
		}

		// move data
		if (uiNextRowsSize > 0 && m_uiOffset != uiNextRowsOffset)
			memmove(m_pchContent + m_uiOffset, m_pchContent + uiNextRowsOffset, uiNextRowsSize);
		FREE(pTmpBuf);

		// shorten buffer
		MEM_HANDLE_T hTmp = _mallocator->ReAlloc(_handle, m_uiDataSize + uiNewRowSize - uiCurRowSize);
		if (hTmp != INVALID_HANDLE)
		{
			_handle = hTmp;
			_size = _mallocator->chunk_size(_handle);
			m_pchContent = Pointer<char>();
		}

		m_uiRowCnt--;
		m_uiDataSize -= uiCurRowSize;
	}

	set_data_size();
	set_row_count();

	return (0);

ERROR_RET:
	m_uiOffset = uiOldOffset;
	return (iRet);
}

int RawData::delete_cur_row(const RowValue &stRow)
{
	int iRet = 0;
	ALLOC_SIZE_T uiOldOffset;
	ALLOC_SIZE_T uiNextRowsSize;

	uiOldOffset = m_uiOffset;
	if ((iRet = skip_row(stRow)) != 0)
	{
		goto ERROR_RET;
	}
	uiNextRowsSize = m_uiDataSize - m_uiOffset;

	memmove(m_pchContent + m_uiRowOffset, m_pchContent + m_uiOffset, uiNextRowsSize);
	m_uiDataSize -= (m_uiOffset - m_uiRowOffset);
	m_uiRowCnt--;
	set_data_size();
	set_row_count();

	m_uiOffset = m_uiRowOffset;
	return (iRet);

ERROR_RET:
	m_uiOffset = uiOldOffset;
	return (iRet);
}

int RawData::delete_all_rows()
{
	m_uiDataSize = m_uiDataStart;
	m_uiRowOffset = m_uiDataStart;
	m_uiRowCnt = 0;
	m_uiOffset = m_uiDataSize;

	set_data_size();
	set_row_count();

	m_uiNeedSize = 0;

	return (0);
}

int RawData::set_cur_row_flag(unsigned char uchFlag)
{
	if (m_uiRowOffset >= m_uiDataSize)
	{
		snprintf(m_szErr, sizeof(m_szErr), "no more rows");
		return (-1);
	}
	*(unsigned char *)(m_pchContent + m_uiRowOffset) = uchFlag;

	return (0);
}

int RawData::copy_row()
{
	int iRet;
	ALLOC_SIZE_T uiSize = m_pstRef->m_uiOffset - m_pstRef->m_uiRowOffset;
	if ((iRet = expand_chunk(uiSize)) != 0)
		return (iRet);

	memcpy(m_pchContent + m_uiOffset, m_pstRef->m_pchContent + m_pstRef->m_uiRowOffset, uiSize);
	m_uiOffset += uiSize;
	m_uiDataSize += uiSize;
	m_uiRowCnt++;

	set_data_size();
	set_row_count();

	return (0);
}

int RawData::copy_all()
{
	int iRet;
	ALLOC_SIZE_T uiSize = m_pstRef->m_uiDataSize;
	if ((iRet = re_alloc_chunk(uiSize)) != 0)
		return (iRet);

	memcpy(m_pchContent, m_pstRef->m_pchContent, uiSize);

	if ((iRet = Attach(_handle)) != 0)
		return (iRet);

	return (0);
}

int RawData::append_n_records(unsigned int uiNRows, const char *pchData, const unsigned int uiLen)
{
	int iRet;

	iRet = expand_chunk(uiLen);
	if (iRet != 0)
		return (iRet);

	memcpy(m_pchContent + m_uiDataSize, pchData, uiLen);
	m_uiDataSize += uiLen;
	m_uiRowCnt += uiNRows;

	set_data_size();
	set_row_count();

	return (0);
}

void RawData::init_timp_stamp()
{
	if (unlikely(NULL == m_pchContent))
	{
		return;
	}

	if (unlikely(m_uiOffset + 3 * sizeof(uint16_t) > _size))
	{
		return;
	}
	uint16_t dwCurHour = RELATIVE_HOUR_CALCULATOR->get_relative_hour();

	m_LastAccessHour = dwCurHour;
	m_LastUpdateHour = dwCurHour;
	m_CreateHour = dwCurHour;

	*(uint16_t *)(m_pchContent + m_uiTimeStampOffSet) = dwCurHour;
	*(uint16_t *)(m_pchContent + m_uiTimeStampOffSet + sizeof(uint16_t)) = dwCurHour;
	*(uint16_t *)(m_pchContent + m_uiTimeStampOffSet + 2 * sizeof(uint16_t)) = dwCurHour;
}

void RawData::attach_time_stamp()
{
	if (unlikely(NULL == m_pchContent))
	{
		return;
	}
	if (unlikely(m_uiTimeStampOffSet + 3 * sizeof(uint16_t) > _size))
	{
		return;
	}
	m_LastAccessHour = *(uint16_t *)(m_pchContent + m_uiTimeStampOffSet);
	m_LastUpdateHour = *(uint16_t *)(m_pchContent + m_uiTimeStampOffSet + sizeof(uint16_t));
	m_CreateHour = *(uint16_t *)(m_pchContent + m_uiTimeStampOffSet + 2 * sizeof(uint16_t));
}
void RawData::update_last_access_time_by_hour()
{
	if (unlikely(NULL == m_pchContent))
	{
		return;
	}
	if (unlikely(m_uiTimeStampOffSet + sizeof(uint16_t) > _size))
	{
		return;
	}
	m_LastAccessHour = RELATIVE_HOUR_CALCULATOR->get_relative_hour();
	*(uint16_t *)(m_pchContent + m_uiTimeStampOffSet) = m_LastAccessHour;
}
void RawData::update_last_update_time_by_hour()
{
	if (unlikely(NULL == m_pchContent))
	{
		return;
	}
	if (unlikely(m_uiTimeStampOffSet + 2 * sizeof(uint16_t) > _size))
	{
		return;
	}
	m_LastUpdateHour = RELATIVE_HOUR_CALCULATOR->get_relative_hour();
	*(uint16_t *)(m_pchContent + m_uiTimeStampOffSet + sizeof(uint16_t)) = m_LastUpdateHour;
}
uint32_t RawData::get_create_time_by_hour()
{
	return m_CreateHour;
}
uint32_t RawData::get_last_access_time_by_hour()
{

	return m_LastAccessHour;
}

uint32_t RawData::get_last_update_time_by_hour()
{

	return m_LastUpdateHour;
}
uint32_t RawData::get_select_op_count()
{
	return m_uchGetCount;
}

void RawData::inc_select_count()
{

	if (unlikely(m_uchGetCount >= BTYE_MAX_VALUE))
	{
		return;
	}
	if (unlikely(m_uiGetCountOffset + sizeof(uint8_t) > _size))
	{
		return;
	}
	m_uchGetCount++;
	*(uint8_t *)(m_pchContent + m_uiGetCountOffset) = m_uchGetCount;
}

DTCTableDefinition *RawData::get_node_table_def()
{
	return _tabledef;
}
