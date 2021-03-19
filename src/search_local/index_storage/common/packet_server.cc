/*
 * =====================================================================================
 *
 *       Filename:  packet_erver.cc
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

#include "version.h"
#include "packet.h"
#include "table_def.h"
#include "decode.h"
#include "task_request.h"

#include "log.h"

/* not yet pollized*/
int Packet::encode_detect(const DTCTableDefinition *tdef, int sn)
{
	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = DRequest::Flag::KeepAlive;
	header.cmd = DRequest::Get;

	DTCVersionInfo vi;
	// tablename & hash
	vi.set_table_name(tdef->table_name());
	vi.set_table_hash(tdef->table_hash());
	vi.set_serial_nr(sn);
	// app version
	vi.set_tag(5, "dtcd");
	// lib version
	vi.set_tag(6, "ctlib-v" DTC_VERSION);
	vi.set_tag(9, tdef->field_type(0));

	DTCRequestInfo ri;
	// key
	ri.set_key(DTCValue::Make(0));
	//	ri.set_timeout(30);

	// field set
	char fs[4] = {1, 0, 0, 0xFF};

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(vi, DField::None);

	/* no table definition */
	header.len[DRequest::Section::table_definition] = 0;

	/* encode request info */
	header.len[DRequest::Section::RequestInfo] =
		encoded_bytes_simple_section(ri, tdef->key_type());

	/* no result info */
	header.len[DRequest::Section::ResultInfo] = 0;

	/* encode update info */
	header.len[DRequest::Section::UpdateInfo] = 0;

	/* encode condition info */
	header.len[DRequest::Section::ConditionInfo] = 0;

	/* full set */
	header.len[DRequest::Section::FieldSet] = 4;

	/* no result set */
	header.len[DRequest::Section::DTCResultSet] = 0;

	bytes = encode_header(header);
	const int len = bytes;

	/* exist and large enough, use. else free and malloc */
	int total_len = sizeof(BufferChain) + sizeof(struct iovec) + len;
	if (buf == NULL)
	{
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}
	else if (buf && buf->totalBytes < (int)(total_len - sizeof(BufferChain)))
	{
		FREE_IF(buf);
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}

	/* usedBtytes never used for Packet's buf */
	buf->nextBuffer = NULL;
	v = (struct iovec *)buf->data;
	nv = 1;
	char *p = buf->data + sizeof(struct iovec);
	v->iov_base = p;
	v->iov_len = len;

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, vi, DField::None);
	p = encode_simple_section(p, ri, tdef->key_type());

	// encode field set
	memcpy(p, fs, 4);
	p += 4;

	if (p - (char *)v->iov_base != len)
		fprintf(stderr, "%s(%d): BAD ENCODER len=%ld must=%d\n",
				__FILE__, __LINE__, (long)(p - (char *)v->iov_base), len);

	return 0;
}

int Packet::encode_reload_config(const DTCTableDefinition *tdef, int sn)
{
	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = DRequest::Flag::KeepAlive;
	header.cmd = DRequest::ReloadConfig;

	DTCVersionInfo vi;
	// tablename & hash
	vi.set_table_name(tdef->table_name());
	vi.set_table_hash(tdef->table_hash());
	vi.set_serial_nr(sn);
	// app version
	vi.set_tag(5, "dtcd");
	// lib version
	vi.set_tag(6, "ctlib-v" DTC_VERSION);
	vi.set_tag(9, tdef->field_type(0));

	DTCRequestInfo ri;
	// key
	ri.set_key(DTCValue::Make(0));
	//	ri.set_timeout(30);

	// field set
	char fs[4] = {1, 0, 0, 0xFF};

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(vi, DField::None);

	/* no table definition */
	header.len[DRequest::Section::table_definition] = 0;

	/* encode request info */
	header.len[DRequest::Section::RequestInfo] =
		encoded_bytes_simple_section(ri, tdef->key_type());

	/* no result info */
	header.len[DRequest::Section::ResultInfo] = 0;

	/* encode update info */
	header.len[DRequest::Section::UpdateInfo] = 0;

	/* encode condition info */
	header.len[DRequest::Section::ConditionInfo] = 0;

	/* full set */
	header.len[DRequest::Section::FieldSet] = 4;

	/* no result set */
	header.len[DRequest::Section::DTCResultSet] = 0;

	bytes = encode_header(header);
	const int len = bytes;

	/* pool, exist and large enough, use. else free and malloc */
	int total_len = sizeof(BufferChain) + sizeof(struct iovec) + len;
	if (buf == NULL)
	{
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}
	else if (buf && buf->totalBytes < (int)(total_len - sizeof(BufferChain)))
	{
		FREE_IF(buf);
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}

	/* usedBtytes never used for Packet's buf */
	buf->nextBuffer = NULL;
	v = (struct iovec *)buf->data;
	nv = 1;
	char *p = buf->data + sizeof(struct iovec);
	v->iov_base = p;
	v->iov_len = len;

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, vi, DField::None);
	p = encode_simple_section(p, ri, tdef->key_type());

	// encode field set
	memcpy(p, fs, 4);
	p += 4;

	if (p - (char *)v->iov_base != len)
		fprintf(stderr, "%s(%d): BAD ENCODER len=%ld must=%d\n",
				__FILE__, __LINE__, (long)(p - (char *)v->iov_base), len);

	return 0;
}

