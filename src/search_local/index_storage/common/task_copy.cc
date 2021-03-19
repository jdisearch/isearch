/*
 * =====================================================================================
 *
 *       Filename:  task_copy.cc
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

#include "decode.h"
#include "protocol.h"
#include "buffer_error.h"
#include "log.h"
#include "task_request.h"
#include "task_pkey.h"

// all Copy() only copy raw request state,
// don't copy in-process data

// vanilla Copy(), un-used yet
int DTCTask::Copy(const DTCTask &rq)
{
	//straight member-by-member copy or duplicate
	TableReference::set_table_definition(rq.table_definition());
	stage = DecodeStageDone;
	role = TaskRoleServer;

	versionInfo.Copy(rq.versionInfo);
	requestInfo.Copy(rq.requestInfo);
	resultInfo.Copy(rq.resultInfo);

	key = rq.request_key();
	requestCode = rq.requestCode;
	requestType = rq.requestType;
	requestFlags = rq.requestFlags;
	processFlags = rq.processFlags & (PFLAG_ALLROWS | PFLAG_FIELDSETWITHKEY | PFLAG_PASSTHRU);
	if (table_definition())
		table_definition()->INC();
	fieldList = rq.fieldList ? new DTCFieldSet(*rq.fieldList) : NULL;
	updateInfo = rq.updateInfo ? new DTCFieldValue(*rq.updateInfo) : NULL;
	conditionInfo = rq.conditionInfo ? new DTCFieldValue(*rq.conditionInfo) : NULL;

	return 0;
}

// vanilla Copy(), un-used yet
int TaskRequest::Copy(const TaskRequest &rq)
{
	DTCTask::Copy(rq); // always success
	expire_time = rq.expire_time;
	timestamp = rq.timestamp;

	// don't use generic build_packed_key, copy manually
	barHash = rq.barHash;
	// Dup multi-fields key
	if (rq.multiKey)
	{
		multiKey = (DTCValue *)MALLOC(key_fields() * sizeof(DTCValue));
		if (multiKey == NULL)
			throw -ENOMEM;
		memcpy(multiKey, rq.multiKey, key_fields() * sizeof(DTCValue));
	}
	// Dup packed key
	if (rq.packedKey)
	{
		int pksz = TaskPackedKey::packed_key_size(packedKey, key_format());
		packedKey = (char *)MALLOC(pksz);
		if (packedKey == NULL)
			throw -ENOMEM;
		memcpy(packedKey, rq.packedKey, pksz);
	}
	// Copy internal API batch key list
	keyList = rq.internal_key_val_list();
	return 0;
}

// only for batch request spliting
int DTCTask::Copy(const DTCTask &rq, const DTCValue *newkey)
{
	// copy non-field depend informations, see above Copy() variant
	TableReference::set_table_definition(rq.table_definition());
	stage = DecodeStageDone;
	role = TaskRoleServer;
	versionInfo.Copy(rq.versionInfo);
	//如果是批量请求，拷贝批量task的versioninfo之后记得强制set一下keytype
	//因为有些老的api发过来的请求没有设置全局的keytype
	//这样批量的时候可能会在heper端出现-2024错误
	if (((TaskRequest *)&rq)->is_batch_request())
	{
		if (table_definition())
			versionInfo.set_key_type(table_definition()->key_type());
		else
			log_notice("table_definition() is null.please check it");
	}
	requestInfo.Copy(rq.requestInfo);
	resultInfo.Copy(rq.resultInfo);
	key = newkey;
	requestCode = rq.requestCode;
	requestType = rq.requestType;
	requestFlags = rq.requestFlags & ~DRequest::Flag::MultiKeyValue;
	processFlags = rq.processFlags & (PFLAG_ALLROWS | PFLAG_FIELDSETWITHKEY | PFLAG_PASSTHRU);
	if (table_definition())
		table_definition()->INC();

	// Need() field list always same
	fieldList = rq.fieldList ? new DTCFieldSet(*rq.fieldList) : NULL;

	// primary key set to first key component
	requestInfo.set_key(newkey[0]);

	// k1 means key field number minus 1
	const int k1 = rq.key_fields() - 1;
	if (k1 <= 0)
	{
		// single-field key, copy straight
		updateInfo = rq.updateInfo ? new DTCFieldValue(*rq.updateInfo) : NULL;
		conditionInfo = rq.conditionInfo ? new DTCFieldValue(*rq.conditionInfo) : NULL;
	}
	else if (rq.requestCode == DRequest::Insert || rq.requestCode == DRequest::Replace)
	{
		// multi-field key, insert or replace
		conditionInfo = rq.conditionInfo ? new DTCFieldValue(*rq.conditionInfo) : NULL;

		// enlarge update information, add non-primary key using Set() operator
		updateInfo = rq.updateInfo ? new DTCFieldValue(*rq.updateInfo, k1) : new DTCFieldValue(k1);
		for (int i = 1; i <= k1; i++)
		{ // last one is k1
			updateInfo->add_value(i, DField::Set, /*field_type(i)*/ DField::Signed, newkey[i]);
			// FIXME: server didn't support non-integer multi-field key
		}
	}
	else
	{
		// multi-field key, other commands
		updateInfo = rq.updateInfo ? new DTCFieldValue(*rq.updateInfo) : NULL;

		// enlarge condition information, add non-primary key using EQ() comparator
		conditionInfo = rq.conditionInfo ? new DTCFieldValue(*rq.conditionInfo, k1) : new DTCFieldValue(k1);
		for (int i = 1; i <= k1; i++)
		{ // last one is k1
			conditionInfo->add_value(i, DField::EQ, /*field_type(i)*/ DField::Signed, newkey[i]);
			// FIXME: server didn't support non-integer multi-field key
		}
	}

	return 0;
}

