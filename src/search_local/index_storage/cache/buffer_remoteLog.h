/*
 * =====================================================================================
 *
 *       Filename:  buffer_remoteLog.h
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
#ifndef _CACHE_REMOTE_LOG_
#define _CACHE_REMOTE_LOG_

#include "value.h"
#include <task_request.h>
#include "singleton.h"
#include <sstream>
#include <map>
#include "table_def.h"
#include "protocol.h"
#include "log.h"
#include <stdio.h>
#define REMOTELOG_OP_FLOW_TYPE 1
extern void remote_log(int type, const char *key, int op_type, int op_result, char *content, long op_time, int cmd, int magic, int contentlen);
enum E_TASK_PROCESS_STAGE
{
	TASK_NOTIFY_STAGE = 0,
	TASK_REPLY_STAGE = 1
};

class CacheRemoteLog
{
public:
	CacheRemoteLog() : m_curtask(NULL), m_IsNoDbMode(false), m_TableDef(0), m_UpdateMode(MODE_SYNC), m_InsertMode(MODE_SYNC), m_RemotePort(0), m_OpLog(false)
	{
	}
	~CacheRemoteLog()
	{
	}
	void set_remote_port(int iPort)
	{
		m_RemotePort = iPort;
	}
	void set_op_log_on()
	{
		m_OpLog = true;
	}
	void set_remote_log_mode(DTCTableDefinition *tableDef, bool isNoDbMode, EUpdateMode insertMode, EUpdateMode updateMode)
	{
		this->m_TableDef = tableDef;
		this->m_IsNoDbMode = isNoDbMode;
		this->m_UpdateMode = updateMode;
		this->m_InsertMode = insertMode;
	}

	void write_remote_log(uint64_t ddwOptime, TaskRequest *curtask, E_TASK_PROCESS_STAGE stage)
	{
		if (0 == m_RemotePort)
		{
			return;
		}
		if (!m_OpLog)
			return;
		this->m_curtask = curtask;
		if (NULL == m_curtask)
		{
			return;
		}

		if ((DRequest::Get == m_curtask->request_code()) || (DRequest::SvrAdmin == m_curtask->request_code()))
		{

			return;
		}

		if ((DRequest::Replace == m_curtask->request_code()) && (m_UpdateMode == MODE_ASYNC || m_InsertMode == MODE_ASYNC))
		{
			return;
		}

		if (NULL == m_TableDef)
		{
			return;
		}
		if (DRequest::Purge == m_curtask->request_code())
		{
			std::string strPurgeContent = "purge Node";
			remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), DRequest::Purge,
					   0, const_cast<char *>(strPurgeContent.c_str()), ddwOptime, 0, 0, strPurgeContent.length());
			return;
		}

		if (!m_IsNoDbMode)
		{

			if (TASK_NOTIFY_STAGE == stage)
			{

				if ((m_UpdateMode == MODE_SYNC) && ((m_InsertMode == MODE_SYNC)))
				{

					return;
				}
				write_task_notify_stage_log(ddwOptime);
				return;
			}
			if (TASK_REPLY_STAGE == stage)
			{

				write_task_reply_stage_log(ddwOptime);
				return;
			}
		}
		else
		{

			write_no_db_op_log(ddwOptime);
		}
	}

private:
	void write_task_notify_stage_log(uint64_t ddwOptime)
	{
		std::stringstream oss;
		oss << "task notify stage, Async mode ,  ";
		if ((DRequest::Update == m_curtask->request_code()) && (m_UpdateMode == MODE_ASYNC))
		{
			oss << extract_update_content();
			remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), m_curtask->request_code(),
					   m_curtask->result_code(), const_cast<char *>(oss.str().c_str()), ddwOptime, 0, 0, oss.str().length());
		}
		if ((DRequest::Insert == m_curtask->request_code()) && (m_InsertMode == MODE_ASYNC))
		{
			oss << extract_insert_content();
			remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), m_curtask->request_code(),
					   m_curtask->result_code(), const_cast<char *>(oss.str().c_str()), ddwOptime, 0, 0, oss.str().length());
		}
		if ((DRequest::Replace == m_curtask->request_code()) && (m_UpdateMode == MODE_ASYNC))
		{
			oss << extract_replace_content();
			remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), m_curtask->request_code(),
					   m_curtask->result_code(), const_cast<char *>(oss.str().c_str()), ddwOptime, 0, 0, oss.str().length());
		}
	}

	void write_task_reply_stage_log(uint64_t ddwOptime)
	{
		std::stringstream oss;
		oss << "task reply stage, ";
		oss << get_op_content();
		remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), m_curtask->request_code(),
				   m_curtask->result_code(), const_cast<char *>(oss.str().c_str()), ddwOptime, 0, 0, oss.str().length());
	}

	void write_no_db_op_log(uint64_t ddwOptime)
	{
		std::string strContent = get_no_db_op_content();
		remote_log(REMOTELOG_OP_FLOW_TYPE, extract_key().c_str(), m_curtask->request_code(),
				   m_curtask->result_code(), const_cast<char *>(strContent.c_str()), ddwOptime, 0, 0, strContent.length());
	}
	std::string get_no_db_op_content()
	{

		std::stringstream oss;
		oss << "NoDb Op,content: " << get_op_content();
		return oss.str();
	}

	std::string get_op_content()
	{
		if (DRequest::Update == m_curtask->request_code())
		{
			return extract_update_content();
		}
		else if (DRequest::Insert == m_curtask->request_code())
		{
			return extract_insert_content();
		}
		else if (DRequest::Delete == m_curtask->request_code())
		{
			return extract_delete_content();
		}
		else if (DRequest::Replace == m_curtask->request_code())
		{
			return extract_replace_content();
		}
		else
		{
			return "";
		}
	}

	void filter_quotation(char *ptr, int len)
	{
		if ((NULL == ptr) || (len <= 0))
		{
			return;
		}

		for (int iCharLoop = 0; iCharLoop < len; iCharLoop++)
		{
			if ('\"' == ptr[iCharLoop])
			{
				ptr[iCharLoop] = '|';
			}
		}
	}
	std::string hex_to_string(char *ptr, int len)
	{
		if ((NULL == ptr) || (len <= 0))
		{
			return "";
		}
		std::string str;
		while (len--)
		{
			char szTemp[16] = {0};
			memset(szTemp, 0, 16);
			snprintf(szTemp, sizeof(szTemp), "%02x", *ptr++);
			str += szTemp;
		}
		return str;
	}

	std::string value_to_str(const DTCValue *value, int fieldType)
	{
		if (NULL == value)
		{
			return "";
		}
		std::stringstream oss;
		switch (fieldType)
		{
		case DField::Signed:
		{
			oss << value->s64;
			break;
		}

		case DField::Unsigned:
		{
			oss << value->u64;
			break;
		}

		case DField::String:
		{
			filter_quotation(value->str.ptr, value->str.len);
			oss << value->str.ptr;
			break;
		}
		case DField::Binary:
		{
			return hex_to_string(value->str.ptr, value->str.len);
		}
		case DField::Float:
		{
			oss << value->flt;
			break;
		}
		default:
		{
			return "";
		}
		}
		return oss.str();
	}

	std::string extract_key()
	{
		if (NULL == m_curtask)
		{
			return "";
		}
		return value_to_str(m_curtask->request_key(), m_TableDef->field_type(0));
	}

	std::string extract_condition_content(const DTCFieldValue *condition)
	{
		if (NULL == condition)
		{
			return "";
		}
		std::stringstream oss;
		oss << "where conditon:[";
		for (int j = 0; j < condition->num_fields(); j++)
		{
			if (m_TableDef->is_volatile(j))
			{
				return "";
			}
			uint8_t op = condition->field_operation(j);
			if (op >= DField::TotalComparison)
			{
				continue;
			}

			static const char *const compStr[] = {"EQ", "NE", "LT", "LE", "GT", "GE"};
			oss << m_TableDef->field_name(condition->field_id(j)) << " ";
			oss << compStr[op] << " ";
			oss << value_to_str(condition->field_value(j), condition->field_type(j));
			oss << ";";
		}
		oss << "]";
		return oss.str();
	}

	std::string extract_update_content(const DTCFieldValue *updateInfo)
	{
		if (NULL == updateInfo)
		{
			return "";
		}
		std::stringstream oss;
		oss << "update content:[";
		for (int i = 0; i < updateInfo->num_fields(); i++)
		{
			const int fid = updateInfo->field_id(i);

			if (m_TableDef->is_volatile(fid))
			{
				continue;
			}

			switch (updateInfo->field_operation(i))
			{
			case DField::Set:
			{
				oss << m_TableDef->field_name(fid) << ":" << value_to_str(updateInfo->field_value(i), updateInfo->field_type(i)) << ";";
				break;
			}
			case DField::Add:
			{
				oss << m_TableDef->field_name(fid) << ":" << m_TableDef->field_name(fid) << "+" << value_to_str(updateInfo->field_value(i), updateInfo->field_type(i)) << ";";
				break;
			}
			default:
			{
				break;
			}
			}
		}
		oss << "]";
		return oss.str();
	}

	std::string extract_insert_content()
	{
		if (NULL == m_curtask)
		{
			return "";
		}
		std::stringstream oss;

		if (m_curtask->request_operation())
		{
			oss << "insert content: [";
			const DTCFieldValue *updateInfo = m_curtask->request_operation();
			for (int i = 0; i < updateInfo->num_fields(); ++i)
			{
				int fid = updateInfo->field_id(i);
				if (m_TableDef->is_volatile(fid))
				{
					continue;
				}
				oss << m_TableDef->field_name(fid) << ":" << value_to_str(updateInfo->field_value(i), updateInfo->field_type(i)) << ";";
			}
			oss << "]";
		}
		return oss.str();
	}

	std::string extract_update_content()
	{
		if (NULL == m_curtask)
		{
			return "";
		}
		std::stringstream oss;
		oss << extract_update_content(m_curtask->request_operation());
		oss << extract_condition_content(m_curtask->request_condition());
		return oss.str();
	}

	std::string extract_delete_content()
	{
		if (NULL == m_curtask)
		{
			return "";
		}
		return extract_condition_content(m_curtask->request_condition());
	}

	std::string extract_replace_content()
	{
		if (NULL == m_curtask)
		{
			return "";
		}
		return extract_update_content(m_curtask->request_operation());
	}

private:
	TaskRequest *m_curtask;
	bool m_IsNoDbMode;
	DTCTableDefinition *m_TableDef;
	EUpdateMode m_UpdateMode;
	EUpdateMode m_InsertMode;
	int m_RemotePort; /*如果端口没有设置正确，写日志函数就啥都不用做了*/
	bool m_OpLog;
};

#define REMOTE_LOG Singleton<CacheRemoteLog>::Instance()
#endif
