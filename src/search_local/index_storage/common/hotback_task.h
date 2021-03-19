/*
 * =====================================================================================
 *
 *       Filename:  hotback_task.h
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
#ifndef __HOTBACK_TASK_H__
#define __HOTBACK_TASK_H__

struct HotBackTask
{
private:
	/*do nothing, only forbiden use */
	HotBackTask(const HotBackTask &other);
	HotBackTask &operator=(const HotBackTask &other);

public:
	HotBackTask();
	~HotBackTask();

	void set_packed_key(char *pPackedKey, int keyLen);
	void set_value(char *pValue, int valueLen);
	void set_type(int type)
	{
		m_Type = type;
	}
	void set_flag(int flag)
	{
		m_Flag = flag;
	}
	char *get_packed_key()
	{
		return m_pPackedKey;
	}
	char *get_value()
	{
		return m_pValue;
	}
	int get_type()
	{
		return m_Type;
	}
	int get_flag()
	{
		return m_Flag;
	}
	int get_packed_key_len()
	{
		return m_PackedKeyLen;
	}
	int get_value_len()
	{
		return m_ValueLen;
	}

private:
	int m_Type;
	int m_Flag;
	char *m_pPackedKey;
	int m_PackedKeyLen;
	char *m_pValue;
	int m_ValueLen;


};
#endif