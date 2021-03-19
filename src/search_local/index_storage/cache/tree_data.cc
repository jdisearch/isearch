/*
 * =====================================================================================
 *
 *       Filename:  tree_data.cc
 *
 *    Description:  T-tree data struct operation. For user invoke.
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

#include "tree_data.h"
#include "global.h"
#include "task_pkey.h"
#include "buffer_flush.h"
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

#define GET_TREE_VALUE(x, t)                                \
	do                                                      \
	{                                                       \
		if (unlikely(m_uiOffset + sizeof(t) > _size))       \
			goto ERROR_RET;                                 \
		x = (typeof(x)) * (t *)(m_pchContent + m_uiOffset); \
		m_uiOffset += sizeof(t);                            \
	} while (0)

#define GET_TREE_VALUE_AT_OFFSET(x, t, offset)          \
	do                                                  \
	{                                                   \
		if (unlikely(offset + sizeof(t) > _size))       \
			goto ERROR_RET;                             \
		x = (typeof(x)) * (t *)(m_pchContent + offset); \
	} while (0)

#define SET_TREE_VALUE_AT_OFFSET(x, t, offset)    \
	do                                            \
	{                                             \
		if (unlikely(offset + sizeof(t) > _size)) \
			goto ERROR_RET;                       \
		*(t *)(m_pchContent + offset) = x;        \
	} while (0)

#define SET_TREE_VALUE(x, t)                          \
	do                                                \
	{                                                 \
		if (unlikely(m_uiOffset + sizeof(t) > _size)) \
			goto ERROR_RET;                           \
		*(t *)(m_pchContent + m_uiOffset) = x;        \
		m_uiOffset += sizeof(t);                      \
	} while (0)

#define SET_TREE_BIN_VALUE(p, len)                            \
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

#define SKIP_TREE_SIZE(s)                     \
	do                                        \
	{                                         \
		if (unlikely(m_uiOffset + s > _size)) \
			goto ERROR_RET;                   \
		m_uiOffset += s;                      \
	} while (0)

TreeData::TreeData(Mallocator *pstMalloc) : m_stTree(*pstMalloc)
{
	m_pstRootData = NULL;
	m_uchIndexDepth = 0;
	m_uiNeedSize = 0;
	m_iKeySize = 0;
	_handle = INVALID_HANDLE;
	m_iTableIdx = -1;
	_size = 0;
	_root_size = 0;
	_mallocator = pstMalloc;
	memset(m_szErr, 0, sizeof(m_szErr));

	m_uchKeyIdx = -1;
	m_iExpireId = -1;
	m_iLAId = -1;
	m_iLCmodId = -1;

	m_uiOffset = 0;
	m_uiRowOffset = 0;
	m_ullAffectedrows = 0;

	m_IndexPartOfUniqField = false;
	m_hRecord = INVALID_HANDLE;
}

TreeData::~TreeData()
{
	_handle = INVALID_HANDLE;
	_root_size = 0;
}

int TreeData::Init(uint8_t uchKeyIdx, int iKeySize, const char *pchKey, int laId, int expireId, int nodeIdx)
{
	int ks = iKeySize != 0 ? iKeySize : 1 + *(unsigned char *)pchKey;
	int uiDataSize = 2 + sizeof(uint32_t) * 4 + sizeof(uint16_t) * 3 + sizeof(MEM_HANDLE_T) + ks;

	_handle = INVALID_HANDLE;
	_root_size = 0;

	_handle = _mallocator->Malloc(uiDataSize);
	if (_handle == INVALID_HANDLE)
	{
		snprintf(m_szErr, sizeof(m_szErr), "malloc error");
		m_uiNeedSize = uiDataSize;
		return (EC_NO_MEM);
	}
	_root_size = _mallocator->chunk_size(_handle);

	m_pstRootData = Pointer<RootData>();
	m_pstRootData->m_uchDataType = ((m_iTableIdx << 7) & 0x80) + DATA_TYPE_TREE_ROOT;
	m_pstRootData->m_treeSize = 0;
	m_pstRootData->m_uiTotalRawSize = 0;
	m_pstRootData->m_uiNodeCnts = 0;
	m_pstRootData->m_uiRowCnt = 0;
	m_pstRootData->m_hRoot = INVALID_HANDLE;

	m_pstRootData->m_uchGetCount = 1;

	m_uiLAOffset = 0;

	m_iKeySize = iKeySize;
	m_uchKeyIdx = uchKeyIdx;
	m_iLAId = laId;
	m_iExpireId = expireId;
	if (nodeIdx != -1)
	{
		m_iTableIdx = nodeIdx;
	}
	if (m_iTableIdx != 0 && m_iTableIdx != 1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "node idx error");
		return -100;
	}

	if (iKeySize != 0)
	{
		memcpy(m_pstRootData->m_achKey, pchKey, iKeySize);
	}
	else
	{
		memcpy(m_pstRootData->m_achKey, pchKey, ks);
	}

	m_stTree.Attach(INVALID_HANDLE);

	return (0);
}

int TreeData::Init(const char *pchKey)
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
	m_pstTab = TableDefinitionManager::Instance()->get_table_def_by_idx(m_iTableIdx);
	if (m_pstTab == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, tabledef[NULL]");
		return -1;
	}

	return Init(m_pstTab->key_fields() - 1, m_pstTab->key_format(), pchKey, m_pstTab->lastacc_field_id(), m_pstTab->expire_time_field_id());
}

int TreeData::Attach(MEM_HANDLE_T hHandle, uint8_t uchKeyIdx, int iKeySize, int laid, int lcmodid, int expireid)
{
	_root_size = _mallocator->chunk_size(hHandle);
	if (unlikely(_root_size == 0))
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error: %s", _mallocator->get_err_msg());
		return (-1);
	}
	_handle = hHandle;

	m_pstRootData = Pointer<RootData>();

	unsigned char uchType;
	uchType = m_pstRootData->m_uchDataType;
	if (unlikely((uchType & 0x7f) != DATA_TYPE_TREE_ROOT))
	{
		snprintf(m_szErr, sizeof(m_szErr), "invalid data type: %u", uchType);
		return (-2);
	}

	m_uiLAOffset = 0;

	m_iKeySize = iKeySize;
	m_uchKeyIdx = uchKeyIdx;
	m_iExpireId = expireid;
	m_iLAId = laid;
	m_iLCmodId = lcmodid;

	m_stTree.Attach(m_pstRootData->m_hRoot);

	return (0);
}

int TreeData::Attach(MEM_HANDLE_T hHandle)
{
	_handle = hHandle;
	char *p = Pointer<char>();
	m_iTableIdx = (*p >> 7) & 0x01;
	if (m_iTableIdx != 0 && m_iTableIdx != 1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, nodeIdx[%d] error", m_iTableIdx);
		return -1;
	}
	m_pstTab = TableDefinitionManager::Instance()->get_table_def_by_idx(m_iTableIdx);
	if (m_pstTab == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error, tabledef[NULL]");
		return -1;
	}
	return Attach(hHandle, m_pstTab->key_fields() - 1, m_pstTab->key_format(), m_pstTab->lastacc_field_id(), m_pstTab->lastcmod_field_id(), m_pstTab->expire_time_field_id());
}

int TreeData::encode_tree_row(const RowValue &stRow, unsigned char uchOp)
{
	SET_TREE_VALUE(uchOp, unsigned char);
	for (int j = 1; j <= stRow.num_fields(); j++) //¿½±´Ò»ÐÐÊý¾Ý
	{
		if (stRow.table_definition()->is_discard(j))
			continue;
		const DTCValue *const v = stRow.field_value(j);
		switch (stRow.field_type(j))
		{
		case DField::Signed:
			if (unlikely(stRow.field_size(j) > (int)sizeof(int32_t)))
				SET_TREE_VALUE(v->s64, int64_t);
			else
				SET_TREE_VALUE(v->s64, int32_t);
			break;

		case DField::Unsigned:
			if (unlikely(stRow.field_size(j) > (int)sizeof(uint32_t)))
				SET_TREE_VALUE(v->u64, uint64_t);
			else
				SET_TREE_VALUE(v->u64, uint32_t);
			break;

		case DField::Float:
			if (likely(stRow.field_size(j) > (int)sizeof(float)))
				SET_TREE_VALUE(v->flt, double);
			else
				SET_TREE_VALUE(v->flt, float);
			break;

		case DField::String:
		case DField::Binary:
		default:
		{
			SET_TREE_BIN_VALUE(v->bin.ptr, v->bin.len);
			break;
		}
		} //end of switch
	}

	return 0;

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "encode row error");
	return (-100);
}

int TreeData::expand_tree_chunk(MEM_HANDLE_T *pRecord, ALLOC_SIZE_T tExpSize)
{
	if (pRecord == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "tree data not init yet");
		return (-1);
	}

	uint32_t dataSize = *(uint32_t *)(m_pchContent + sizeof(unsigned char));
	if (dataSize + tExpSize > _size)
	{
		ALLOC_HANDLE_T hTmp = _mallocator->ReAlloc((*pRecord), dataSize + tExpSize);
		if (hTmp == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "realloc error[%s]", _mallocator->get_err_msg());
			m_uiNeedSize = dataSize + tExpSize;
			return (EC_NO_MEM);
		}
		m_pstRootData->m_treeSize -= _size;
		*pRecord = hTmp;
		_size = _mallocator->chunk_size(hTmp);
		m_pchContent = Pointer<char>(*pRecord);
		m_pstRootData->m_treeSize += _size;
	}
	return (0);
}

int TreeData::insert_sub_tree(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T hRoot)
{
	int iRet;
	if (uchCondIdxCnt != TTREE_INDEX_POS)
	{
		snprintf(m_szErr, sizeof(m_szErr), "index field error");
		return (-100);
	}

	bool isAllocNode = false;
	DTCValue value = stCondition[TTREE_INDEX_POS];
	char *indexKey = reinterpret_cast<char *>(&value);
	CmpCookie cookie(m_pstTab, uchCondIdxCnt);
	iRet = m_stTree.Insert(indexKey, &cookie, pfComp, hRoot, isAllocNode);
	if (iRet == 0 && isAllocNode)
	{
		m_pstRootData->m_treeSize += sizeof(TtreeNode);
	}
	return iRet;
}

int TreeData::Find(uint8_t uchCondIdxCnt, const RowValue &stCondition, KeyComparator pfComp, ALLOC_HANDLE_T *&hRecord)
{
	int iRet;
	if (uchCondIdxCnt != TTREE_INDEX_POS)
	{
		snprintf(m_szErr, sizeof(m_szErr), "index field error");
		return (-100);
	}

	DTCValue value = stCondition[TTREE_INDEX_POS];
	char *indexKey = reinterpret_cast<char *>(&value);
	CmpCookie cookie(m_pstTab, uchCondIdxCnt);
	iRet = m_stTree.Find(indexKey, &cookie, pfComp, hRecord);
	return iRet;
}

int TreeData::insert_row_flag(const RowValue &stRow, KeyComparator pfComp, unsigned char uchFlag)
{
	int iRet;
	uint32_t rowCnt = 0;
	MEM_HANDLE_T *pRecord = NULL;
	MEM_HANDLE_T hRecord = INVALID_HANDLE;
	int trowSize = calc_tree_row_size(stRow, 0);
	int tSize = 0;
	m_uiOffset = 0;

	iRet = Find(TTREE_INDEX_POS, stRow, pfComp, pRecord);
	if (iRet == -100)
		return iRet;
	if (pRecord == NULL)
	{
		tSize = trowSize + sizeof(unsigned char) + sizeof(uint32_t) * 2;
		hRecord = _mallocator->Malloc(tSize);
		if (hRecord == INVALID_HANDLE)
		{
			m_uiNeedSize = tSize;
			snprintf(m_szErr, sizeof(m_szErr), "malloc error");
			return (EC_NO_MEM);
		}
		_size = _mallocator->chunk_size(hRecord);
		m_pchContent = Pointer<char>(hRecord);
		*m_pchContent = DATA_TYPE_TREE_NODE; //RawFormat->DataType
		m_uiOffset += sizeof(unsigned char);
		*(uint32_t *)(m_pchContent + m_uiOffset) = 0; //RawFormat->data_size
		m_uiOffset += sizeof(uint32_t);
		*(uint32_t *)(m_pchContent + m_uiOffset) = 0; //RawFormat->RowCount
		m_uiOffset += sizeof(uint32_t);

		iRet = encode_tree_row(stRow, uchFlag);
		if (iRet != 0)
		{
			goto ERROR_INSERT_RET;
		}

		iRet = insert_sub_tree(TTREE_INDEX_POS, stRow, pfComp, hRecord);
		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "insert error");
			m_uiNeedSize = sizeof(TtreeNode);
			_mallocator->Free(hRecord);
			goto ERROR_INSERT_RET;
		}
		m_pstRootData->m_treeSize += _size;
		m_pstRootData->m_uiNodeCnts++;
	}
	else
	{
		m_pchContent = Pointer<char>(*pRecord);
		_size = _mallocator->chunk_size(*pRecord);
		iRet = expand_tree_chunk(pRecord, trowSize);
		if (iRet != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "expand tree chunk error");
			return iRet;
		}

		m_uiOffset = *(uint32_t *)(m_pchContent + sizeof(unsigned char)); //datasize

		iRet = encode_tree_row(stRow, uchFlag);
		if (iRet != 0)
		{
			goto ERROR_INSERT_RET;
		}
	}

	/*每次insert数据之后，更新头部信息*/
	rowCnt = *(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t));
	*(uint32_t *)(m_pchContent + sizeof(unsigned char)) = m_uiOffset;
	*(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t)) = rowCnt + 1;
	m_pstRootData->m_hRoot = m_stTree.Root();
	m_pstRootData->m_uiRowCnt += 1;
	m_pstRootData->m_uiTotalRawSize += trowSize;

