/*
 * =====================================================================================
 *
 *       Filename:  task_api.cc
 *
 *    Description:  
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
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include "version.h"

#include "decode.h"
#include "protocol.h"
#include "buffer_error.h"
#include "log.h"
#include "task_request.h"
#include "result.h"
#include <dtc_int.h>

inline int DTCFieldSet::Copy(const FieldSetByName &rq)
{
	const int num = rq.num_fields();
	for (int n = 0; n < num; n++)
	{
		const int id = rq.field_id(n);
		// filter out invalid and duplicated fieldId
		if (id >= 0 && id < INVALID_FIELD_ID && !field_present(id))
			add_field(id);
	}
	return 0;
}

int DTCFieldValue::Copy(const FieldValueByName &rq, int mode, const DTCTableDefinition *tdef)
{
	int clear = 0;
	const int num = rq.num_fields();
	unsigned knum = (int)tdef->key_fields();
	for (int n = 0; n < num; n++)
	{
		unsigned id = rq.field_id(n);
		const DTCValue *val = rq.field_value(n);
		unsigned op = rq.field_operation(n);
		unsigned vt = rq.field_type(n);

		const int ft = tdef->field_type(id);

		if (vt == 0 || vt >= DField::TotalType)
		{
			// value type is invalid
			return -EC_BAD_VALUE_TYPE;
		}

		if (mode == 0)
		{
			// condition information
			if (op >= DField::TotalComparison || DTCTask::validcomps[ft][op] == 0)
			{
				// invalid comparator id or
				// the field type don't support the comparator
				return -EC_BAD_OPERATOR;
			}

			if (id < knum)
			{
				// part of cache hash key fields
				if (op != DField::EQ)
				{
					// key must always EQ
					return -EC_BAD_MULTIKEY;
				}
			}
			else
			{
				// non-key condition exists, clear all_rows flag
				clear = 1;
			}

			if (DTCTask::validxtype[DField::Set][ft][vt] == 0)
			{
				// value type must compatible with field type
				return -EC_BAD_OPERATOR;
			}
		}
		else if (mode == 1)
		{
			// mode 1 is insert/replace
			if (op != DField::Set)
			{
				// insert values, must be assignment
				return -EC_BAD_OPERATOR;
			}
			if (DTCTask::validxtype[op][ft][vt] == 0)
			{
				// value type must compatible with field type
				return -EC_BAD_OPERATOR;
			}
		}
		else
		{
			// update operation
			if (tdef->is_read_only(id))
			{
				// read-only field cannot be updated
				return -EC_READONLY_FIELD;
			}
			if (op >= DField::TotalOperation || DTCTask::validxtype[op][ft][vt] == 0)
			{
				// invalid operator id or
				// the field type don't support the operator
				return -EC_BAD_OPERATOR;
			}
		}

		// everything is fine
		add_value(id, op, vt, *val); // val never bu NULL
		// non key field, mark its type
		update_type_mask(tdef->field_flags(id));
	}

	return clear;
}

int TaskRequest::Copy(NCRequest &rq, const DTCValue *kptr)
{
	if (1)
	{
		/* timeout present */
		int client_timeout = requestInfo.tag_present(1) ? requestInfo.get_expire_time(3) : default_expire_time();

		//log_debug("client api set timeout %d ms", client_timeout);
		struct timeval now;
		gettimeofday(&now, NULL);

		responseTimer = (int)(now.tv_sec * 1000000ULL + now.tv_usec);
		expire_time = now.tv_sec * 1000ULL + now.tv_usec / 1000 + client_timeout;
		timestamp = now.tv_sec;
	}

	// this Copy() may be failed
	int ret = DTCTask::Copy(rq, kptr);
	if (ret < 0)
		return ret;

	// inline from prepare_process()
	if ((requestFlags & DRequest::Flag::MultiKeyValue))
	{
		if (rq.kvl.key_fields() != key_fields())
		{
			set_error(-EC_KEY_NEEDED, "decoder", "key field count incorrect");
			return -EC_KEY_NEEDED;
		}
		if (rq.kvl.key_count() < 1)
		{
			set_error(-EC_KEY_NEEDED, "decoder", "require key value");
			return -EC_KEY_NEEDED;
		}
		// set batch key
		keyList = &rq.kvl;
	}
	else if (request_code() != DRequest::SvrAdmin)
	{
		if (build_packed_key() < 0)
			return -1; // ERROR
		calculate_barrier_key();
	}

	return 0;
}