static char *EncodeBinary(char *p, const char *src, int len)
{
	if (len)
		memcpy(p, src, len);
	return p + len;
}

static char *EncodeBinary(char *p, const DTCBinary &b)
{
	return EncodeBinary(p, b.ptr, b.len);
}

int Packet::encode_fetch_data(TaskRequest &task)
{
	const DTCTableDefinition *tdef = task.table_definition();
	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = DRequest::Flag::KeepAlive;
	header.cmd = task.flag_fetch_data() ? DRequest::Get : task.request_code(); 

	// save & remove limit information
	uint32_t limitStart = task.requestInfo.limit_start();
	uint32_t limitCount = task.requestInfo.limit_count();
	if (task.request_code() != DRequest::Replicate) 
	{
		task.requestInfo.set_limit_start(0);
		task.requestInfo.set_limit_count(0);
	}

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(task.versionInfo, DField::None);

	/* no table definition */
	header.len[DRequest::Section::table_definition] = 0;

	/* encode request info */
	header.len[DRequest::Section::RequestInfo] =
		encoded_bytes_simple_section(task.requestInfo, tdef->key_type());

	/* no result info */
	header.len[DRequest::Section::ResultInfo] = 0;

	/* no update info */
	header.len[DRequest::Section::UpdateInfo] = 0;

	/* encode condition info */
	header.len[DRequest::Section::ConditionInfo] =
		encoded_bytes_multi_key(task.multi_key_array(), task.table_definition());

	/* full set */
	header.len[DRequest::Section::FieldSet] =
		tdef->packed_field_set(task.flag_field_set_with_key()).len;

	/* no result set */
	header.len[DRequest::Section::DTCResultSet] = 0;

	bytes = encode_header(header);
	const int len = bytes;

	/* pool, exist and large enough, use. else free and malloc */
	int total_len = sizeof(BufferChain) + sizeof(struct iovec) + len;
	if (buf == NULL)
	{
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}
	else if (buf && buf->totalBytes < total_len - (int)sizeof(BufferChain))
	{
		FREE_IF(buf);
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}

	buf->nextBuffer = NULL;
	v = (struct iovec *)buf->data;
	nv = 1;
	char *p = buf->data + sizeof(struct iovec);
	v->iov_base = p;
	v->iov_len = len;

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, task.versionInfo, DField::None);
	p = encode_simple_section(p, task.requestInfo, tdef->key_type());
	// restore limit info
	task.requestInfo.set_limit_start(limitStart);
	task.requestInfo.set_limit_count(limitCount);
	p = encode_multi_key(p, task.multi_key_array(), task.table_definition());
	p = EncodeBinary(p, tdef->packed_field_set(task.flag_field_set_with_key()));

	if (p - (char *)v->iov_base != len)
		fprintf(stderr, "%s(%d): BAD ENCODER len=%ld must=%d\n",
				__FILE__, __LINE__, (long)(p - (char *)v->iov_base), len);

	return 0;
}

