/*
 * =====================================================================================
 *
 *       Filename:  t_tree.cc
 *
 *    Description:  T-tree fundamental operation. only for TreeData invoke.
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
#include <assert.h>
#include "log.h"
#include "t_tree.h"
#include "value.h"
#include "data_chunk.h"

/*#ifndef MODU_TEST
#include "tree_data.h"
#endif*/

#define GET_KEY(x, u, t)            \
	do                              \
	{                               \
		x = (typeof(x)) * (t *)(u); \
	} while (0)

int64_t KeyCompare(const char *pchKey, void *pCmpCookie, Mallocator &stMalloc, ALLOC_HANDLE_T hOtherKey)
{
	const char *pOtherKey = reinterpret_cast<char *>(stMalloc.handle_to_ptr(hOtherKey));
	pOtherKey = pOtherKey + sizeof(unsigned char) * 2 + 2 * sizeof(uint32_t);

	CmpCookie *cookie = reinterpret_cast<CmpCookie *>(pCmpCookie);
	const DTCTableDefinition *t_pstTab = cookie->m_pstTab;
	const int idx = cookie->m_uchIdx;
	int fieldType = t_pstTab->field_type(idx);

	char *v = const_cast<char *>(pchKey);
	DTCValue *value = reinterpret_cast<DTCValue *>(v);

	switch (fieldType)
	{
	case DField::Signed:
		int64_t skey, sotherKey;
		skey = value->s64;
		if (unlikely(t_pstTab->field_size(idx) > (int)sizeof(int32_t)))
		{
			GET_KEY(sotherKey, pOtherKey, int64_t);
		}
		else
		{
			GET_KEY(sotherKey, pOtherKey, int32_t);
		}
		return skey - sotherKey;

	case DField::Unsigned:
		uint64_t ukey, uotherKey;
		ukey = value->u64;
		if (unlikely(t_pstTab->field_size(idx) > (int)sizeof(uint32_t)))
		{
			GET_KEY(uotherKey, pOtherKey, uint64_t);
		}
		else
		{
			GET_KEY(uotherKey, pOtherKey, uint32_t);
		}
		return ukey - uotherKey;

	case DField::Float:
		double dkey, dotherKey, sKey;
		dkey = value->flt;
		if (likely(t_pstTab->field_size(idx) > (int)sizeof(float)))
		{
			GET_KEY(dotherKey, pOtherKey, double);
		}
		else
		{
			GET_KEY(dotherKey, pOtherKey, float);
		}
		sKey = dkey - dotherKey;
		if (sKey > -0.0001 && sKey < 0.0001)
			return 0;
		return sKey;

	case DField::String:
	{
		int keyLen = 0, tKeyLen = 0;
		char *key = NULL;
		if (DField::String == fieldType)
		{
			keyLen = value->str.len;
			key = value->str.ptr;
		}
		else if (DField::Binary == fieldType)
		{
			keyLen = value->bin.len;
			key = value->bin.ptr;
		}
		else
			keyLen = 0;

		GET_KEY(tKeyLen, pOtherKey, int);
		if (keyLen == 0 && tKeyLen == 0)
		{
			return 0;
		}
		else if (keyLen == 0 && tKeyLen != 0)
		{
			return -1;
		}
		else if (keyLen != 0 && tKeyLen == 0)
		{
			return 1;
		}
		else if (keyLen != 0 && tKeyLen != 0)
		{
			pOtherKey = pOtherKey + sizeof(int);
			int len = keyLen < tKeyLen ? keyLen : tKeyLen;
			int res = strncasecmp(key, pOtherKey, len);
			if (keyLen == tKeyLen)
				return res;
			else if (res == 0)
			{
				return keyLen > tKeyLen ? 1 : -1;
			}
			else
			{
				return res;
			}
		}
		return 0;
	}
	case DField::Binary:
	{
		int keyLen = 0, tKeyLen = 0;
		char *key = NULL;
		if (DField::String == fieldType)
		{
			keyLen = value->str.len;
			key = value->str.ptr;
		}
		else if (DField::Binary == fieldType)
		{
			keyLen = value->bin.len;
			key = value->bin.ptr;
		}
		else
			keyLen = 0;

		GET_KEY(tKeyLen, pOtherKey, int);
		if (keyLen == 0 && tKeyLen == 0)
		{
			return 0;
		}
		else if (keyLen == 0 && tKeyLen != 0)
		{
			return -1;
		}
		else if (keyLen != 0 && tKeyLen == 0)
		{
			return 1;
		}
		else if (keyLen != 0 && tKeyLen != 0)
		{
			pOtherKey = pOtherKey + sizeof(int);
			int len = keyLen < tKeyLen ? keyLen : tKeyLen;
			int res = memcmp(key, pOtherKey, len);
			if (keyLen == tKeyLen)
				return res;
			else if (res == 0)
			{
				return keyLen > tKeyLen ? 1 : -1;
			}
			else
			{
				return res;
			}
		}
		return 0;
	}

	default:
		return 0;
	}
	return 0;
}