ERROR_INSERT_RET:
	m_uiOffset = 0;
	_size = 0;
	hRecord = INVALID_HANDLE;
	m_pchContent = NULL;

	return (iRet);
}

int TreeData::insert_row(const RowValue &stRow, KeyComparator pfComp, bool isDirty)
{
	return insert_row_flag(stRow, pfComp, isDirty ? OPER_INSERT : OPER_SELECT);
}

unsigned TreeData::ask_for_destroy_size(void)
{
	if (unlikely(_root_size == 0))
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error: %s", _mallocator->get_err_msg());
		return (-1);
	}
	return m_pstRootData->m_treeSize + _root_size;
}

int TreeData::Destroy()
{
	if (unlikely(_root_size == 0))
	{
		snprintf(m_szErr, sizeof(m_szErr), "attach error: %s", _mallocator->get_err_msg());
		return (-1);
	}
	m_stTree.Destroy();
	_mallocator->Free(_handle);

	_handle = INVALID_HANDLE;
	_root_size = 0;
	return (0);
}

int TreeData::copy_raw_all(RawData *pstRawData)
{
	int iRet;
	uint32_t totalNodeCnt = m_pstRootData->m_uiNodeCnts;
	if (totalNodeCnt == 0)
	{
		return 1;
	}
	pResCookie resCookie;
	MEM_HANDLE_T pCookie[totalNodeCnt];
	resCookie.m_handle = pCookie;
	resCookie.nodesNum = 0;
	iRet = m_stTree.traverse_forward(Visit, &resCookie);
	if (iRet != 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), " traverse tree-data rows error:%d", iRet);
		return (-1);
	}
	ALLOC_SIZE_T headlen = sizeof(unsigned char) + sizeof(uint32_t) * 2;
	for (uint32_t i = 0; i < resCookie.nodesGot; i++)
	{
		char *pch = Pointer<char>(pCookie[i]);
		ALLOC_SIZE_T dtsize = *(uint32_t *)(pch + sizeof(unsigned char));

		uint32_t rowcnt = *(uint32_t *)(pch + sizeof(unsigned char) + sizeof(uint32_t));
		iRet = pstRawData->append_n_records(rowcnt, pch + headlen, dtsize - headlen);
		if (iRet != 0)
			return iRet;
	}
	if ((iRet = pstRawData->Attach(pstRawData->get_handle())) != 0)
		return (iRet);

	return 0;
}