int TaskRequest::Copy(const TaskRequest &rq, const DTCValue *newkey)
{
	DTCTask::Copy(rq, newkey); // always success
	expire_time = rq.expire_time;
	timestamp = rq.timestamp;
	// splited, now NOT batch request
	// re-calculate packed and barrier key
	if (request_code() != DRequest::SvrAdmin)
	{
		if (build_packed_key() < 0)
			return -1; // ERROR
		calculate_barrier_key();
	}
	return 0;
}

// only for commit row, replace whole row
int DTCTask::Copy(const RowValue &r)
{
	TableReference::set_table_definition(r.table_definition());
	stage = DecodeStageDone;
	role = TaskRoleServer;
	requestCode = DRequest::Replace;
	requestType = cmd2type[requestCode];
	requestFlags = DRequest::Flag::KeepAlive;
	processFlags = PFLAG_ALLROWS | PFLAG_FIELDSETWITHKEY;

	const int n = r.num_fields();

	// dup strings&binary field value to packetbuf
	DTCValue row[n + 1];
	int len = 0;
	for (int i = 0; i <= n; i++)
	{
		int t = r.field_type(i);
		if (t == DField::String || t == DField::Binary)
			len += r[i].bin.len + 1;
	}
	char *p = packetbuf.Allocate(len, role);
	for (int i = 0; i <= n; i++)
	{
		int t = r.field_type(i);
		if (t == DField::String || t == DField::Binary)
		{
			int l = r[i].bin.len;
			row[i].bin.len = l;
			row[i].bin.ptr = p;
			memcpy(p, r[i].bin.ptr, l);
			p[l] = 0;
			p += l + 1;
		}
		else
		{
			row[i] = r[i];
		}
	}

	// tablename & hash
	versionInfo.set_table_name(table_name());
	versionInfo.set_table_hash(table_hash());
	versionInfo.set_serial_nr(0);
	versionInfo.set_tag(9, key_type());

	// key
	requestInfo.set_key(row[0]);
	// cmd
	requestInfo.set_tag(1, DRequest::Replace);

	// a replace command with all fields set to desired value
	updateInfo = new DTCFieldValue(n);
	for (int i = 1; i <= n; i++)
		updateInfo->add_value(i, DField::Set,
							 //api cast all uint to long long,and set fieldtype to uint
							 //so here should be consistent
							 (r.field_type(i) == DField::Unsigned) ? DField::Signed : r.field_type(i),
							 row[i]);

	// first field always is key
	// bug fix, key should not point to local row[0]
	key = requestInfo.Key();
	;

	return 0;
}

int TaskRequest::Copy(const RowValue &row)
{
	DTCTask::Copy(row); // always success

	// ALWAYS a replace request
	// calculate packed and barrier key
	if (build_packed_key() < 0)
		return -1; // ERROR
	calculate_barrier_key();
	return 0;
}