int Visit(Mallocator &stMalloc, ALLOC_HANDLE_T &hRecord, void *pCookie)
{
	pResCookie *cookie = reinterpret_cast<pResCookie *>(pCookie);
	const char *m_pchContent = reinterpret_cast<char *>(stMalloc.handle_to_ptr(hRecord));
	uint32_t hRecordRowCnts = *(uint32_t *)(m_pchContent + sizeof(unsigned char) + sizeof(uint32_t));

	if (cookie->nodesNum > 0 && cookie->rowsGot >= cookie->nodesNum)
		return 0;
	(cookie->m_handle)[cookie->nodesGot] = hRecord;
	cookie->nodesGot = cookie->nodesGot + 1;
	cookie->rowsGot = cookie->rowsGot + hRecordRowCnts;
	return 0;
}

int _TtreeNode::Init()
{
	m_hLeft = INVALID_HANDLE;
	m_hRight = INVALID_HANDLE;
	m_chBalance = 0;
	m_ushNItems = 0;
	for (int i = 0; i < PAGE_SIZE; i++)
		m_ahItems[i] = INVALID_HANDLE;
	return (0);
}

ALLOC_HANDLE_T _TtreeNode::Alloc(Mallocator &stMalloc, ALLOC_HANDLE_T hRecord)
{
	ALLOC_HANDLE_T h;
	h = stMalloc.Malloc(sizeof(TtreeNode));
	if (h == INVALID_HANDLE)
		return (INVALID_HANDLE);

	TtreeNode *p = (TtreeNode *)stMalloc.handle_to_ptr(h);
	p->Init();
	p->m_ahItems[0] = hRecord;
	p->m_ushNItems = 1;

	return (h);
}

int convert_cvalue(Mallocator &stMalloc, DTCValue *pch, void *pCmpCookie, ALLOC_HANDLE_T hReInsert)
{
	CmpCookie *cookie = reinterpret_cast<CmpCookie *>(pCmpCookie);
	const DTCTableDefinition *t_pstTab = cookie->m_pstTab;
	const int idx = cookie->m_uchIdx;
	int fieldType = t_pstTab->field_type(idx);

	char *pchKey = ((DataChunk *)stMalloc.handle_to_ptr(hReInsert))->index_key();

	switch (fieldType)
	{
	case DField::Signed:
		if (unlikely(t_pstTab->field_size(idx) > (int)sizeof(int32_t)))
			pch->s64 = *(int64_t *)pchKey;
		else
			pch->s64 = (int64_t) * (int32_t *)pchKey;
		break;

	case DField::Unsigned:
		if (unlikely(t_pstTab->field_size(idx) > (int)sizeof(uint32_t)))
			pch->u64 = *(uint64_t *)pchKey;
		else
			pch->u64 = (uint64_t) * (uint32_t *)pchKey;
		break;

	case DField::Float:
		if (likely(t_pstTab->field_size(idx) > (int)sizeof(float)))
			pch->flt = *(double *)pchKey;
		else
			pch->flt = (double)*(float *)pchKey;
		break;

	case DField::String:
	case DField::Binary:
		pch->bin.len = *((int *)pchKey);
		pch->bin.ptr = pchKey + sizeof(int);
		break;
	}

	return 0;
}