int TreeData::copy_tree_all(RawData *pstRawData)
{
	int iRet;
	if (m_pstTab->num_fields() < 1)
	{
		log_error("field nums is too short");
		return -1;
	}

	unsigned int uiTotalRows = pstRawData->total_rows();
	if (uiTotalRows == 0)
		return (0);

	pstRawData->rewind();
	RowValue stOldRow(m_pstTab);
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		unsigned char uchRowFlags;
		stOldRow.default_value();
		if (pstRawData->decode_row(stOldRow, uchRowFlags, 0) != 0)
		{
			log_error("raw-data decode row error: %s", pstRawData->get_err_msg());
			return (-1);
		}

		iRet = insert_row(stOldRow, KeyCompare, false);
		if (iRet == EC_NO_MEM)
		{
			/*这里为了下次完全重新建立T树，把未建立完的树全部删除*/
			m_uiNeedSize = pstRawData->data_size() - pstRawData->data_start();
			destroy_sub_tree();
			return (EC_NO_MEM);
		}
	}

	return (0);
}

int TreeData::decode_tree_row(RowValue &stRow, unsigned char &uchRowFlags, int iDecodeFlag)
{
	m_uiRowOffset = m_uiOffset;

	GET_TREE_VALUE(uchRowFlags, unsigned char);
	for (int j = 1; j <= stRow.num_fields(); j++)
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
				GET_TREE_VALUE(stRow.field_value(j)->s64, int64_t);
			}
			else
			{
				GET_TREE_VALUE(stRow.field_value(j)->s64, int32_t);
			}
			break;

		case DField::Unsigned:
			if (unlikely(stRow.field_size(j) > (int)sizeof(uint32_t)))
			{
				GET_TREE_VALUE(stRow.field_value(j)->u64, uint64_t);
			}
			else
			{
				GET_TREE_VALUE(stRow.field_value(j)->u64, uint32_t);
			}
			break;

		case DField::Float:
			if (likely(stRow.field_size(j) > (int)sizeof(float)))
			{
				GET_TREE_VALUE(stRow.field_value(j)->flt, double);
			}
			else
			{
				GET_TREE_VALUE(stRow.field_value(j)->flt, float);
			}
			break;

		case DField::String:
		case DField::Binary:
		default:
		{
			GET_TREE_VALUE(stRow.field_value(j)->bin.len, int);
			stRow.field_value(j)->bin.ptr = m_pchContent + m_uiOffset;
			SKIP_TREE_SIZE((uint32_t)stRow.field_value(j)->bin.len);
			break;
		}
		} //end of switch
	}
	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "get value error");
	return (-100);
}

int TreeData::compare_tree_data(RowValue *stpNodeRow)
{
	uint32_t rowCnt = m_pstRootData->m_uiNodeCnts;
	if (rowCnt == 0)
	{
		return 1;
	}

	const uint8_t *ufli = m_pstTab->uniq_fields_list();
	for (int i = 0; !m_IndexPartOfUniqField && i < m_pstTab->uniq_fields(); i++)
	{
		if (ufli[i] == TTREE_INDEX_POS)
		{
			m_IndexPartOfUniqField = true;
			break;
		}
	}

	if (m_IndexPartOfUniqField)
	{
		MEM_HANDLE_T *pRecord = NULL;
		RowValue stOldRow(m_pstTab);
		char *indexKey = reinterpret_cast<char *>(stpNodeRow->field_value(TTREE_INDEX_POS));
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		int iRet = m_stTree.Find(indexKey, &cookie, KeyCompare, pRecord);
		if (iRet == -100)
			return iRet;
		if (pRecord != NULL)
		{
			m_pchContent = Pointer<char>(*pRecord);
			uint32_t rows = *(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t));
			m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2;
			_size = _mallocator->chunk_size(*pRecord);

			for (uint32_t j = 0; j < rows; j++)
			{
				stOldRow.default_value();
				unsigned char uchRowFlags;
				if (decode_tree_row(stOldRow, uchRowFlags, 0) != 0)
				{
					return (-2);
				}
				if (stpNodeRow->Compare(stOldRow, m_pstTab->uniq_fields_list(), m_pstTab->uniq_fields()) == 0)
				{
					m_hRecord = *pRecord;
					return 0;
				}
			}
		}
	}
	else
	{
		pResCookie resCookie;
		MEM_HANDLE_T pCookie[rowCnt];
		resCookie.m_handle = pCookie;
		resCookie.nodesNum = 0;
		if (m_stTree.traverse_forward(Visit, &resCookie) != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), " traverse tree-data rows error");
			return (-1);
		}

		RowValue stOldRow(m_pstTab);
		for (uint32_t i = 0; i < resCookie.nodesGot; i++)
		{ //逐行拷贝数据
			m_pchContent = Pointer<char>(pCookie[i]);
			uint32_t rows = *(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t));
			m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2;
			_size = _mallocator->chunk_size(pCookie[i]);

			for (uint32_t j = 0; j < rows; j++)
			{
				stOldRow.default_value();
				unsigned char uchRowFlags;
				if (decode_tree_row(stOldRow, uchRowFlags, 0) != 0)
				{
					return (-2);
				}
				if (stpNodeRow->Compare(stOldRow, m_pstTab->uniq_fields_list(), m_pstTab->uniq_fields()) == 0)
				{
					m_hRecord = pCookie[i];
					return 0;
				}
			}
		}
	}

	return 1;
}

