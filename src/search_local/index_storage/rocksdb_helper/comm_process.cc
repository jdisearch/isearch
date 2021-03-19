/*
 * =====================================================================================
 *
 *       Filename: comm_process.cc 
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comm_process.h"
#include <protocol.h>
#include <dbconfig.h>
#include <log.h>
#include <task_base.h>

#include "proc_title.h"

#define CAST(type, var) type *var = (type *)addr

CommHelper::CommHelper() : addr(NULL), check(0)
{
	Timeout = 0;
}

CommHelper::~CommHelper()
{
	addr = NULL;
}

CommHelper::CommHelper(CommHelper &proc)
{
}

void CommHelper::init_title(int group, int role)
{
	snprintf(_name, sizeof(_name),
			 "helper%d%c",
			 group,
			 MACHINEROLESTRING[role]);

	snprintf(_title, sizeof(_title), "%s: ", _name);
	_titlePrefixSize = strlen(_title);
	_title[sizeof(_title) - 1] = '\0';
}

void CommHelper::set_title(const char *status)
{
	strncpy(_title + _titlePrefixSize, status, sizeof(_title) - 1 - _titlePrefixSize);
	set_proc_title(_title);
}

int CommHelper::global_init(void)
{
	return 0;
}

int CommHelper::Init(void)
{
	return 0;
}

int CommHelper::Execute()
{
	int ret = -1;

	log_debug("request code: %d", request_code());

	switch (request_code())
	{
	/*  Nop/Purge/Flush这几个操作对于数据源来说不需要做操作（应该也不会有这样的请求） */
	case DRequest::Nop:
	case DRequest::Purge:
	case DRequest::Flush:
		return 0;

	case DRequest::Get:
		logapi.Start();
		ret = process_get();
		logapi.Done(__FILE__, __LINE__, "SELECT", ret, ret);
		break;

	case DRequest::Insert:
		logapi.Start();
		ret = process_insert();
		logapi.Done(__FILE__, __LINE__, "INSERT", ret, ret);
		break;

	case DRequest::Update:
		logapi.Start();
		ret = process_update();
		logapi.Done(__FILE__, __LINE__, "UPDATE", ret, ret);
		break;

	case DRequest::Delete:
		logapi.Start();
		ret = process_delete();
		logapi.Done(__FILE__, __LINE__, "DELETE", ret, ret);
		break;

	case DRequest::Replace:
		logapi.Start();
		ret = process_replace();
		logapi.Done(__FILE__, __LINE__, "REPLACE", ret, ret);
		break;

	default:
		log_error("unknow request code");
		set_error(-EC_BAD_COMMAND, __FUNCTION__, "[Helper]invalid request-code");
		return -1;
	};

	return ret;
}

void CommHelper::Attach(void *p)
{
	addr = p;
}

DTCTableDefinition *CommHelper::Table(void) const
{
	CAST(DTCTask, task);
	return task->table_definition();
}

const PacketHeader *CommHelper::Header() const
{
#if 0
	CAST(DTCTask, task);
	return &(task->headerInfo);
#else
	// NO header anymore
	return NULL;
#endif
}

const DTCVersionInfo *CommHelper::version_info() const
{
	CAST(DTCTask, task);
	return &(task->versionInfo);
}

int CommHelper::request_code() const
{
	CAST(DTCTask, task);
	return task->request_code();
}

int CommHelper::has_request_key() const
{
	CAST(DTCTask, task);
	return task->has_request_key();
}

const DTCValue *CommHelper::request_key() const
{
	CAST(DTCTask, task);
	return task->request_key();
}

unsigned int CommHelper::int_key() const
{
	CAST(DTCTask, task);
	return task->int_key();
}

const DTCFieldValue *CommHelper::request_condition() const
{
	CAST(DTCTask, task);
	return task->request_condition();
}

const DTCFieldValue *CommHelper::request_operation() const
{
	CAST(DTCTask, task);
	return task->request_operation();
}

const DTCFieldSet *CommHelper::request_fields() const
{
	CAST(DTCTask, task);
	return task->request_fields();
}

void CommHelper::set_error(int err, const char *from, const char *msg)
{
	CAST(DTCTask, task);
	task->set_error(err, from, msg);
}

int CommHelper::count_only(void) const
{
	CAST(DTCTask, task);
	return task->count_only();
}

int CommHelper::all_rows(void) const
{
	CAST(DTCTask, task);
	return task->all_rows();
}

int CommHelper::update_row(RowValue &row)
{
	CAST(DTCTask, task);
	return task->update_row(row);
}

int CommHelper::compare_row(const RowValue &row, int iCmpFirstNField) const
{
	CAST(DTCTask, task);
	return task->compare_row(row, iCmpFirstNField);
}

int CommHelper::prepare_result(void)
{
	CAST(DTCTask, task);
	return task->prepare_result();
}

void CommHelper::update_key(RowValue *r)
{
	CAST(DTCTask, task);
	task->update_key(r);
}

void CommHelper::update_key(RowValue &r)
{
	update_key(&r);
}

int CommHelper::set_total_rows(unsigned int nr)
{
	CAST(DTCTask, task);
	return task->set_total_rows(nr);
}

int CommHelper::set_affected_rows(unsigned int nr)
{
	CAST(DTCTask, task);
	task->resultInfo.set_affected_rows(nr);
	return 0;
}

int CommHelper::append_row(const RowValue &r)
{
	return append_row(&r);
}

int CommHelper::append_row(const RowValue *r)
{
	CAST(DTCTask, task);
	int ret = task->append_row(r);
	if (ret > 0)
		ret = 0;
	return ret;
}

/* 查询服务器地址 */
const char *CommHelper::get_server_address(void) const
{
	return _server_string;
}

/* 查询配置文件 */
int CommHelper::get_int_val(const char *sec, const char *key, int def)
{
	return _config->get_int_val(sec, key, def);
}

unsigned long long CommHelper::get_size_val(const char *sec, const char *key, unsigned long long def, char unit)
{
	return _config->get_size_val(sec, key, def, unit);
}

int CommHelper::get_idx_val(const char *v1, const char *v2, const char *const *v3, int v4)
{
	return _config->get_idx_val(v1, v2, v3, v4);
}

const char *CommHelper::get_str_val(const char *sec, const char *key)
{
	return _config->get_str_val(sec, key);
}

bool CommHelper::has_section(const char *sec)
{
	return _config->has_section(sec);
}

bool CommHelper::has_key(const char *sec, const char *key)
{
	return _config->has_key(sec, key);
}

/* 查询表定义 */
int CommHelper::field_type(int n) const { return _tdef->field_type(n); }
int CommHelper::field_size(int n) const { return _tdef->field_size(n); }
int CommHelper::field_id(const char *n) const { return _tdef->field_id(n); }
const char *CommHelper::field_name(int id) const { return _tdef->field_name(id); }
int CommHelper::num_fields(void) const { return _tdef->num_fields(); }
int CommHelper::key_fields(void) const { return _tdef->key_fields(); }