int _TtreeNode::Insert(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T hRecord, bool &isAllocNode)
{
	TtreeNode *pstNode;

	GET_OBJ(stMalloc, hNode, pstNode);
	uint16_t ushNodeCnt = pstNode->m_ushNItems;
	int iDiff = pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[0]);

	if (iDiff == 0)
	{
		//		assert(0);
		return (-2);
	}

	if (iDiff <= 0)
	{
		ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
		if ((hLeft == INVALID_HANDLE || iDiff == 0) && pstNode->m_ushNItems < PAGE_SIZE)
		{
			for (uint32_t i = ushNodeCnt; i > 0; i--)
				pstNode->m_ahItems[i] = pstNode->m_ahItems[i - 1];
			pstNode->m_ahItems[0] = hRecord;
			pstNode->m_ushNItems++;
			return (0);
		}
		if (hLeft == INVALID_HANDLE)
		{
			hLeft = Alloc(stMalloc, hRecord);
			if (hLeft == INVALID_HANDLE)
				return (-1);
			isAllocNode = true;
			pstNode->m_hLeft = hLeft;
		}
		else
		{
			ALLOC_HANDLE_T hChild = hLeft;
			int iGrow = Insert(stMalloc, hChild, pchKey, pCmpCookie, pfComp, hRecord, isAllocNode);
			if (iGrow < 0)
				return iGrow;
			if (hChild != hLeft)
			{
				hLeft = hChild;
				pstNode->m_hLeft = hChild;
			}
			if (iGrow == 0)
				return (0);
		}
		if (pstNode->m_chBalance > 0)
		{
			pstNode->m_chBalance = 0;
			return (0);
		}
		else if (pstNode->m_chBalance == 0)
		{
			pstNode->m_chBalance = -1;
			return (1);
		}
		else
		{
			TtreeNode *pstLeft = (TtreeNode *)stMalloc.handle_to_ptr(hLeft);
			if (pstLeft->m_chBalance < 0)
			{ // single LL turn
				pstNode->m_hLeft = pstLeft->m_hRight;
				pstLeft->m_hRight = hNode;
				pstNode->m_chBalance = 0;
				pstLeft->m_chBalance = 0;
				hNode = hLeft;
			}
			else
			{ // double LR turn
				ALLOC_HANDLE_T hRight = pstLeft->m_hRight;
				TtreeNode *pstRight = (TtreeNode *)stMalloc.handle_to_ptr(hRight);
				pstLeft->m_hRight = pstRight->m_hLeft;
				pstRight->m_hLeft = hLeft;
				pstNode->m_hLeft = pstRight->m_hRight;
				pstRight->m_hRight = hNode;
				pstNode->m_chBalance = (pstRight->m_chBalance < 0) ? 1 : 0;
				pstLeft->m_chBalance = (pstRight->m_chBalance > 0) ? -1 : 0;
				pstRight->m_chBalance = 0;
				hNode = hRight;
			}
			return (0);
		}
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[ushNodeCnt - 1]);
	if (iDiff == 0)
	{
		//		assert(0);
		return (-2);
	}
	if (iDiff >= 0)
	{
		ALLOC_HANDLE_T hRight = pstNode->m_hRight;
		if ((hRight == INVALID_HANDLE || iDiff == 0) && pstNode->m_ushNItems < PAGE_SIZE)
		{
			pstNode->m_ahItems[ushNodeCnt] = hRecord;
			pstNode->m_ushNItems++;
			return (0);
		}
		if (hRight == INVALID_HANDLE)
		{
			hRight = Alloc(stMalloc, hRecord);
			if (hRight == INVALID_HANDLE)
				return (-1);
			pstNode->m_hRight = hRight;
			isAllocNode = true;
		}
		else
		{
			ALLOC_HANDLE_T hChild = hRight;
			int iGrow = Insert(stMalloc, hChild, pchKey, pCmpCookie, pfComp, hRecord, isAllocNode);
			if (iGrow < 0)
				return iGrow;
			if (hChild != hRight)
			{
				hRight = hChild;
				pstNode->m_hRight = hChild;
			}
			if (iGrow == 0)
				return (0);
		}
		if (pstNode->m_chBalance < 0)
		{
			pstNode->m_chBalance = 0;
			return (0);
		}
		else if (pstNode->m_chBalance == 0)
		{
			pstNode->m_chBalance = 1;
			return (1);
		}
		else
		{
			TtreeNode *pstRight = (TtreeNode *)stMalloc.handle_to_ptr(hRight);
			if (pstRight->m_chBalance > 0)
			{ // single RR turn
				pstNode->m_hRight = pstRight->m_hLeft;
				pstRight->m_hLeft = hNode;
				pstNode->m_chBalance = 0;
				pstRight->m_chBalance = 0;
				hNode = hRight;
			}
			else
			{ // double RL turn
				ALLOC_HANDLE_T hLeft = pstRight->m_hLeft;
				TtreeNode *pstLeft = (TtreeNode *)stMalloc.handle_to_ptr(hLeft);
				pstRight->m_hLeft = pstLeft->m_hRight;
				pstLeft->m_hRight = hRight;
				pstNode->m_hRight = pstLeft->m_hLeft;
				pstLeft->m_hLeft = hNode;
				pstNode->m_chBalance = (pstLeft->m_chBalance > 0) ? -1 : 0;
				pstRight->m_chBalance = (pstLeft->m_chBalance < 0) ? 1 : 0;
				pstLeft->m_chBalance = 0;
				hNode = hLeft;
			}
			return (0);
		}
	}

	int iLeft = 1;
	int iRight = ushNodeCnt - 1;
	while (iLeft < iRight)
	{
		int i = (iLeft + iRight) >> 1;
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[i]);
		if (iDiff == 0)
		{
			//			assert(0);
			return (-2);
		}
		if (iDiff > 0)
		{
			iLeft = i + 1;
		}
		else
		{
			iRight = i;
			if (iDiff == 0)
				break;
		}
	}
	// Insert before item[r]
	if (pstNode->m_ushNItems < PAGE_SIZE)
	{
		for (int i = ushNodeCnt; i > iRight; i--)
			pstNode->m_ahItems[i] = pstNode->m_ahItems[i - 1];
		pstNode->m_ahItems[iRight] = hRecord;
		pstNode->m_ushNItems++;
		return (0);
	}
	else
	{
		TtreeNode stBackup;
		memcpy(&stBackup, pstNode, sizeof(TtreeNode));
		ALLOC_HANDLE_T hReInsert;
		if (pstNode->m_chBalance >= 0)
		{
			hReInsert = pstNode->m_ahItems[0];
			for (int i = 1; i < iRight; i++)
				pstNode->m_ahItems[i - 1] = pstNode->m_ahItems[i];
			pstNode->m_ahItems[iRight - 1] = hRecord;
		}
		else
		{
			hReInsert = pstNode->m_ahItems[ushNodeCnt - 1];
			for (int i = ushNodeCnt - 1; i > iRight; i--)
				pstNode->m_ahItems[i] = pstNode->m_ahItems[i - 1];
			pstNode->m_ahItems[iRight] = hRecord;
		}

		DTCValue pch;
		convert_cvalue(stMalloc, &pch, pCmpCookie, hReInsert);
		int iRet = Insert(stMalloc, hNode, (const char *)(&pch), pCmpCookie, pfComp, hReInsert, isAllocNode);
		if (iRet < 0)
		{
			memcpy(pstNode->m_ahItems, stBackup.m_ahItems, sizeof(pstNode->m_ahItems));
		}
		return (iRet);
	}
}

