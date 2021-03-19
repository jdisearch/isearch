/*
 * =====================================================================================
 *
 *       Filename:  data_chunk.h
 *
 *    Description:  packaging data chunk method.
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

#ifndef DATA_CHUNK_H
#define DATA_CHUNK_H

#include <stdint.h>
#include "raw_data.h"
#include "tree_data.h"

class DataChunk
{
protected:
	unsigned char m_uchDataType; // 数据chunk的类型

public:
	/*************************************************
	  Description:	计算基本结构大小
	  Input:		
	  Output:		
	  Return:		内存大小
	*************************************************/
	ALLOC_SIZE_T base_size()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
			return (sizeof(RawFormat));
		else
			return (sizeof(RootData));
	}

	/*************************************************
	  Description:	index key
	  Input:		
	  Output:		
	  Return:		key
	*************************************************/
	char *index_key()
	{
		char *indexKey = (char *)this;
		return indexKey + sizeof(unsigned char) * 2 + sizeof(uint32_t) * 2;
	}

	/*************************************************
	  Description:	获取格式化后的key
	  Input:		
	  Output:		
	  Return:		key指针
	*************************************************/
	const char *Key() const
	{
		if ((m_uchDataType & 0x7f) == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_achKey;
		}
		else if ((m_uchDataType & 0x7f) == DATA_TYPE_TREE_ROOT)
		{
			RootData *pstRoot = (RootData *)this;
			return pstRoot->m_achKey;
		}
		return NULL;
	}

	/*************************************************
	  Description:	获取格式化后的key
	  Input:		
	  Output:		
	  Return:		key指针
	*************************************************/
	char *Key()
	{
		if ((m_uchDataType & 0x7f) == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_achKey;
		}
		else if ((m_uchDataType & 0x7f) == DATA_TYPE_TREE_ROOT)
		{
			RootData *pstRoot = (RootData *)this;
			return pstRoot->m_achKey;
		}
		return NULL;
	}

	/*************************************************
	  Description:	保存key
	  Input:		key	key的实际值
	  Output:		
	  Return:		
	*************************************************/

#define SET_KEY_FUNC(type, key)                       \
	void set_key(type key)                             \
	{                                                 \
		if (m_uchDataType == DATA_TYPE_RAW)           \
		{                                             \
			RawFormat *pstRaw = (RawFormat *)this;    \
			*(type *)(void *)pstRaw->m_achKey = key;  \
		}                                             \
		else                                          \
		{                                             \
			RootData *pstRoot = (RootData *)this;     \
			*(type *)(void *)pstRoot->m_achKey = key; \
		}                                             \
	}

	SET_KEY_FUNC(int32_t, iKey)
	SET_KEY_FUNC(uint32_t, uiKey)
	SET_KEY_FUNC(int64_t, llKey)
	SET_KEY_FUNC(uint64_t, ullKey)

	/*************************************************
	  Description:	保存字符串key
	  Input:		key	key的实际值
				iLen	key的长度
	  Output:		
	  Return:		
	*************************************************/
	void set_key(const char *pchKey, int iLen)
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			*(unsigned char *)pstRaw->m_achKey = iLen;
			memcpy(pstRaw->m_achKey + 1, pchKey, iLen);
		}
		else
		{
			RootData *pstRoot = (RootData *)this;
			*(unsigned char *)pstRoot->m_achKey = iLen;
			memcpy(pstRoot->m_achKey + 1, pchKey, iLen);
		}
	}

	/*************************************************
	  Description:	保存格式化好的字符串key
	  Input:		key	key的实际值, 要求key[0]是长度
	  Output:		
	  Return:		
	*************************************************/
	void set_key(const char *pchKey)
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			memcpy(pstRaw->m_achKey, pchKey, *(unsigned char *)pchKey);
		}
		else
		{
			RootData *pstRoot = (RootData *)this;
			memcpy(pstRoot->m_achKey, pchKey, *(unsigned char *)pchKey);
		}
	}

	/*************************************************
	  Description:	查询字符串key大小
	  Input:		
	  Output:		
	  Return:		key大小
	*************************************************/
	int str_key_size()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return *(unsigned char *)pstRaw->m_achKey;
		}
		else
		{
			RootData *pstRoot = (RootData *)this;
			return *(unsigned char *)pstRoot->m_achKey;
		}
	}

	/*************************************************
	  Description:	查询二进制key大小
	  Input:		
	  Output:		
	  Return:		key大小
	*************************************************/
	int bin_key_size() { return str_key_size(); }

	unsigned int head_size()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
			return sizeof(RawFormat);
		else
			return sizeof(RootData);
	}

	/*************************************************
	  Description:	查询数据头大小，如果是CRawData的chunk，data_size()是不包括Row的长度，仅包括头部信息以及key
	  Input:		
	  Output:		
	  Return:		内存大小
	*************************************************/
	unsigned int data_size(int iKeySize)
	{
		int iKeyLen = iKeySize ? iKeySize : 1 + str_key_size();
		return head_size() + iKeyLen;
	}

	unsigned int node_size()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_uiDataSize;
		}
		else
		{
			return 0; // unknow
		}
	}

	unsigned int create_time()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_CreateHour;
		}
		else
		{
			return 0; // unknow
		}
	}
	unsigned last_access_time()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_LastAccessHour;
		}
		else
		{
			return 0; // unknow
		}
	}
	unsigned int last_update_time()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_LastUpdateHour;
		}
		else
		{
			return 0; // unknow
		}
	}

	uint32_t total_rows()
	{
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			RawFormat *pstRaw = (RawFormat *)this;
			return pstRaw->m_uiRowCnt;
		}
		else
		{
			RootData *pstRoot = (RootData *)this;
			return pstRoot->m_uiRowCnt;
		}
	}

	/*************************************************
	  Description:	销毁内存并释放内存
	  Input:		
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int Destroy(Mallocator *pstMalloc)
	{
		MEM_HANDLE_T hHandle = pstMalloc->ptr_to_handle(this);
		if (m_uchDataType == DATA_TYPE_RAW)
		{
			return pstMalloc->Free(hHandle);
		}
		else if (m_uchDataType == DATA_TYPE_TREE_ROOT)
		{
			TreeData stTree(pstMalloc);
			int iRet = stTree.Attach(hHandle);
			if (iRet != 0)
			{
				return (iRet);
			}
			return stTree.Destroy();
		}
		return (-1);
	}

	/* 查询如果destroy这块内存，能释放多少空间出来 （包括合并）*/
	unsigned ask_for_destroy_size(Mallocator *pstMalloc)
	{
		MEM_HANDLE_T hHandle = pstMalloc->ptr_to_handle(this);

		if (m_uchDataType == DATA_TYPE_RAW)
		{
			return pstMalloc->ask_for_destroy_size(hHandle);
		}
		else if (m_uchDataType == DATA_TYPE_TREE_ROOT)
		{
			TreeData stTree(pstMalloc);
			if (stTree.Attach(hHandle))
				return 0;

			return stTree.ask_for_destroy_size();
		}

		log_debug("ask_for_destroy_size failed");
		return 0;
	}
};

#endif