#define ERR_RET(ret_msg, fmt, args...)     \
	do                                     \
	{                                      \
		set_error(err, "decoder", ret_msg); \
		log_debug(fmt, ##args);            \
		return err;                        \
	} while (0)
int DTCTask::Copy(NCRequest &rq, const DTCValue *kptr)
{
	NCServer *sv = rq.server;

	TableReference::set_table_definition(rq.tdef);
	stage = DecodeStageDone;
	role = TaskRoleServer;
	requestCode = rq.cmd;
	requestType = cmd2type[requestCode];

#define PASSING_FLAGS (             \
	DRequest::Flag::no_cache |       \
	DRequest::Flag::NoResult |      \
	DRequest::Flag::no_next_server |  \
	DRequest::Flag::MultiKeyValue | \
	0)

	requestFlags = DRequest::Flag::KeepAlive | (rq.flags & PASSING_FLAGS);

	// tablename & hash
	versionInfo.set_table_name(rq.tablename);
	if (rq.tdef)
		versionInfo.set_table_hash(rq.tdef->table_hash());
	versionInfo.set_serial_nr(sv->NextSerialNr());
	// app version
	if (sv->appname)
		versionInfo.set_tag(5, sv->appname);
	// lib version
	versionInfo.set_tag(6, "ctlib-v" DTC_VERSION);
	versionInfo.set_tag(9, rq.keytype);

	// hot backup id
	versionInfo.set_hot_backup_id(rq.hotBackupID);

	// hot backup timestamp
	versionInfo.set_master_hb_timestamp(rq.master_hb_timestamp);
	versionInfo.set_slave_hb_timestamp(rq.slave_hb_timestamp);
	if (sv->tdef && rq.adminCode != 0)
		versionInfo.set_data_table_hash(sv->tdef->table_hash());

	if (rq.flags & DRequest::Flag::MultiKeyValue)
	{
		kptr = NULL;
		if (sv->simple_batch_key() && rq.kvl.key_count() == 1)
		{
			/* single field single key batch, convert to normal */
			kptr = rq.kvl.val;
			requestFlags &= ~DRequest::Flag::MultiKeyValue;
		}
	}

	key = kptr;
	processFlags = PFLAG_ALLROWS;

	if (kptr)
	{
		requestInfo.set_key(*kptr);
	}
	// cmd
	if (sv->get_timeout())
	{
		requestInfo.set_timeout(sv->get_timeout());
	}
	//limit
	if (rq.limitCount)
	{
		requestInfo.set_limit_start(rq.limitStart);
		requestInfo.set_limit_count(rq.limitCount);
	}
	if (rq.adminCode > 0)
	{
		requestInfo.set_admin_code(rq.adminCode);
	}

	if (rq.fs.num_fields() > 0)
	{
		fieldList = new DTCFieldSet(rq.fs.num_fields());
		fieldList->Copy(rq.fs); // never failed
	}

	if (rq.ui.num_fields() > 0)
	{
		/* decode updateInfo */
		updateInfo = new DTCFieldValue(rq.ui.num_fields());
		const int mode = requestCode == DRequest::Update ? 2 : 1;
		int err = updateInfo->Copy(rq.ui, mode, rq.tdef);
		if (err < 0)
			ERR_RET("decode update info error", "decode update info error: %d", err);
	}

	if (rq.ci.num_fields() > 0)
	{
		/* decode conditionInfo */
		conditionInfo = new DTCFieldValue(rq.ci.num_fields());
		int err = conditionInfo->Copy(rq.ci, 0, rq.tdef);
		if (err < 0)
			ERR_RET("decode condition info error", "decode update info error: %d", err);
		if (err > 0)
		{
			clear_all_rows();
		}
	}

	return 0;
}

int DTCTask::process_internal_result(uint32_t ts)
{
	ResultPacket *rp = result_code() >= 0 ? get_result_packet() : NULL;

	if (rp)
	{
		resultInfo.set_total_rows(rp->totalRows);
	}
	else
	{
		resultInfo.set_total_rows(0);
		if (result_code() == 0)
			set_error(0, NULL, NULL);
	}

	if (ts)
	{
		resultInfo.set_time_info(ts);
	}
	versionInfo.set_tag(6, "ctlib-v" DTC_VERSION);
	if (result_key() == NULL && request_key() != NULL)
		set_result_key(*request_key());

	replyFlags = DRequest::Flag::KeepAlive | flag_multi_key_val();
	if (rp == NULL)
	{
		replyCode = DRequest::result_code;
	}
	else
	{
		replyCode = DRequest::DTCResultSet;
		DTCBinary v;
		v.ptr = rp->bc->data + rp->rowDataBegin;
		v.len = rp->bc->usedBytes - rp->rowDataBegin;
		if (rp->fieldSet)
			result = new ResultSet(*rp->fieldSet, table_definition());
		else
			result = new ResultSet((const uint8_t *)"", 0, num_fields(), table_definition());
		result->set_value_data(rp->numRows, v);
	}
	return 0;
}

int DTCTask::update_key_expire_time(int max)
{
	int fid = dataTableDef->expire_time_field_id();
	for (int i = 0; i < updateInfo->num_fields(); ++i)
	{
		if (updateInfo->field_id(i) == fid)
		{
			// found updateInfo with expire time, update it
			DTCValue *val = updateInfo->field_value(i);
			if (val->s64 > 0 && val->s64 <= max)
			{
				log_debug("key expire time: %ld", val->s64);
				val->s64 += time(NULL);
				log_debug("set key expire at time: %ld", val->s64);
			}
			else if (val->s64 == 0)
			{
				log_debug("key expire time: 0, never expire");
			}
			else
			{
				log_error("unknown key expire time");
				return -1;
			}
			break;
		}
	}
	return 0;
}