int Packet::encode_pass_thru(DTCTask &task)
{
	const DTCTableDefinition *tdef = task.table_definition();
	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = DRequest::Flag::KeepAlive;
	header.cmd = task.request_code();

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(task.versionInfo, DField::None);

	/* no table definition */
	header.len[DRequest::Section::table_definition] = 0;

	/* encode request info */
	header.len[DRequest::Section::RequestInfo] =
		encoded_bytes_simple_section(task.requestInfo, tdef->key_type());

	/* no result info */
	header.len[DRequest::Section::ResultInfo] = 0;

	/* encode update info */
	header.len[DRequest::Section::UpdateInfo] =
		task.request_operation() ? encoded_bytes_field_value(*task.request_operation()) : 0;

	/* encode condition info */
	header.len[DRequest::Section::ConditionInfo] =
		task.request_condition() ? encoded_bytes_field_value(*task.request_condition()) : 0;

	/* full set */
	header.len[DRequest::Section::FieldSet] =
		task.request_fields() ? encoded_bytes_field_set(*task.request_fields()) : 0;

	/* no result set */
	header.len[DRequest::Section::DTCResultSet] = 0;

	bytes = encode_header(header);
	const int len = bytes;

	/* pool, exist and large enough, use. else free and malloc */
	int total_len = sizeof(BufferChain) + sizeof(struct iovec) + len;
	if (buf == NULL)
	{
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}
	else if (buf && buf->totalBytes < total_len - (int)sizeof(BufferChain))
	{
		FREE_IF(buf);
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}

	buf->nextBuffer = NULL;
	v = (struct iovec *)buf->data;
	nv = 1;
	char *p = buf->data + sizeof(struct iovec);
	v->iov_base = p;
	v->iov_len = len;

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, task.versionInfo, DField::None);
	p = encode_simple_section(p, task.requestInfo, tdef->key_type());
	if (task.request_operation())
		p = encode_field_value(p, *task.request_operation());
	if (task.request_condition())
		p = encode_field_value(p, *task.request_condition());
	if (task.request_fields())
		p = encode_field_set(p, *task.request_fields());

	if (p - (char *)v->iov_base != len)
		fprintf(stderr, "%s(%d): BAD ENCODER len=%ld must=%d\n",
				__FILE__, __LINE__, (long)(p - (char *)v->iov_base), len);

	return 0;
}

int Packet::encode_forward_request(TaskRequest &task)
{
	if (task.flag_pass_thru())
		return encode_pass_thru(task);
	if (task.flag_fetch_data())
		return encode_fetch_data(task);
	if (task.request_code() == DRequest::Get || task.request_code() == DRequest::Replicate)
		return encode_fetch_data(task);
	return encode_pass_thru(task);
}