int _TtreeNode::Delete(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, bool &isFreeNode)
{
	TtreeNode *pstNode;
	ALLOC_HANDLE_T hTmp;

	GET_OBJ(stMalloc, hNode, pstNode);
	uint16_t ushNodeCnt = pstNode->m_ushNItems;
	int iDiff = pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[0]);

	if (iDiff < 0)
	{
		ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
		if (hLeft != INVALID_HANDLE)
		{
			ALLOC_HANDLE_T hChild = hLeft;
			int iRet = Delete(stMalloc, hChild, pchKey, pCmpCookie, pfComp, isFreeNode);
			if (iRet < -1)
				return (iRet);
			if (hChild != hLeft)
			{
				pstNode->m_hLeft = hChild;
			}
			if (iRet > 0)
			{
				return balance_left_branch(stMalloc, hNode);
			}
			else if (iRet == 0)
			{
				return (0);
			}
		}
		//		assert(iDiff == 0);
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[ushNodeCnt - 1]);
	if (iDiff <= 0)
	{
		for (int i = 0; i < ushNodeCnt; i++)
		{
			if (pfComp(pchKey, pCmpCookie, stMalloc, pstNode->m_ahItems[i]) == 0)
			{
				if (ushNodeCnt == 1)
				{
					if (pstNode->m_hRight == INVALID_HANDLE)
					{
						hTmp = pstNode->m_hLeft;
						stMalloc.Free(hNode);
						hNode = hTmp;
						return (1);
					}
					else if (pstNode->m_hLeft == INVALID_HANDLE)
					{
						hTmp = pstNode->m_hRight;
						stMalloc.Free(hNode);
						hNode = hTmp;
						return (1);
					}
					isFreeNode = true;
				}
				ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
				ALLOC_HANDLE_T hRight = pstNode->m_hRight;
				if (ushNodeCnt <= MIN_ITEMS)
				{
					if (hLeft != INVALID_HANDLE && pstNode->m_chBalance <= 0)
					{
						TtreeNode *pstLeft;
						GET_OBJ(stMalloc, hLeft, pstLeft);
						while (pstLeft->m_hRight != INVALID_HANDLE)
						{
							GET_OBJ(stMalloc, pstLeft->m_hRight, pstLeft);
						}
						while (--i >= 0)
						{
							pstNode->m_ahItems[i + 1] = pstNode->m_ahItems[i];
						}
						pstNode->m_ahItems[0] = pstLeft->m_ahItems[pstLeft->m_ushNItems - 1];
						DTCValue pch;
						convert_cvalue(stMalloc, &pch, pCmpCookie, pstNode->m_ahItems[0]);

						ALLOC_HANDLE_T hChild = hLeft;
						int iRet = Delete(stMalloc, hChild, (const char *)(&pch), pCmpCookie, pfComp, isFreeNode);
						if (iRet < -1)
						{
							return (iRet);
						}
						if (hChild != hLeft)
						{
							pstNode->m_hLeft = hChild;
						}
						if (iRet > 0)
						{
							iRet = balance_left_branch(stMalloc, hNode);
						}
						return (iRet);
					}
					else if (pstNode->m_hRight != INVALID_HANDLE)
					{
						TtreeNode *pstRight;
						GET_OBJ(stMalloc, hRight, pstRight);
						while (pstRight->m_hLeft != INVALID_HANDLE)
						{
							GET_OBJ(stMalloc, pstRight->m_hLeft, pstRight);
						}
						while (++i < ushNodeCnt)
						{
							pstNode->m_ahItems[i - 1] = pstNode->m_ahItems[i];
						}
						pstNode->m_ahItems[ushNodeCnt - 1] = pstRight->m_ahItems[0];
						DTCValue pch;
						convert_cvalue(stMalloc, &pch, pCmpCookie, pstNode->m_ahItems[ushNodeCnt - 1]);
						ALLOC_HANDLE_T hChild = hRight;
						int iRet = Delete(stMalloc, hChild, (const char *)(&pch), pCmpCookie, pfComp, isFreeNode);
						if (iRet < -1)
						{
							return (iRet);
						}
						if (hChild != hRight)
						{
							pstNode->m_hRight = hChild;
						}
						if (iRet > 0)
						{
							iRet = balance_right_branch(stMalloc, hNode);
						}
						return (iRet);
					}
				}

				while (++i < ushNodeCnt)
				{
					pstNode->m_ahItems[i - 1] = pstNode->m_ahItems[i];
				}
				pstNode->m_ushNItems--;

				return (0);
			}
		}
	}

	ALLOC_HANDLE_T hRight = pstNode->m_hRight;
	if (hRight != 0)
	{
		ALLOC_HANDLE_T hChild = hRight;
		int iRet = Delete(stMalloc, hChild, pchKey, pCmpCookie, pfComp, isFreeNode);
		if (iRet < -1)
		{
			return (iRet);
		}
		if (hChild != hRight)
		{
			pstNode->m_hRight = hChild;
		}
		if (iRet > 0)
		{
			return balance_right_branch(stMalloc, hNode);
		}
		else
		{
			return iRet;
		}
	}

	return -1;
}