int TreeData::replace_tree_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, unsigned char &RowFlag, bool setrows)
{
	int iRet;
	unsigned int uiTotalRows = 0;
	uint32_t iDelete = 0;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	stpNodeTab = m_pstTab;
	stpTaskTab = stTask.table_definition();
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	m_ullAffectedrows = 0;

	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	if (stpNodeTab == stpTaskTab)
		stpNodeRow = stpTaskRow;

	stNewRow.default_value();
	stTask.update_row(*stpTaskRow);

	if (stpNodeTab != stpTaskTab)
		stpNodeRow->Copy(stpTaskRow);
	else
		stpNodeRow = stpTaskRow;

	iRet = compare_tree_data(stpNodeRow);
	if (iRet < 0)
	{
		snprintf(m_szErr, sizeof(m_szErr), "compare tree data error:%d", iRet);
		return iRet;
	}
	else if (iRet == 0)
	{
		DTCValue new_value = (*stpTaskRow)[TTREE_INDEX_POS];
		char *NewIndex = reinterpret_cast<char *>(&new_value);
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		if (KeyCompare(NewIndex, &cookie, *_mallocator, m_hRecord) != 0) //Index字段变更
		{
			char *tmp_pchContent = m_pchContent;
			uint32_t tmp_size = _size;
			ALLOC_SIZE_T tmp_uiOffset = m_uiOffset;
			iRet = insert_row(*stpTaskRow, KeyCompare, m_async);
			m_pchContent = tmp_pchContent;
			_size = tmp_size;
			m_uiOffset = tmp_uiOffset;

			if (iRet == EC_NO_MEM)
				return iRet;
			else if (iRet == 0)
			{
				m_uiOffset = m_uiRowOffset;
				RowValue stOldRow(m_pstTab);
				stOldRow.default_value();
				unsigned char uchRowFlags;
				if (decode_tree_row(stOldRow, uchRowFlags, 0) != 0)
				{
					return (-2);
				}
				RowFlag = uchRowFlags;
				uiTotalRows = get_row_count();
				m_uiOffset = m_uiRowOffset;
				if (delete_cur_row(stOldRow) == 0)
					iDelete++;

				if (uiTotalRows > 0 && uiTotalRows == iDelete && get_row_count() == 0) //RowFormat上的内容已删光
				{
					//删除tree node
					bool isFreeNode = false;
					DTCValue value = (stOldRow)[TTREE_INDEX_POS]; //for轮询的最后一行数据
					char *indexKey = reinterpret_cast<char *>(&value);
					CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
					int iret = m_stTree.Delete(indexKey, &cookie, KeyCompare, isFreeNode);
					if (iret != 0)
					{
						snprintf(m_szErr, sizeof(m_szErr), "delete stTree failed:%d", iret);
						return -4;
					}
					if (isFreeNode)
						m_pstRootData->m_treeSize -= sizeof(TtreeNode);
					m_pstRootData->m_treeSize -= _size;
					m_pstRootData->m_uiNodeCnts--;
					m_pstRootData->m_hRoot = m_stTree.Root();
					//释放handle
					_mallocator->Free(m_hRecord);
				}
			}
		}
		else //Index字段不变
		{
			MEM_HANDLE_T *pRawHandle = NULL;
			int iRet = Find(TTREE_INDEX_POS, *stpNodeRow, KeyCompare, pRawHandle);
			if (iRet == -100 || iRet == 0)
				return iRet;

			iRet = replace_cur_row(*stpNodeRow, m_async, pRawHandle); // 加进cache
			if (iRet == EC_NO_MEM)
			{
				return iRet;
			}
			if (iRet != 0)
			{
				/*标记加入黑名单*/
				stTask.push_black_list_size(need_size());
				return (-6);
			}
		}
		m_ullAffectedrows = 2;
	}
	return 0;
}

int TreeData::replace_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord)
{
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow, *stpCurRow;

	stpNodeTab = m_pstTab;
	stpTaskTab = stTask.table_definition();
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	RowValue stCurRow(stpNodeTab);

	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	stpCurRow = &stCurRow;
	if (stpNodeTab == stpTaskTab)
		stpNodeRow = stpTaskRow;

	m_pchContent = Pointer<char>(hRecord);
	unsigned int uiTotalRows = get_row_count();
	m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2; //offset DataType + data_size + RowCount
	_size = _mallocator->chunk_size(hRecord);

	unsigned char uchRowFlags;
	uint32_t iDelete = 0;
	uint32_t iInsert = 0;
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		if (decode_tree_row(*stpNodeRow, uchRowFlags, 0) != 0)
			return (-1);

		if (stpNodeTab != stpTaskTab)
			stpTaskRow->Copy(stpNodeRow);

		stpCurRow->Copy(stpNodeRow);

		//如果不符合查询条件
		if (stTask.compare_row(*stpTaskRow) == 0)
			continue;

		MEM_HANDLE_T *pRawHandle = NULL;
		int iRet = Find(TTREE_INDEX_POS, *stpCurRow, KeyCompare, pRawHandle);
		if (iRet == -100 || iRet == 0)
			return iRet;

		stTask.update_row(*stpTaskRow); //修改数据

		if (stpNodeTab != stpTaskTab)
			stpNodeRow->Copy(stpTaskRow);

		if (m_ullAffectedrows == 0)
		{
			iRet = 0;
			DTCValue new_value = (*stpTaskRow)[TTREE_INDEX_POS];
			char *NewIndex = reinterpret_cast<char *>(&new_value);
			CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);

			if (KeyCompare(NewIndex, &cookie, *_mallocator, hRecord) != 0) //update Index字段
			{
				char *tmp_pchContent = m_pchContent;
				uint32_t tmp_size = _size;
				ALLOC_SIZE_T tmp_uiOffset = m_uiOffset;

				iRet = insert_row(*stpTaskRow, KeyCompare, m_async);

				m_pchContent = tmp_pchContent;
				_size = tmp_size;
				m_uiOffset = tmp_uiOffset;
				if (iRet == EC_NO_MEM)
				{
					return iRet;
				}
				else if (iRet == 0)
				{
					iInsert++;
					m_uiOffset = m_uiRowOffset;
					if (delete_cur_row(*stpCurRow) == 0)
						iDelete++;
				}
			}
			else
			{
				iRet = replace_cur_row(*stpNodeRow, m_async, pRawHandle); // 加进cache
				if (iRet == EC_NO_MEM)
				{
					return iRet;
				}
				if (iRet != 0)
				{
					/*标记加入黑名单*/
					stTask.push_black_list_size(need_size());
					return (-6);
				}
			}

			m_ullAffectedrows += 2;
		}
		else
		{
			if (delete_cur_row(*stpCurRow) == 0)
			{
				iDelete++;
				m_ullAffectedrows++;
			}
		}
	}

	if (uiTotalRows > 0 && uiTotalRows - iDelete == 0) //RowFormat上的内容已删光
	{
		//删除tree node
		bool isFreeNode = false;
		DTCValue value = (*stpCurRow)[TTREE_INDEX_POS]; //for轮询的最后一行数据
		char *indexKey = reinterpret_cast<char *>(&value);
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		int iret = m_stTree.Delete(indexKey, &cookie, KeyCompare, isFreeNode);
		if (iret != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "delete stTree failed:%d", iret);
			return -4;
		}
		if (isFreeNode)
			m_pstRootData->m_treeSize -= sizeof(TtreeNode);
		m_pstRootData->m_treeSize -= _size;
		m_pstRootData->m_uiNodeCnts--;
		m_pstRootData->m_hRoot = m_stTree.Root();
		//释放handle
		_mallocator->Free(hRecord);
	}

	return 0;
}

/*
 * encode到私有内存，防止replace，update引起重新rellocate导致value引用了过期指针
 */
int TreeData::encode_to_private_area(RawData &raw, RowValue &value, unsigned char value_flag)
{
	int ret = raw.Init(Key(), raw.calc_row_size(value, m_pstTab->key_fields() - 1));
	if (0 != ret)
	{
		log_error("init raw-data struct error, ret=%d, err=%s", ret, raw.get_err_msg());
		return -1;
	}

	ret = raw.insert_row(value, false, false);
	if (0 != ret)
	{
		log_error("insert row to raw-data error: ret=%d, err=%s", ret, raw.get_err_msg());
		return -2;
	}

	raw.rewind();

	ret = raw.decode_row(value, value_flag, 0);
	if (0 != ret)
	{
		log_error("decode raw-data to row error: ret=%d, err=%s", ret, raw.get_err_msg());
		return -3;
	}

	return 0;
}

