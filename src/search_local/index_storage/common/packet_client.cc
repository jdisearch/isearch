/*
 * =====================================================================================
 *
 *       Filename:  packet_client.cc
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
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <new>

#include "value.h"
#include "section.h"
#include "protocol.h"
#include "version.h"
#include "packet.h"
#include "../api/c_api_cc/dtcint.h"
#include "table_def.h"
#include "decode.h"

#include "log.h"

template <class T>
int Templateencode_request(NCRequest &rq, const DTCValue *kptr, T *tgt)
{
	NCServer *sv = rq.server;
	int key_type = rq.keytype;
	const char *tab_name = rq.tablename;

	const char *accessKey = sv->accessToken.c_str();

	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = rq.tdef ? DRequest::Flag::KeepAlive : DRequest::Flag::KeepAlive + DRequest::Flag::NeedTableDefinition;
	header.flags |= (rq.flags & (DRequest::Flag::no_cache | DRequest::Flag::NoResult | DRequest::Flag::no_next_server | DRequest::Flag::MultiKeyValue));
	header.cmd = rq.cmd;

	DTCVersionInfo vi;
	// tablename & hash
	vi.set_table_name(tab_name);
	if (rq.tdef)
		vi.set_table_hash(rq.tdef->table_hash());
	vi.set_serial_nr(sv->NextSerialNr());
	// app version
	if (sv->appname)
		vi.set_tag(5, sv->appname);
	// lib version
	vi.set_tag(6, "ctlib-v" DTC_VERSION);
	vi.set_tag(9, key_type);

	// hot backup id
	vi.set_hot_backup_id(rq.hotBackupID);

	// hot backup timestamp
	vi.set_master_hb_timestamp(rq.MasterHBTimestamp);
	vi.set_slave_hb_timestamp(rq.SlaveHBTimestamp);
	if (sv->tdef && rq.adminCode != 0)
		vi.set_data_table_hash(sv->tdef->table_hash());

	// accessKey
	vi.set_access_key(accessKey);

	Array kt(0, NULL);
	Array kn(0, NULL);
	Array kv(0, NULL);
	int isbatch = 0;
	if (rq.flags & DRequest::Flag::MultiKeyValue)
	{
		if (sv->SimpleBatchKey() && rq.kvl.KeyCount() == 1)
		{
			/* single field single key batch, convert to normal */
			kptr = rq.kvl.val;
			header.flags &= ~DRequest::Flag::MultiKeyValue;
		}
		else
		{
			isbatch = 1;
		}
	}

	if (isbatch)
	{
		int keyFieldCnt = rq.kvl.KeyFields();
		int keyCount = rq.kvl.KeyCount();
		int i, j;
		vi.set_tag(10, keyFieldCnt);
		vi.set_tag(11, keyCount);
		// key type
		kt.ptr = (char *)MALLOC(sizeof(uint8_t) * keyFieldCnt);
		if (kt.ptr == NULL)
			throw std::bad_alloc();
		for (i = 0; i < keyFieldCnt; i++)
			kt.Add((uint8_t)(rq.kvl.KeyType(i)));
		vi.set_tag(12, DTCValue::Make(kt.ptr, kt.len));
		// key name
		kn.ptr = (char *)MALLOC((256 + sizeof(uint32_t)) * keyFieldCnt);
		if (kn.ptr == NULL)
			throw std::bad_alloc();
		for (i = 0; i < keyFieldCnt; i++)
			kn.Add(rq.kvl.KeyName(i));
		vi.set_tag(13, DTCValue::Make(kn.ptr, kn.len));
		// key value
		unsigned int buf_size = 0;
		for (j = 0; j < keyCount; j++)
		{
			for (i = 0; i < keyFieldCnt; i++)
			{
				DTCValue &v = rq.kvl(j, i);
				switch (rq.kvl.KeyType(i))
				{
				case DField::Signed:
				case DField::Unsigned:
					if (buf_size < kv.len + sizeof(uint64_t))
					{
						if (REALLOC(kv.ptr, buf_size + 256) == NULL)
							throw std::bad_alloc();
						buf_size += 256;
					}
					kv.Add(v.u64);
					break;
				case DField::String:
				case DField::Binary:
					if (buf_size < (unsigned int)kv.len + sizeof(uint32_t) + v.bin.len)
					{
						if (REALLOC(kv.ptr, buf_size + sizeof(uint32_t) + v.bin.len) == NULL)
							throw std::bad_alloc();
						buf_size += sizeof(uint32_t) + v.bin.len;
					}
					kv.Add(v.bin.ptr, v.bin.len);
					break;
				default:
					break;
				}
			}
		}
	}

	DTCRequestInfo ri;
	// key
	if (isbatch)
		ri.set_key(DTCValue::Make(kv.ptr, kv.len));
	else if (kptr)
		ri.set_key(*kptr);
	// cmd
	if (sv->GetTimeout())
	{
		ri.set_timeout(sv->GetTimeout());
	}
	//limit
	if (rq.limitCount)
	{
		ri.set_limit_start(rq.limitStart);
		ri.set_limit_count(rq.limitCount);
	}
	if (rq.adminCode > 0)
	{
		ri.set_admin_code(rq.adminCode);
	}

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(vi, DField::None);

	//log_info("header.len:%d, vi.access_key:%s, vi.access_key().len:%d", header.len[DRequest::Section::VersionInfo], vi.access_key().ptr, vi.access_key().len);

	/* no table definition */
	header.len[DRequest::Section::table_definition] = 0;

	/* calculate rq info */
	header.len[DRequest::Section::RequestInfo] =
		encoded_bytes_simple_section(ri, isbatch ? DField::String : key_type);

	/* no result info */
	header.len[DRequest::Section::ResultInfo] = 0;

	/* copy update info */
	header.len[DRequest::Section::UpdateInfo] =
		encoded_bytes_field_value(rq.ui);

	/* copy condition info */
	header.len[DRequest::Section::ConditionInfo] =
		encoded_bytes_field_value(rq.ci);

	/* full set */
	header.len[DRequest::Section::FieldSet] =
		encoded_bytes_field_set(rq.fs);

	/* no result set */
	header.len[DRequest::Section::DTCResultSet] = 0;

	const int len = Packet::encode_header(header);
	char *p = tgt->allocate_simple(len);

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, vi, DField::None);
	p = encode_simple_section(p, ri, isbatch ? DField::String : key_type);
	p = encode_field_value(p, rq.ui);
	p = encode_field_value(p, rq.ci);
	p = encode_field_set(p, rq.fs);

	FREE(kt.ptr);
	FREE(kn.ptr);
	FREE(kv.ptr);

	return 0;
}