int Packet::encode_result(DTCTask &task, int mtu, uint32_t ts)
{
	const DTCTableDefinition *tdef = task.table_definition();
	// rp指向返回数据集
	ResultPacket *rp = task.result_code() >= 0 ? task.get_result_packet() : NULL;
	BufferChain *rb = NULL;
	int nrp = 0, lrp = 0, off = 0;

	if (mtu <= 0)
	{
		mtu = MAXPACKETSIZE;
	}

	/* rp may exist but no result */
	if (rp && (rp->numRows || rp->totalRows))
	{
		//rb指向数据结果集缓冲区起始位置
		rb = rp->bc;
		if (rb)
			rb->Count(nrp, lrp);
		off = 5 - encoded_bytes_length(rp->numRows);
		encode_length(rb->data + off, rp->numRows);
		lrp -= off;
		task.resultInfo.set_total_rows(rp->totalRows);
	}
	else
	{
		if (rp && rp->totalRows == 0 && rp->bc)
		{
			FREE(rp->bc);
			rp->bc = NULL;
		}
		task.resultInfo.set_total_rows(0);
		if (task.result_code() == 0)
		{
			task.set_error(0, NULL, NULL);
		}
		//任务出现错误的时候，可能结果集里面还有值，此时需要将结果集的buffer释放掉
		else if (task.result_code() < 0)
		{
			ResultPacket *resultPacket = task.get_result_packet();
			if (resultPacket)
			{
				if (resultPacket->bc)
				{
					FREE(resultPacket->bc);
					resultPacket->bc = NULL;
				}
			}
		}
	}
	if (ts)
	{
		task.resultInfo.set_time_info(ts);
	}
	task.versionInfo.set_serial_nr(task.request_serial());
	task.versionInfo.set_tag(6, "ctlib-v" DTC_VERSION);
	if (task.result_key() == NULL && task.request_key() != NULL)
		task.set_result_key(*task.request_key());

	PacketHeader header;

	header.version = 1;
	header.scts = 8;
	header.flags = DRequest::Flag::KeepAlive | task.flag_multi_key_val();
	/* rp may exist but no result */
	header.cmd = (rp && (rp->numRows || rp->totalRows)) ? DRequest::DTCResultSet : DRequest::result_code;

	/* calculate version info */
	header.len[DRequest::Section::VersionInfo] =
		encoded_bytes_simple_section(task.versionInfo, DField::None);

	/* copy table definition */
	header.len[DRequest::Section::table_definition] =
		task.flag_table_definition() ? tdef->packed_definition().len : 0;

	/* no request info */
	header.len[DRequest::Section::RequestInfo] = 0;

	/* calculate result info */
	header.len[DRequest::Section::ResultInfo] =
		encoded_bytes_simple_section(task.resultInfo,
									 tdef->field_type(0));

	/* no update info */
	header.len[DRequest::Section::UpdateInfo] = 0;

	/* no condition info */
	header.len[DRequest::Section::ConditionInfo] = 0;

	/* no field set */
	header.len[DRequest::Section::FieldSet] = 0;

	/* copy result set */
	header.len[DRequest::Section::DTCResultSet] = lrp;

	bytes = encode_header(header);
	if (bytes > mtu)
	{
		/* clear result set */
		nrp = 0;
		lrp = 0;
		rb = NULL;
		rp = NULL;
		/* set message size error */
		task.set_error(-EMSGSIZE, "encode_result", "encoded result exceed the maximum network packet size");
		/* re-encode resultinfo */
		header.len[DRequest::Section::ResultInfo] =
			encoded_bytes_simple_section(task.resultInfo,
										 tdef->field_type(0));
		header.cmd = DRequest::result_code;
		header.len[DRequest::Section::DTCResultSet] = 0;
		/* FIXME: only work in LITTLE ENDIAN machine */
		bytes = encode_header(header);
	}

	//non-result packet len
	const int len = bytes - lrp;

	/* pool, exist and large enough, use. else free and malloc */
	int total_len = sizeof(BufferChain) + sizeof(struct iovec) * (nrp + 1) + len;
	if (buf == NULL)
	{
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}
	else if (buf && buf->totalBytes < total_len - (int)sizeof(BufferChain))
	{
		FREE_IF(buf);
		buf = (BufferChain *)MALLOC(total_len);
		if (buf == NULL)
		{
			return -ENOMEM;
		}
		buf->totalBytes = total_len - sizeof(BufferChain);
	}

	//发送实际数据集
	buf->nextBuffer = nrp ? rb : NULL;
	v = (struct iovec *)buf->data;
	char *p = buf->data + sizeof(struct iovec) * (nrp + 1);
	v->iov_base = p;
	v->iov_len = len;
	nv = nrp + 1;

	for (int i = 1; i <= nrp; i++, rb = rb->nextBuffer)
	{
		v[i].iov_base = rb->data + off;
		v[i].iov_len = rb->usedBytes - off;
		off = 0;
	}

	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	p = encode_simple_section(p, task.versionInfo, DField::None);
	if (task.flag_table_definition())
		p = EncodeBinary(p, tdef->packed_definition());
	p = encode_simple_section(p, task.resultInfo, tdef->field_type(0));

	if (p - (char *)v->iov_base != len)
		fprintf(stderr, "%s(%d): BAD ENCODER len=%ld must=%d\n",
				__FILE__, __LINE__, (long)(p - (char *)v->iov_base), len);

	return 0;
}

int Packet::encode_result(TaskRequest &task, int mtu)
{
	return encode_result((DTCTask &)task, mtu, task.Timestamp());
}

void Packet::free_result_buff()
{
	BufferChain *resbuff = buf->nextBuffer;
	buf->nextBuffer = NULL;

	while (resbuff)
	{
		char *p = (char *)resbuff;
		resbuff = resbuff->nextBuffer;
		FREE(p);
	}
}

int Packet::Bytes(void)
{
	int sendbytes = 0;
	for (int i = 0; i < nv; i++)
	{
		sendbytes += v[i].iov_len;
	}
	return sendbytes;
}