inline int _TtreeNode::balance_left_branch(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode)
{
	TtreeNode *pstNode;
	GET_OBJ(stMalloc, hNode, pstNode);

	if (pstNode->m_chBalance < 0)
	{
		pstNode->m_chBalance = 0;
		return (1);
	}
	else if (pstNode->m_chBalance == 0)
	{
		pstNode->m_chBalance = 1;
		return (0);
	}
	else
	{
		ALLOC_HANDLE_T hRight = pstNode->m_hRight;
		TtreeNode *pstRight;
		GET_OBJ(stMalloc, hRight, pstRight);

		if (pstRight->m_chBalance >= 0)
		{ // single RR turn
			pstNode->m_hRight = pstRight->m_hLeft;
			pstRight->m_hLeft = hNode;
			if (pstRight->m_chBalance == 0)
			{
				pstNode->m_chBalance = 1;
				pstRight->m_chBalance = -1;
				hNode = hRight;
				return 0;
			}
			else
			{
				pstNode->m_chBalance = 0;
				pstRight->m_chBalance = 0;
				hNode = hRight;
				return 1;
			}
		}
		else
		{ // double RL turn
			ALLOC_HANDLE_T hLeft = pstRight->m_hLeft;
			TtreeNode *pstLeft;
			GET_OBJ(stMalloc, hLeft, pstLeft);
			pstRight->m_hLeft = pstLeft->m_hRight;
			pstLeft->m_hRight = hRight;
			pstNode->m_hRight = pstLeft->m_hLeft;
			pstLeft->m_hLeft = hNode;
			pstNode->m_chBalance = pstLeft->m_chBalance > 0 ? -1 : 0;
			pstRight->m_chBalance = pstLeft->m_chBalance < 0 ? 1 : 0;
			pstLeft->m_chBalance = 0;
			hNode = hLeft;
			return 1;
		}
	}
}

inline int _TtreeNode::balance_right_branch(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode)
{
	TtreeNode *pstNode;
	GET_OBJ(stMalloc, hNode, pstNode);

	if (pstNode->m_chBalance > 0)
	{
		pstNode->m_chBalance = 0;
		return (1);
	}
	else if (pstNode->m_chBalance == 0)
	{
		pstNode->m_chBalance = -1;
		return (0);
	}
	else
	{
		ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
		TtreeNode *pstLeft;
		GET_OBJ(stMalloc, hLeft, pstLeft);
		if (pstLeft->m_chBalance <= 0)
		{ // single LL turn
			pstNode->m_hLeft = pstLeft->m_hRight;
			pstLeft->m_hRight = hNode;
			if (pstLeft->m_chBalance == 0)
			{
				pstNode->m_chBalance = -1;
				pstLeft->m_chBalance = 1;
				hNode = hLeft;
				return (0);
			}
			else
			{
				pstNode->m_chBalance = 0;
				pstLeft->m_chBalance = 0;
				hNode = hLeft;
				return (1);
			}
		}
		else
		{ // double LR turn
			ALLOC_HANDLE_T hRight = pstLeft->m_hRight;
			TtreeNode *pstRight;
			GET_OBJ(stMalloc, hRight, pstRight);

			pstLeft->m_hRight = pstRight->m_hLeft;
			pstRight->m_hLeft = hLeft;
			pstNode->m_hLeft = pstRight->m_hRight;
			pstRight->m_hRight = hNode;
			pstNode->m_chBalance = pstRight->m_chBalance < 0 ? 1 : 0;
			pstLeft->m_chBalance = pstRight->m_chBalance > 0 ? -1 : 0;
			pstRight->m_chBalance = 0;
			hNode = hRight;
			return (1);
		}
	}
}

unsigned _TtreeNode::ask_for_destroy_size(Mallocator &stMalloc, ALLOC_HANDLE_T hNode)
{
	unsigned size = 0;

	if (INVALID_HANDLE == hNode)
		return size;

	TtreeNode *pstNode;
	GET_OBJ(stMalloc, hNode, pstNode);
	ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
	ALLOC_HANDLE_T hRight = pstNode->m_hRight;

	for (int i = 0; i < pstNode->m_ushNItems; i++)
		size += stMalloc.chunk_size(pstNode->m_ahItems[i]);
	//size += ((DataChunk*)(stMalloc.handle_to_ptr(pstNode->m_ahItems[i])))->ask_for_destroy_size(&stMalloc);

	size += stMalloc.chunk_size(hNode);

	size += ask_for_destroy_size(stMalloc, hLeft);
	size += ask_for_destroy_size(stMalloc, hRight);

	return size;
}