int TreeData::update_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord)
{
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow, *stpCurRow;

	stpNodeTab = m_pstTab;
	stpTaskTab = stTask.table_definition();
	RowValue stNewRow(stpTaskTab);
	RowValue stNewNodeRow(stpNodeTab);
	RowValue stCurRow(stpNodeTab);

	stpTaskRow = &stNewRow;
	stpNodeRow = &stNewNodeRow;
	stpCurRow = &stCurRow;
	if (stpNodeTab == stpTaskTab)
		stpNodeRow = stpTaskRow;

	m_pchContent = Pointer<char>(hRecord);
	unsigned int uiTotalRows = get_row_count();
	m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2; //offset DataType + data_size + RowCount
	_size = _mallocator->chunk_size(hRecord);

	unsigned char uchRowFlags;
	uint32_t iDelete = 0;
	uint32_t iInsert = 0;
	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		if (decode_tree_row(*stpNodeRow, uchRowFlags, 0) != 0)
			return (-1);

		if (stpNodeTab != stpTaskTab)
			stpTaskRow->Copy(stpNodeRow);

		stpCurRow->Copy(stpNodeRow);

		//如果不符合查询条件
		if (stTask.compare_row(*stpTaskRow) == 0)
			continue;

		MEM_HANDLE_T *pRawHandle = NULL;
		int iRet = Find(TTREE_INDEX_POS, *stpCurRow, KeyCompare, pRawHandle);
		if (iRet == -100 || iRet == 0)
			return iRet;

		stTask.update_row(*stpTaskRow); //修改数据

		if (stpNodeTab != stpTaskTab)
			stpNodeRow->Copy(stpTaskRow);

		iRet = 0;
		DTCValue new_value = (*stpTaskRow)[TTREE_INDEX_POS];
		char *NewIndex = reinterpret_cast<char *>(&new_value);
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);

		if (KeyCompare(NewIndex, &cookie, *_mallocator, hRecord) != 0) //update Index字段
		{
			char *tmp_pchContent = m_pchContent;
			uint32_t tmp_size = _size;
			ALLOC_SIZE_T tmp_uiOffset = m_uiOffset;

			iRet = insert_row(*stpTaskRow, KeyCompare, m_async);

			m_pchContent = tmp_pchContent;
			_size = tmp_size;
			m_uiOffset = tmp_uiOffset;
			if (iRet == EC_NO_MEM)
			{
				return iRet;
			}
			else if (iRet == 0)
			{
				iInsert++;
				m_uiOffset = m_uiRowOffset;
				if (delete_cur_row(*stpCurRow) == 0)
					iDelete++;
			}
		}
		else
		{
			// 在私有区间decode
			RawData stTmpRows(&g_stSysMalloc, 1);
			if (encode_to_private_area(stTmpRows, *stpNodeRow, uchRowFlags))
			{
				log_error("encode rowvalue to private rawdata area failed");
				return -3;
			}

			iRet = replace_cur_row(*stpNodeRow, m_async, pRawHandle); // 加进cache
			if (iRet == EC_NO_MEM)
			{
				return iRet;
			}
			if (iRet != 0)
			{
				/*标记加入黑名单*/
				stTask.push_black_list_size(need_size());
				return (-6);
			}
		}

		m_ullAffectedrows++;
		if (uchRowFlags & OPER_DIRTY)
			m_llDirtyRowsInc--;
		if (m_async)
			m_llDirtyRowsInc++;
	}

	if (uiTotalRows > 0 && uiTotalRows - iDelete == 0) //RowFormat上的内容已删光
	{
		//删除tree node
		bool isFreeNode = false;
		DTCValue value = (*stpCurRow)[TTREE_INDEX_POS]; //for轮询的最后一行数据
		char *indexKey = reinterpret_cast<char *>(&value);
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		int iret = m_stTree.Delete(indexKey, &cookie, KeyCompare, isFreeNode);
		if (iret != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "delete stTree failed:%d", iret);
			return -4;
		}
		if (isFreeNode)
			m_pstRootData->m_treeSize -= sizeof(TtreeNode);
		m_pstRootData->m_treeSize -= _size;
		m_pstRootData->m_uiNodeCnts--;
		m_pstRootData->m_hRoot = m_stTree.Root();
		//释放handle
		_mallocator->Free(hRecord);
	}

	if (iInsert != iDelete)
	{
		snprintf(m_szErr, sizeof(m_szErr), "update index change error: insert:%d, delete:%d", iInsert, iDelete);
		return (-10);
	}

	return 0;
}

int TreeData::delete_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord)
{
	int iRet;
	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;

	stpNodeTab = m_pstTab;
	stpTaskTab = stTask.table_definition();
	RowValue stNodeRow(stpNodeTab);
	RowValue stTaskRow(stpTaskTab);
	if (stpNodeTab == stpTaskTab)
	{
		stpNodeRow = &stTaskRow;
		stpTaskRow = &stTaskRow;
	}
	else
	{
		stpNodeRow = &stNodeRow;
		stpTaskRow = &stTaskRow;
	}

	unsigned int iAffectRows = 0;
	unsigned char uchRowFlags;

	m_pchContent = Pointer<char>(hRecord);
	unsigned int uiTotalRows = get_row_count();
	m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2; //offset DataType + data_size + RowCount
	_size = _mallocator->chunk_size(hRecord);

	for (unsigned int i = 0; i < uiTotalRows; i++)
	{
		if ((decode_tree_row(*stpNodeRow, uchRowFlags, 0)) != 0)
		{
			return (-2);
		}
		if (stpNodeTab != stpTaskTab)
		{
			stpTaskRow->Copy(stpNodeRow);
		}
		if (stTask.compare_row(*stpTaskRow) != 0)
		{ //符合del条件
			iRet = delete_cur_row(*stpNodeRow);
			if (iRet != 0)
			{
				log_error("tree-data delete row error: %d", iRet);
				return (-5);
			}
			iAffectRows++;
			m_llRowsInc--;
			if (uchRowFlags & OPER_DIRTY)
				m_llDirtyRowsInc--;
		}
	}

	if (iAffectRows > uiTotalRows)
		return (-3);
	else if (iAffectRows == uiTotalRows && uiTotalRows > 0) //RowFormat上的内容已删光
	{
		//删除tree node
		bool isFreeNode = false;
		DTCValue value = (*stpNodeRow)[TTREE_INDEX_POS]; //for轮询的最后一行数据
		char *indexKey = reinterpret_cast<char *>(&value);
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		int iret = m_stTree.Delete(indexKey, &cookie, KeyCompare, isFreeNode);
		if (iret != 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "delete stTree failed:%d\t%s", iret, m_stTree.get_err_msg());
			return -4;
		}
		if (isFreeNode)
			m_pstRootData->m_treeSize -= sizeof(TtreeNode);
		m_pstRootData->m_treeSize -= _size;
		m_pstRootData->m_uiNodeCnts--;
		m_pstRootData->m_hRoot = m_stTree.Root();
		//释放handle
		_mallocator->Free(hRecord);
	}

	return (0);
}

int TreeData::skip_row(const RowValue &stRow)
{
	if (m_pchContent == NULL)
	{
		snprintf(m_szErr, sizeof(m_szErr), "rawdata not init yet");
		return (-1);
	}

	m_uiOffset = m_uiRowOffset;
	if (m_uiOffset >= get_data_size())
	{
		snprintf(m_szErr, sizeof(m_szErr), "already at end of data");
		return (-2);
	}

	SKIP_TREE_SIZE(sizeof(unsigned char)); // flag

	for (int j = m_uchKeyIdx + 1; j <= stRow.num_fields(); j++) //拷贝一行数据
	{
		//id: bug fix skip discard
		if (stRow.table_definition()->is_discard(j))
			continue;
		int temp = 0;
		switch (stRow.field_type(j))
		{
		case DField::Unsigned:
		case DField::Signed:
			GET_TREE_VALUE_AT_OFFSET(temp, int, m_uiOffset);

			if (stRow.field_size(j) > (int)sizeof(int32_t))
				SKIP_TREE_SIZE(sizeof(int64_t));
			else
				SKIP_TREE_SIZE(sizeof(int32_t));
			;
			break;

		case DField::Float: //浮点数
			if (stRow.field_size(j) > (int)sizeof(float))
				SKIP_TREE_SIZE(sizeof(double));
			else
				SKIP_TREE_SIZE(sizeof(float));
			break;

		case DField::String: //字符串
		case DField::Binary: //二进制数据
		default:
		{
			int iLen;
			GET_TREE_VALUE(iLen, int);
			SKIP_TREE_SIZE(iLen);
			break;
		}
		} //end of switch
	}

	return (0);

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "skip row error");
	return (-100);
}