char *Packet::allocate_simple(int len)
{
	buf = (BufferChain *)MALLOC(sizeof(BufferChain) + sizeof(struct iovec) + len);
	if (buf == NULL)
		throw std::bad_alloc();

	buf->nextBuffer = NULL;
	/* never use usedBytes here */
	buf->totalBytes = sizeof(struct iovec) + len;
	v = (struct iovec *)buf->data;
	nv = 1;
	char *p = buf->data + sizeof(struct iovec);
	v->iov_base = p;
	v->iov_len = len;
	bytes = len;
	return p;
}

int Packet::encode_request(NCRequest &rq, const DTCValue *kptr)
{
	return Templateencode_request(rq, kptr, this);
}

class SimpleBuffer : public DTCBinary
{
public:
	char *allocate_simple(int size);
};

char *SimpleBuffer::allocate_simple(int size)
{
	len = size;
	ptr = (char *)MALLOC(len);
	if (ptr == NULL)
		throw std::bad_alloc();
	return ptr;
}

int Packet::encode_simple_request(NCRequest &rq, const DTCValue *kptr, char *&ptr, int &len)
{
	SimpleBuffer buf;
	int ret = Templateencode_request(rq, kptr, &buf);
	ptr = buf.ptr;
	len = buf.len;
	return ret;
}