int _TtreeNode::Destroy(Mallocator &stMalloc, ALLOC_HANDLE_T hNode)
{
	if (hNode != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(stMalloc, hNode, pstNode);
		ALLOC_HANDLE_T hLeft = pstNode->m_hLeft;
		ALLOC_HANDLE_T hRight = pstNode->m_hRight;
		for (int i = 0; i < pstNode->m_ushNItems; i++)
			stMalloc.Free(pstNode->m_ahItems[i]);
		//((DataChunk*)(stMalloc.handle_to_ptr(pstNode->m_ahItems[i])))->Destroy(&stMalloc);
		stMalloc.Free(hNode);

		Destroy(stMalloc, hLeft);
		Destroy(stMalloc, hRight);
	}
	return (0);
}

int _TtreeNode::Find(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T *&phRecord)
{
	int iDiff;

	phRecord = NULL;
	if (m_ushNItems == 0)
		return (0);

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff == 0)
	{
		phRecord = &(m_ahItems[0]);
		return (1);
	}
	else if (iDiff > 0)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
		if (iDiff == 0)
		{
			phRecord = &(m_ahItems[m_ushNItems - 1]);
			return (1);
		}
		else if (iDiff > 0)
		{
			if (m_hRight == INVALID_HANDLE)
			{
				return (0);
			}
			TtreeNode *pstNode;
			GET_OBJ(stMalloc, m_hRight, pstNode);
			return pstNode->Find(stMalloc, pchKey, pCmpCookie, pfComp, phRecord);
		}

		int iLeft = 1;
		int iRight = m_ushNItems - 1;
		while (iLeft < iRight)
		{
			int i = (iLeft + iRight) >> 1;
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff == 0)
			{
				phRecord = &(m_ahItems[i]);
				return (1);
			}
			if (iDiff > 0)
			{
				iLeft = i + 1;
			}
			else
			{
				iRight = i;
			}
		}
		return (0);
	}
	else
	{
		if (m_hLeft == INVALID_HANDLE)
		{
			return (0);
		}
		TtreeNode *pstNode;
		GET_OBJ(stMalloc, m_hLeft, pstNode);
		return pstNode->Find(stMalloc, pchKey, pCmpCookie, pfComp, phRecord);
	}
}

int _TtreeNode::Find(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord)
{
	int iRet;
	ALLOC_HANDLE_T *phItem;

	hRecord = INVALID_HANDLE;
	iRet = Find(stMalloc, pchKey, pCmpCookie, pfComp, phItem);
	if (iRet == 1 && phItem != NULL)
	{
		hRecord = *phItem;
	}

	return (iRet);
}

int _TtreeNode::find_handle(Mallocator &stMalloc, ALLOC_HANDLE_T hRecord)
{

	if (m_ushNItems == 0)
		return (0);

	for (int i = 0; i < m_ushNItems; i++)
		if (m_ahItems[i] == hRecord)
			return (1);

	TtreeNode *pstNode;
	if (m_hRight != INVALID_HANDLE)
	{
		GET_OBJ(stMalloc, m_hRight, pstNode);
		if (pstNode->find_handle(stMalloc, hRecord) == 1)
			return (1);
	}

	if (m_hLeft != INVALID_HANDLE)
	{
		GET_OBJ(stMalloc, m_hLeft, pstNode);
		if (pstNode->find_handle(stMalloc, hRecord) == 1)
			return (1);
	}

	return (0);
}

int _TtreeNode::find_node(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hNode)
{
	int iDiff;

	hNode = INVALID_HANDLE;
	if (m_ushNItems == 0)
		return (0);

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff == 0)
	{
		hNode = stMalloc.ptr_to_handle(this);
		return (1);
	}
	else if (iDiff > 0)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
		if (iDiff <= 0)
		{
			hNode = stMalloc.ptr_to_handle(this);
			return (1);
		}
		else if (iDiff > 0)
		{
			if (m_hRight == INVALID_HANDLE)
			{
				return (0);
			}
			TtreeNode *pstNode;
			GET_OBJ(stMalloc, m_hRight, pstNode);
			return pstNode->find_node(stMalloc, pchKey, pCmpCookie, pfComp, hNode);
		}
	}
	else
	{
		if (m_hLeft == INVALID_HANDLE)
		{
			hNode = stMalloc.ptr_to_handle(this);
			return (1);
		}
		TtreeNode *pstNode;
		GET_OBJ(stMalloc, m_hLeft, pstNode);
		return pstNode->find_node(stMalloc, pchKey, pCmpCookie, pfComp, hNode);
	}

	return (0);
}