int TreeData::replace_cur_row(const RowValue &stRow, bool isDirty, MEM_HANDLE_T *hRecord)
{
	int iRet = 0;
	ALLOC_SIZE_T uiOldOffset;
	ALLOC_SIZE_T uiNextRowsSize;
	ALLOC_SIZE_T uiNewRowSize = 0;
	ALLOC_SIZE_T uiCurRowSize = 0;
	ALLOC_SIZE_T uiNextRowsOffset;
	ALLOC_SIZE_T uiDataSize = get_data_size();

	uiOldOffset = m_uiOffset;
	if ((iRet = skip_row(stRow)) != 0)
	{
		goto ERROR_RET;
	}

	unsigned char uchRowFlag;
	GET_TREE_VALUE_AT_OFFSET(uchRowFlag, unsigned char, m_uiRowOffset);
	if (isDirty)
		uchRowFlag = OPER_UPDATE;

	uiNewRowSize = calc_tree_row_size(stRow, m_uchKeyIdx);
	uiCurRowSize = m_uiOffset - m_uiRowOffset;
	uiNextRowsOffset = m_uiOffset;
	uiNextRowsSize = uiDataSize - m_uiOffset;

	if (uiNewRowSize > uiCurRowSize)
	{
		// enlarge buffer
		MEM_HANDLE_T hTmp = _mallocator->ReAlloc(*hRecord, uiDataSize + uiNewRowSize - uiCurRowSize);
		if (hTmp == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "realloc error");
			m_uiNeedSize = uiDataSize + uiNewRowSize - uiCurRowSize;
			iRet = EC_NO_MEM;
			goto ERROR_RET;
		}
		m_pstRootData->m_treeSize -= _size;
		*hRecord = hTmp;
		_size = _mallocator->chunk_size(*hRecord);
		m_pstRootData->m_treeSize += _size;
		m_pchContent = Pointer<char>(*hRecord);

		// move data
		if (uiNextRowsSize > 0)
			memmove(m_pchContent + uiNextRowsOffset + (uiNewRowSize - uiCurRowSize), m_pchContent + uiNextRowsOffset, uiNextRowsSize);

		// copy new row
		m_uiOffset = m_uiRowOffset;
		iRet = encode_tree_row(stRow, uchRowFlag);
		if (iRet != 0)
		{
			if (uiNextRowsSize > 0)
				memmove(m_pchContent + uiNextRowsOffset, m_pchContent + uiNextRowsOffset + (uiNewRowSize - uiCurRowSize), uiNextRowsSize);
			iRet = -1;
			goto ERROR_RET;
		}
	}
	else
	{
		// back up old row
		void *pTmpBuf = MALLOC(uiCurRowSize);
		if (pTmpBuf == NULL)
		{
			m_uiNeedSize = uiCurRowSize;
			snprintf(m_szErr, sizeof(m_szErr), "malloc error: %m");
			return (-ENOMEM);
		}
		memmove(pTmpBuf, m_pchContent + m_uiRowOffset, uiCurRowSize);

		// copy new row
		m_uiOffset = m_uiRowOffset;
		iRet = encode_tree_row(stRow, uchRowFlag);
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
		MEM_HANDLE_T hTmp = _mallocator->ReAlloc(*hRecord, uiDataSize + uiNewRowSize - uiCurRowSize);
		if (hTmp != INVALID_HANDLE)
		{
			m_pstRootData->m_treeSize -= _size;
			*hRecord = hTmp;
			_size = _mallocator->chunk_size(*hRecord);
			m_pstRootData->m_treeSize += _size;
			m_pchContent = Pointer<char>(*hRecord);
		}
	}
	set_data_size(uiDataSize - uiCurRowSize + uiNewRowSize);
	m_pstRootData->m_uiTotalRawSize += (uiNewRowSize - uiCurRowSize);

ERROR_RET:
	m_uiOffset = uiOldOffset + uiNewRowSize - uiCurRowSize;
	return (iRet);
}

int TreeData::delete_cur_row(const RowValue &stRow)
{
	int iRet = 0;
	ALLOC_SIZE_T uiOldOffset;
	ALLOC_SIZE_T uiNextRowsSize;

	uiOldOffset = m_uiOffset;
	if ((iRet = skip_row(stRow)) != 0)
	{
		log_error("skip error: %d,%s", iRet, get_err_msg());
		goto ERROR_RET;
	}
	uiNextRowsSize = get_data_size() - m_uiOffset;

	memmove(m_pchContent + m_uiRowOffset, m_pchContent + m_uiOffset, uiNextRowsSize);
	set_row_count(get_row_count() - 1);
	set_data_size(get_data_size() - (m_uiOffset - m_uiRowOffset));

	m_pstRootData->m_uiRowCnt--;
	m_pstRootData->m_uiTotalRawSize -= (m_uiOffset - m_uiRowOffset);

	m_uiOffset = m_uiRowOffset;
	return (iRet);

ERROR_RET:
	m_uiOffset = uiOldOffset;
	return (iRet);
}

int TreeData::get_sub_raw_data(TaskRequest &stTask, MEM_HANDLE_T hRecord)
{
	int laid = stTask.flag_no_cache() ? -1 : stTask.table_definition()->lastacc_field_id();

	if (stTask.result_full())
		return 0;

	DTCTableDefinition *stpNodeTab, *stpTaskTab;
	RowValue *stpNodeRow, *stpTaskRow;
	stpNodeTab = m_pstTab;
	stpTaskTab = stTask.table_definition();
	RowValue stNodeRow(stpNodeTab);
	RowValue stTaskRow(stpTaskTab);
	if (stpNodeTab == stpTaskTab)
	{
		stpNodeRow = &stTaskRow;
		stpTaskRow = &stTaskRow;
	}
	else
	{
		stpNodeRow = &stNodeRow;
		stpTaskRow = &stTaskRow;
	}

	m_pchContent = Pointer<char>(hRecord);
	uint32_t rows = get_row_count();
	m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2;
	_size = _mallocator->chunk_size(hRecord);

	unsigned char uchRowFlags;
	for (unsigned int j = 0; j < rows; j++)
	{
		stTask.update_key(*stpNodeRow); // use stpNodeRow is fine, as just modify key field
		if ((decode_tree_row(*stpNodeRow, uchRowFlags, 0)) != 0)
		{
			return (-2);
		}
		// this pointer compare is ok, as these two is both come from tabledefmanager. if they mean same, they are same object.
		if (stpNodeTab != stpTaskTab)
		{
			stpTaskRow->Copy(stpNodeRow);
		}
		if (stTask.compare_row(*stpTaskRow) == 0) //如果不符合查询条件
			continue;

		if (stpTaskTab->expire_time_field_id() > 0)
			stpTaskRow->update_expire_time();
		//当前行添加到task中
		stTask.append_row(stpTaskRow);

		if (stTask.all_rows() && stTask.result_full())
		{
			stTask.set_total_rows((int)rows);
			break;
		}
	}
	return 0;
}

