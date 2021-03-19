/*
 * =====================================================================================
 *
 *       Filename:  hotback_task.cc
 *
 *    Description:  hot back task.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "hotback_task.h"
#include "mem_check.h"
#include <stdlib.h>
#include <string.h>
HotBackTask::HotBackTask() : m_Type(0), m_Flag(0), m_pPackedKey(NULL), m_PackedKeyLen(0), m_pValue(NULL), m_ValueLen(0)
{
}

HotBackTask::~HotBackTask()
{
	FREE_IF(m_pPackedKey);
	FREE_IF(m_pValue);
}

void HotBackTask::set_packed_key(char *pPackedKey, int keyLen)
{
	if ((NULL == pPackedKey) || (0 == keyLen))
	{
		return;
	}

	m_pPackedKey = (char *)MALLOC(keyLen);
	if (NULL == m_pPackedKey)
	{
		return;
	}
	m_PackedKeyLen = keyLen;
	memcpy(m_pPackedKey, pPackedKey, m_PackedKeyLen);
}

void HotBackTask::set_value(char *pValue, int valueLen)
{
	if ((NULL == pValue) || (0 == valueLen))
	{
		return;
	}

	m_pValue = (char *)MALLOC(valueLen);
	if (NULL == m_pPackedKey)
	{
		return;
	}
	m_ValueLen = valueLen;
	memcpy(m_pValue, pValue, m_ValueLen);
}