int _TtreeNode::traverse_forward(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie)
{
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_forward(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	for (int i = 0; i < m_ushNItems; i++)
	{
		if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
		{
			return (iRet);
		}
	}

	if (m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_forward(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_backward(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie)
{
	int iRet;

	if (m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_backward(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}
	for (int i = m_ushNItems; --i >= 0;)
	{
		if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
		{
			return (iRet);
		}
	}
	if (m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_backward(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::post_order_traverse(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie)
{
	int iRet;

	if (m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->post_order_traverse(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	if (m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->post_order_traverse(stMalloc, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	for (int i = m_ushNItems; --i >= 0;)
	{
		if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_forward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, int iInclusion, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
		if (iDiff < 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_forward(stMalloc, pchKey, pCmpCookie, pfComp, iInclusion, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	int i = m_ushNItems;
	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	if (iDiff <= 0)
	{
		for (i = 0; i < m_ushNItems; i++)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0 && iDiff >= 0 - iInclusion)
			{
				if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
				{
					return (iRet);
				}
			}
			else if (iDiff < 0 - iInclusion)
			{
				break;
			}
		}
	}

	if (i >= m_ushNItems && m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_forward(stMalloc, pchKey, pCmpCookie, pfComp, iInclusion, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_forward(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iDiff1;
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
		if (iDiff < 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_forward(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	int i;
	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff1 < 0 || iDiff > 0)
	{ // key1 < item[0]   OR   key > item[n]
	}
	else
	{
		for (i = 0; i < m_ushNItems; i++)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0)
			{
				iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[i]);
				if (iDiff1 >= 0)
				{
					if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
					{
						return (iRet);
					}
				}
			}
		}
	}

	iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	if (iDiff1 >= 0 && m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_forward(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_forward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
		if (iDiff < 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_forward(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	int i;
	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	if (iDiff <= 0)
	{
		for (i = 0; i < m_ushNItems; i++)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0)
			{
				if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
				{
					return (iRet);
				}
			}
		}
	}

	if (m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_forward(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_backward(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iDiff1;
	int iRet;
	int i;

	if (m_hRight != INVALID_HANDLE)
	{
		iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
		if (iDiff1 > 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_backward(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff1 < 0 || iDiff > 0)
	{ // key1 < item[0]   OR   key > item[n]
	}
	else
	{
		for (i = m_ushNItems; --i >= 0;)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0)
			{
				iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[i]);
				if (iDiff1 >= 0)
				{
					if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
					{
						return (iRet);
					}
				}
			}
		}
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff <= 0 && m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_backward(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::traverse_backward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iRet;

	if (m_hRight != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
		if (iDiff > 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->traverse_backward(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff >= 0)
	{
		for (int i = m_ushNItems; --i >= 0;)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff >= 0)
			{
				if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
				{
					return (iRet);
				}
			}
		}
	}

	if (m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->traverse_backward(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	return (0);
}

int _TtreeNode::post_order_traverse(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iDiff1;
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
		if (iDiff < 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->post_order_traverse(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	if (iDiff1 >= 0 && m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->post_order_traverse(stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	int i;
	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff1 < 0 || iDiff > 0)
	{ // key1 < item[0]   OR   key > item[n]
	}
	else
	{
		for (i = 0; i < m_ushNItems; i++)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0)
			{
				iDiff1 = pfComp(pchKey1, pCmpCookie, stMalloc, m_ahItems[i]);
				if (iDiff1 >= 0)
				{
					if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
					{
						return (iRet);
					}
				}
			}
		}
	}

	return (0);
}

int _TtreeNode::post_order_traverse_ge(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iRet;

	if (m_hLeft != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
		if (iDiff < 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->post_order_traverse_ge(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	if (m_hRight != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->post_order_traverse_ge(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	int i;
	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
	if (iDiff <= 0)
	{
		for (i = 0; i < m_ushNItems; i++)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff <= 0)
			{
				if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
				{
					return (iRet);
				}
			}
		}
	}

	return (0);
}

int _TtreeNode::post_order_traverse_le(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	int iDiff;
	int iRet;

	if (m_hRight != INVALID_HANDLE)
	{
		iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[m_ushNItems - 1]);
		if (iDiff > 0)
		{
			if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hRight))->post_order_traverse_le(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
			{
				return (iRet);
			}
		}
	}

	if (m_hLeft != INVALID_HANDLE)
	{
		if ((iRet = ((TtreeNode *)stMalloc.handle_to_ptr(m_hLeft))->post_order_traverse_le(stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie)) != 0)
		{
			return (iRet);
		}
	}

	iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[0]);
	if (iDiff >= 0)
	{
		for (int i = m_ushNItems; --i >= 0;)
		{
			iDiff = pfComp(pchKey, pCmpCookie, stMalloc, m_ahItems[i]);
			if (iDiff >= 0)
			{
				if ((iRet = pfVisit(stMalloc, m_ahItems[i], pCookie)) != 0)
				{
					return (iRet);
				}
			}
		}
	}

	return (0);
}

Ttree::Ttree(Mallocator &stMalloc) : m_stMalloc(stMalloc)
{
	m_hRoot = INVALID_HANDLE;
	m_szErr[0] = 0;
}

Ttree::~Ttree()
{
}

ALLOC_HANDLE_T Ttree::first_node()
{
	if (m_hRoot == INVALID_HANDLE)
		return INVALID_HANDLE;
	TtreeNode *pstNode;
	GET_OBJ(m_stMalloc, m_hRoot, pstNode);
	return pstNode->m_ahItems[0];
}

int Ttree::Insert(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T hRecord, bool &isAllocNode)
{
	ALLOC_HANDLE_T hNode;

	if (m_hRoot == INVALID_HANDLE)
	{
		hNode = TtreeNode::Alloc(m_stMalloc, hRecord);
		if (hNode == INVALID_HANDLE)
		{
			snprintf(m_szErr, sizeof(m_szErr), "alloc tree-node error: %s", m_stMalloc.get_err_msg());
			return (EC_NO_MEM);
		}
		isAllocNode = true;
		m_hRoot = hNode;
	}
	else
	{
		hNode = m_hRoot;
		int iRet = TtreeNode::Insert(m_stMalloc, hNode, pchKey, pCmpCookie, pfComp, hRecord, isAllocNode);
		if (iRet == -2)
		{
			snprintf(m_szErr, sizeof(m_szErr), "key already exists.");
			return (EC_KEY_EXIST);
		}
		else if (iRet == -1)
		{
			snprintf(m_szErr, sizeof(m_szErr), "alloc tree-node error: %s", m_stMalloc.get_err_msg());
			return (EC_NO_MEM);
		}
		else if (iRet < 0)
		{
			snprintf(m_szErr, sizeof(m_szErr), "insert error");
			return (-1);
		}
		if (hNode != m_hRoot)
		{
			m_hRoot = hNode;
		}
	}

	return (0);
}

int Ttree::Delete(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, bool &isFreeNode)
{
	if (m_hRoot == INVALID_HANDLE)
	{
		return (0);
	}

	ALLOC_HANDLE_T hNode = m_hRoot;
	int iRet = TtreeNode::Delete(m_stMalloc, hNode, pchKey, pCmpCookie, pfComp, isFreeNode);
	if (iRet < -1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "internal error");
		return (-1);
	}
	else if (iRet == -1)
	{
		snprintf(m_szErr, sizeof(m_szErr), "tree error");
		return (-1);
	}
	if (hNode != m_hRoot)
		m_hRoot = hNode;

	return (0);
}

int Ttree::find_handle(ALLOC_HANDLE_T hRecord)
{
	if (m_hRoot == INVALID_HANDLE)
	{
		return (0);
	}

	TtreeNode *pstNode;
	GET_OBJ(m_stMalloc, m_hRoot, pstNode);
	return pstNode->find_handle(m_stMalloc, hRecord);
}

int Ttree::Find(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord)
{
	hRecord = INVALID_HANDLE;
	if (m_hRoot == INVALID_HANDLE)
	{
		return (0);
	}

	TtreeNode *pstNode;
	GET_OBJ(m_stMalloc, m_hRoot, pstNode);
	return pstNode->Find(m_stMalloc, pchKey, pCmpCookie, pfComp, hRecord);
}

int Ttree::Find(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T *&phRecord)
{
	phRecord = NULL;
	if (m_hRoot == INVALID_HANDLE)
	{
		return (0);
	}

	TtreeNode *pstNode;
	GET_OBJ(m_stMalloc, m_hRoot, pstNode);
	return pstNode->Find(m_stMalloc, pchKey, pCmpCookie, pfComp, phRecord);
}

int Ttree::Destroy()
{
	TtreeNode::Destroy(m_stMalloc, m_hRoot);
	m_hRoot = INVALID_HANDLE;
	return (0);
}

unsigned Ttree::ask_for_destroy_size(void)
{
	return TtreeNode::ask_for_destroy_size(m_stMalloc, m_hRoot);
}

int Ttree::traverse_forward(ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);
		return pstNode->traverse_forward(m_stMalloc, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_backward(ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);
		return pstNode->traverse_backward(m_stMalloc, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::post_order_traverse(ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);
		return pstNode->post_order_traverse(m_stMalloc, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_forward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, int64_t iInclusion, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->traverse_forward(m_stMalloc, pchKey, pCmpCookie, pfComp, iInclusion, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_forward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->traverse_forward(m_stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_forward(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->traverse_forward(m_stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_backward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->traverse_backward(m_stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::traverse_backward(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->traverse_backward(m_stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::post_order_traverse(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->post_order_traverse(m_stMalloc, pchKey, pchKey1, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::post_order_traverse_ge(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->post_order_traverse_ge(m_stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}

int Ttree::post_order_traverse_le(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie)
{
	if (m_hRoot != INVALID_HANDLE)
	{
		TtreeNode *pstNode;
		GET_OBJ(m_stMalloc, m_hRoot, pstNode);

		return pstNode->post_order_traverse_le(m_stMalloc, pchKey, pCmpCookie, pfComp, pfVisit, pCookie);
	}

	return (0);
}