int TreeData::get_sub_raw(TaskRequest &stTask, unsigned int nodeCnt, bool isAsc, SubRowProcess subRowProc)
{
	pResCookie resCookie;
	MEM_HANDLE_T pCookie[nodeCnt];
	resCookie.m_handle = pCookie;

	if (stTask.all_rows() && stTask.requestInfo.limit_count() > 0) //condition: ONLY `LIMIT` without `WHERE`
		resCookie.nodesNum = stTask.requestInfo.limit_start() + stTask.requestInfo.limit_count();
	else
		resCookie.nodesNum = 0;

	m_stTree.traverse_forward(Visit, &resCookie);

	if (isAsc) //升序
	{
		for (int i = 0; i < (int)resCookie.nodesGot; i++)
		{
			int iRet = (this->*subRowProc)(stTask, pCookie[i]);
			if (iRet != 0)
				return iRet;
		}
	}
	else //降序
	{
		for (int i = (int)resCookie.nodesGot - 1; i >= 0; i--)
		{
			int iRet = (this->*subRowProc)(stTask, pCookie[i]);
			if (iRet != 0)
				return iRet;
		}
	}

	return 0;
}

int TreeData::match_index_condition(TaskRequest &stTask, unsigned int NodeCnt, SubRowProcess subRowProc)
{
	const DTCFieldValue *condition = stTask.request_condition();
	int numfields = 0; //条件字段个数
	bool isAsc = !(m_pstTab->is_desc_order(TTREE_INDEX_POS));

	if (condition)
		numfields = condition->num_fields();

	int indexIdArr[numfields]; //开辟空间比实际使用的大
	int indexCount = 0;		   //条件索引个数
	int firstEQIndex = -1;	   //第一个EQ在indexIdArr中的位置

	for (int i = 0; i < numfields; i++)
	{
		if (condition->field_id(i) == TTREE_INDEX_POS)
		{
			if (firstEQIndex == -1 && condition->field_operation(i) == DField::EQ)
				firstEQIndex = i;
			indexIdArr[indexCount++] = i;
		}
	}

	if (indexCount == 0 || (indexCount == 1 && condition->field_operation(indexIdArr[0]) == DField::NE))
	{ //平板类型
		int iret = get_sub_raw(stTask, NodeCnt, isAsc, subRowProc);
		if (iret != 0)
			return iret;
	}
	else if (firstEQIndex != -1) //有至少一个EQ条件
	{
		MEM_HANDLE_T *pRecord = NULL;

		char *indexKey = reinterpret_cast<char *>(condition->field_value(firstEQIndex));
		CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);
		int iRet = m_stTree.Find(indexKey, &cookie, KeyCompare, pRecord);
		if (iRet == -100)
			return iRet;
		if (pRecord != NULL)
		{
			iRet = (this->*subRowProc)(stTask, *pRecord);
			if (iRet != 0)
				return iRet;
		}
	}
	else
	{
		int leftId = -1;
		int rightId = -1;

		for (int i = 0; i < indexCount; i++)
		{
			switch (condition->field_operation(indexIdArr[i]))
			{
			case DField::LT:
			case DField::LE:
				if (rightId == -1)
					rightId = indexIdArr[i];
				break;

			case DField::GT:
			case DField::GE:
				if (leftId == -1)
					leftId = indexIdArr[i];
				break;

			default:
				break;
			}
		}

		if (leftId != -1 && rightId == -1) //GE
		{
			pResCookie resCookie;
			MEM_HANDLE_T pCookie[NodeCnt];
			resCookie.m_handle = pCookie;
			resCookie.nodesNum = 0;
			char *indexKey = reinterpret_cast<char *>(condition->field_value(leftId));
			CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);

			if (m_stTree.traverse_forward(indexKey, &cookie, KeyCompare, Visit, &resCookie) != 0)
			{
				snprintf(m_szErr, sizeof(m_szErr), " traverse tree-data rows error");
				return (-1);
			}

			if (isAsc)
			{
				for (int i = 0; i < (int)resCookie.nodesGot; i++)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
			else
			{
				for (int i = (int)resCookie.nodesGot - 1; i >= 0; i--)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
		}
		else if (leftId == -1 && rightId != -1) //LE
		{
			pResCookie resCookie;
			MEM_HANDLE_T pCookie[NodeCnt];
			resCookie.m_handle = pCookie;
			resCookie.nodesNum = NodeCnt;
			char *indexKey = reinterpret_cast<char *>(condition->field_value(rightId));
			CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);

			if (m_stTree.traverse_backward(indexKey, &cookie, KeyCompare, Visit, &resCookie) != 0)
			{
				snprintf(m_szErr, sizeof(m_szErr), " traverse tree-data rows error");
				return (-1);
			}

			if (isAsc)
			{
				for (int i = (int)resCookie.nodesGot - 1; i >= 0; i--)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
			else
			{
				for (int i = 0; i < (int)resCookie.nodesGot; i++)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
		}
		else if (leftId != -1 && rightId != -1) //range
		{
			pResCookie resCookie;
			MEM_HANDLE_T pCookie[NodeCnt];
			resCookie.m_handle = pCookie;
			resCookie.nodesNum = 0;
			char *beginKey = reinterpret_cast<char *>(condition->field_value(leftId));
			char *endKey = reinterpret_cast<char *>(condition->field_value(rightId));
			CmpCookie cookie(m_pstTab, TTREE_INDEX_POS);

			if (m_stTree.traverse_forward(beginKey, endKey, &cookie, KeyCompare, Visit, &resCookie) != 0)
			{
				snprintf(m_szErr, sizeof(m_szErr), " traverse tree-data rows error");
				return (-1);
			}

			if (isAsc)
			{
				for (int i = 0; i < (int)resCookie.nodesGot; i++)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
			else
			{
				for (int i = (int)resCookie.nodesGot - 1; i >= 0; i--)
				{
					int iRet = (this->*subRowProc)(stTask, pCookie[i]);
					if (iRet != 0)
						return iRet;
				}
			}
		}
		else //may all NE, raw data process
		{
			int iret = get_sub_raw(stTask, NodeCnt, isAsc, subRowProc);
			if (iret != 0)
				return iret;
		}
	}

	return 0;
}

int TreeData::dirty_rows_in_node()
{
	unsigned int uiTotalNodes = m_pstRootData->m_uiNodeCnts;
	int dirty_rows = 0;
	pResCookie resCookie;
	MEM_HANDLE_T pCookie[uiTotalNodes];
	resCookie.m_handle = pCookie;
	resCookie.nodesNum = 0;

	RowValue stRow(m_pstTab);

	m_stTree.traverse_forward(Visit, &resCookie);

	for (int i = 0; i < (int)resCookie.nodesGot; i++)
	{
		m_pchContent = Pointer<char>(pCookie[i]);
		uint32_t rows = get_row_count();
		m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2;
		_size = _mallocator->chunk_size(pCookie[i]);

		unsigned char uchRowFlags;
		for (unsigned int j = 0; j < rows; j++)
		{
			if (decode_tree_row(stRow, uchRowFlags, 0) != 0)
			{
				log_error("subraw-data decode row error: %s", get_err_msg());
				return (-1);
			}

			if (uchRowFlags & OPER_DIRTY)
				dirty_rows++;
		}
	}

	return dirty_rows;
}

int TreeData::flush_tree_data(DTCFlushRequest *pstFlushReq, Node *pstNode, unsigned int &uiFlushRowsCnt)
{
	unsigned int uiTotalNodes = m_pstRootData->m_uiNodeCnts;

	uiFlushRowsCnt = 0;
	DTCValue astKey[m_pstTab->key_fields()];
	TaskPackedKey::unpack_key(m_pstTab, Key(), astKey);
	RowValue stRow(m_pstTab); //一行数据
	for (int i = 0; i < m_pstTab->key_fields(); i++)
		stRow[i] = astKey[i];

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	pResCookie resCookie;
	MEM_HANDLE_T pCookie[uiTotalNodes];
	resCookie.m_handle = pCookie;
	resCookie.nodesNum = 0;

	m_stTree.traverse_forward(Visit, &resCookie);

	for (int i = 0; i < (int)resCookie.nodesGot; i++)
	{
		m_pchContent = Pointer<char>(pCookie[i]);
		uint32_t rows = get_row_count();
		m_uiOffset = sizeof(unsigned char) + sizeof(uint32_t) * 2;
		_size = _mallocator->chunk_size(pCookie[i]);

		unsigned char uchRowFlags;
		for (unsigned int j = 0; j < rows; j++)
		{
			if (decode_tree_row(stRow, uchRowFlags, 0) != 0)
			{
				log_error("subraw-data decode row error: %s", get_err_msg());
				return (-1);
			}

			if ((uchRowFlags & OPER_DIRTY) == false)
				continue;

			if (pstFlushReq && pstFlushReq->flush_row(stRow) != 0)
			{
				log_error("flush_data() invoke flushRow() failed.");
				return (-2);
			}
			set_cur_row_flag(uchRowFlags & ~OPER_DIRTY);
			m_llDirtyRowsInc--;
			uiFlushRowsCnt++;
		}
	}

	return 0;
}

int TreeData::get_tree_data(TaskRequest &stTask)
{
	uint32_t rowCnt = m_pstRootData->m_uiRowCnt;
	if (rowCnt == 0)
	{
		return 0;
	}

	stTask.prepare_result(); //准备返回结果对象
	if (stTask.all_rows() && (stTask.count_only() || !stTask.in_range((int)rowCnt, 0)))
	{
		if (stTask.is_batch_request())
		{
			if ((int)rowCnt > 0)
				stTask.add_total_rows((int)rowCnt);
		}
		else
		{
			stTask.set_total_rows((int)rowCnt);
		}
	}
	else
	{
		int iret = match_index_condition(stTask, m_pstRootData->m_uiNodeCnts, &TreeData::get_sub_raw_data);
		if (iret != 0)
			return iret;
	}

	return 0;
}

int TreeData::update_tree_data(TaskRequest &stTask, Node *pstNode, RawData *pstAffectedRows, bool async, bool setrows)
{
	uint32_t rowCnt = m_pstRootData->m_uiNodeCnts;
	if (rowCnt == 0)
	{
		return 0;
	}

	m_pstNode = pstNode;
	m_async = async;
	m_llDirtyRowsInc = 0;

	return match_index_condition(stTask, rowCnt, &TreeData::update_sub_raw_data);
}

int TreeData::delete_tree_data(TaskRequest &stTask)
{
	uint32_t rowCnt = m_pstRootData->m_uiNodeCnts;
	if (rowCnt == 0)
	{
		return 0;
	}

	m_llRowsInc = 0;
	m_llDirtyRowsInc = 0;

	stTask.prepare_result(); //准备返回结果对象
	if (stTask.all_rows() && (stTask.count_only() || !stTask.in_range((int)rowCnt, 0)))
	{
		if (stTask.is_batch_request())
		{
			if ((int)rowCnt > 0)
				stTask.add_total_rows((int)rowCnt);
		}
		else
		{
			stTask.set_total_rows((int)rowCnt);
		}
	}
	else
	{
		int iret = match_index_condition(stTask, rowCnt, &TreeData::delete_sub_raw_data);
		if (iret != 0)
			return iret;
	}

	return 0;
}

int TreeData::get_expire_time(DTCTableDefinition *t, uint32_t &expire)
{
	expire = 0;
	if (unlikely(_handle == INVALID_HANDLE))
	{
		snprintf(m_szErr, sizeof(m_szErr), "root tree data not init yet");
		return (-1);
	}
	if (m_iExpireId == -1)
	{
		expire = 0;
		return 0;
	}

	MEM_HANDLE_T root = get_tree_root();
	if (unlikely(root == INVALID_HANDLE))
	{
		snprintf(m_szErr, sizeof(m_szErr), "root tree data not init yet");
		return (-1);
	}

	MEM_HANDLE_T firstHanle = m_stTree.first_node();
	if (unlikely(firstHanle == INVALID_HANDLE))
	{
		snprintf(m_szErr, sizeof(m_szErr), "root tree data not init yet");
		return (-1);
	}

	m_uiOffset = 0;
	_size = _mallocator->chunk_size(firstHanle);
	m_pchContent = Pointer<char>(firstHanle);

	SKIP_TREE_SIZE(sizeof(unsigned char));

	for (int j = m_uchKeyIdx + 1; j <= m_pstTab->num_fields(); j++)
	{
		if (j == m_iExpireId)
		{
			expire = *((uint32_t *)(m_pchContent + m_uiOffset));
			break;
		}

		switch (m_pstTab->field_type(j))
		{
		case DField::Unsigned:
		case DField::Signed:
			if (m_pstTab->field_size(j) > (int)sizeof(int32_t))
				SKIP_TREE_SIZE(sizeof(int64_t));
			else
				SKIP_TREE_SIZE(sizeof(int32_t));
			;
			break;

		case DField::Float:
			if (m_pstTab->field_size(j) > (int)sizeof(float))
				SKIP_TREE_SIZE(sizeof(double));
			else
				SKIP_TREE_SIZE(sizeof(float));
			break;

		case DField::String:
		case DField::Binary:
		default:
			uint32_t iLen = 0;
			GET_TREE_VALUE(iLen, int);
			SKIP_TREE_SIZE(iLen);
			break;
		} //end of switch
	}
	return 0;

	m_uiOffset = 0;
	_size = 0;
	m_pchContent = NULL;

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "get expire error");
	return (-100);
}

ALLOC_SIZE_T TreeData::calc_tree_row_size(const RowValue &stRow, int keyIdx)
{
	if (keyIdx == -1)
		log_error("TreeData may not init yet...");
	ALLOC_SIZE_T tSize = 1;								  // flag
	for (int j = keyIdx + 1; j <= stRow.num_fields(); j++) //¿½±´Ò»ÐÐÊý¾Ý
	{
		if (stRow.table_definition()->is_discard(j))
			continue;
		switch (stRow.field_type(j))
		{
		case DField::Signed:
		case DField::Unsigned:
			tSize += unlikely(stRow.field_size(j) > (int)sizeof(int32_t)) ? sizeof(int64_t) : sizeof(int32_t);
			break;

		case DField::Float: //¸¡µãÊý
			tSize += likely(stRow.field_size(j) > (int)sizeof(float)) ? sizeof(double) : sizeof(float);
			break;

		case DField::String: //×Ö·û´®
		case DField::Binary: //¶þ½øÖÆÊý¾Ý
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

int TreeData::destroy_sub_tree()
{
	m_stTree.Destroy();
	m_pstRootData->m_uiRowCnt = 0;
	m_pstRootData->m_hRoot = INVALID_HANDLE;
	m_pstRootData->m_treeSize = 0;
	m_pstRootData->m_uiTotalRawSize = 0;
	m_pstRootData->m_uiNodeCnts = 0;
	return 0;
}

unsigned int TreeData::get_row_count()
{
	return *(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t));
}

unsigned int TreeData::get_data_size()
{
	return *(uint32_t *)(m_pchContent + sizeof(unsigned char));
}

int TreeData::set_row_count(unsigned int count)
{
	SET_TREE_VALUE_AT_OFFSET(count, uint32_t, sizeof(unsigned char) + sizeof(uint32_t));

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "set data rowcount error");
	return (-100);
}

int TreeData::set_data_size(unsigned int data_size)
{
	SET_TREE_VALUE_AT_OFFSET(data_size, uint32_t, sizeof(unsigned char));

ERROR_RET:
	snprintf(m_szErr, sizeof(m_szErr), "set data size error");
	return (-100);
}

int TreeData::set_cur_row_flag(unsigned char uchFlag)
{
	if (m_uiRowOffset >= get_data_size())
	{
		snprintf(m_szErr, sizeof(m_szErr), "no more rows");
		return (-1);
	}
	*(unsigned char *)(m_pchContent + m_uiRowOffset) = uchFlag;

	return (0);
}