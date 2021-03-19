/*
 * =====================================================================================
 *
 *       Filename:  dtc_req.cc
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <new>

#include "dtc_api.h"
#include "dtc_int.h"

// NCRequest's methods always assign and return
// cache the logical error code
#define return_err(x) return err = (x)
#define return_err_res(x, y, z) return new NCResult(x, y, z)
#define MAX_EXPIRETIME 30 * 24 * 3600
#define CLIENT_CURVE 5
#define AGENT_CURVE 8

NCRequest::NCRequest(NCServer *s, int op)
{
	compressFlag = 0;
	if (op != DRequest::Monitor && (s && s->is_completed() == 0)) //MonitorRequest need not table check
		s = NULL;
	server = s;
	if (server)
		server->INC();
	tdef = NULL;
	tablename = NULL;
	keytype = 0;

	switch (op)
	{
	case DRequest::Nop:
	case DRequest::Get:
	case DRequest::Purge:
	case DRequest::Flush:
	case DRequest::Insert:
	case DRequest::Update:
	case DRequest::Delete:
	case DRequest::Replace:
		if (server)
		{
			tdef = server->tdef;
			tablename = server->tablename;
			keytype = server->keytype;
		}
		break;
	//跨IDC分布支持
	case DRequest::SvrAdmin:
	case DRequest::Invalidate:
		if (server)
		{
			tdef = server->admin_tdef;
			tablename = (char *)"@HOT_BACKUP";
			keytype = DField::Unsigned;
		}
		break;
	case DRequest::Monitor:
		break;
	default:
		op = DRequest::result_code;
	}
	cmd = op;
	err = 0;
	haskey = 0;
	flags = 0;
	key.u64 = 0;
	key.bin.ptr = NULL;
	key.bin.len = 0;

	limitStart = 0;
	limitCount = 0;
	adminCode = 0;
	hotBackupID = 0;
	master_hb_timestamp = 0;
	slave_hb_timestamp = 0;
	if (server)
		kvl.keyinfo = &server->keyinfo;
	gzip = NULL;
}

NCRequest::~NCRequest(void)
{
	unset_key_value();
	unset_key();
	if (server)
		DEC_DELETE(server);
	if (gzip)
		DELETE(gzip);
}

// attach_server() error is transient, don't cache it
int NCRequest::attach_server(NCServer *s)
{
	// NO-OP
	if (s == server)
		return 0;

	switch (cmd)
	{
	case DRequest::Nop:
	case DRequest::Get:
	case DRequest::Purge:
	case DRequest::Flush:
	case DRequest::Insert:
	case DRequest::Update:
	case DRequest::Delete:
	case DRequest::Replace:
		break;
	default:
		// don't allow change admin target server
		return -EPERM;
	}

	/* new server must be initialized */
	if (s == NULL || s->is_completed() == 0)
		return -EC_NOT_INITIALIZED;

	/* precheck same table */
	if (server)
	{
		if (keytype != s->keytype)
			return -EC_BAD_KEY_TYPE;
		if (!server->keyinfo.Equal(s->keyinfo))
			return -EC_BAD_KEY_TYPE;
		if (strcmp(tablename, s->tablename) != 0)
			return -EC_BAD_TABLE_NAME;
		if (tdef == NULL)
		{
			// no current tabledef
		}
		else if (tdef == server->tdef)
		{
			// same tabledef
		}
		else if (tdef->is_same_table(server->tdef))
		{
			// hash equal
		}
		else
		{
			// force re-resolve fieldnames
			ui.Unresolve();
			ci.Unresolve();
			fs.Unresolve();
		}

		// release old server
		DEC_DELETE(server);
	}

	// switch to new server
	server = s;
	server->INC();
	tdef = server->tdef;
	tablename = server->tablename;
	keytype = server->keytype;
	kvl.keyinfo = &server->keyinfo;
	return 0;
}

int NCRequest::Need(const char *n, int vid)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (cmd != DRequest::Get && cmd != DRequest::SvrAdmin)
		return_err(-EC_BAD_OPERATOR);
	int ret = fs.add_field(n, vid);
	if (ret)
		err = ret;
	return ret;
}

int NCRequest::add_condition(const char *n, uint8_t op, uint8_t t, const DTCValue &v)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (cmd == DRequest::Insert || cmd == DRequest::Replace)
		return_err(-EC_BAD_OPERATOR);
	switch (t)
	{
	case DField::Signed:
	case DField::Unsigned:
		if (op >= DField::TotalComparison)
			return_err(-EC_BAD_OPERATOR);
		break;
	case DField::String:
	case DField::Binary:
		if (op != DField::EQ && op != DField::NE)
			return_err(-EC_BAD_OPERATOR);
		break;
	default:
		return_err(-EC_BAD_FIELD_TYPE);
	}
	int ret = ci.add_value(n, op, t, v);
	if (ret)
		err = ret;
	return ret;
}

int NCRequest::add_operation(const char *n, uint8_t op, uint8_t t, const DTCValue &v)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	switch (cmd)
	{
	case DRequest::Insert:
	case DRequest::Replace:
	case DRequest::SvrAdmin:
		if (op != DField::Set)
			return_err(-EC_BAD_OPERATOR);
		break;
	case DRequest::Update:
		break;
	default:
		return_err(-EC_BAD_OPERATOR);
		break;
	}

	switch (t)
	{
	case DField::Signed:
	case DField::Unsigned:
		if (op >= DField::TotalOperation)
			return_err(-EC_BAD_OPERATOR);
		break;

	case DField::String:
	case DField::Binary:
		if (op == DField::Add || op == DField::OR)
			return_err(-EC_BAD_OPERATOR);
		break;

	case DField::Float:
		if (op == DField::SetBits || op == DField::OR)
			return_err(-EC_BAD_OPERATOR);
		break;
	default:
		return_err(-EC_BAD_FIELD_TYPE);
	}
	int ret = ui.add_value(n, op, t, v);
	if (ret)
		err = ret;
	return ret;
}

int NCRequest::init_compress()
{
	int iret = 0;
	if (server == NULL)
		return -EC_NOT_INITIALIZED;
	if (tdef == NULL)
	{
		iret = server->ping(); //ping and get tabledef
		tdef = server->tdef;
	}
	if (iret)
		return iret;
	if (gzip == NULL)
		NEW(DTCCompress, gzip);
	if (gzip == NULL)
		return -ENOMEM;
	if (tdef == NULL)
		return -EC_CHECKSUM_MISMATCH;
	if (server->get_compress_level())
	{
		gzip->set_compress_level(server->get_compress_level());
	}
	return gzip->set_buffer_len(tdef->max_field_size());
}

int NCRequest::compress_set(const char *n, const char *v, int len)
{
	int iret = 0;
	if (len < 0)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldlen is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (n == NULL)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldname is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (v == NULL)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldvalue is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (gzip == NULL)
	{
		iret = init_compress();
		if (iret)
			return iret;
	}
	if (tdef->compress_field_id() <= 0)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:DTC must add a field for compress(field_type=2,field_size=8,default_value=compressflag)");
		return -EC_COMPRESS_ERROR;
	}

	if (tdef->field_type(tdef->field_id(n)) != DField::Binary)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:compress just support binary field");
		return -EC_BAD_VALUE_TYPE;
	}

	iret = gzip->compress(v, len);
	if (iret)
	{
		if (iret == -111111)
			snprintf(_errmsg, sizeof(_errmsg), "compress error:compress buff is null,sth sucks happend");
		else
			snprintf(_errmsg, sizeof(_errmsg), "compress error:zlib return code is %d.", iret);
		return -EC_COMPRESS_ERROR;
	}
	iret = set_compress_flag(n);
	if (iret)
		return iret;

	return add_operation(n, DField::Set, DField::String, DTCValue::Make(gzip->get_buf(), gzip->get_len()));
}

int NCRequest::compress_set_force(const char *n, const char *v, int len)
{
	int iret = 0;
	if (len < 0)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldlen is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (n == NULL)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldname is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (v == NULL)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:fieldvalue is invalid");
		return -EC_COMPRESS_ERROR;
	}

	if (gzip == NULL)
	{
		iret = init_compress();
		if (iret)
			return iret;
	}

	if (tdef->field_type(tdef->field_id(n)) != DField::Binary)
	{
		snprintf(_errmsg, sizeof(_errmsg), "compress error:compress just support binary field");
		return -EC_BAD_VALUE_TYPE;
	}

	iret = gzip->compress(v, len);
	if (iret)
	{
		if (iret == -111111)
			snprintf(_errmsg, sizeof(_errmsg), "compress error:compress buff is null,sth sucks happend");
		else
			snprintf(_errmsg, sizeof(_errmsg), "compress error:zlib return code is %d.", iret);
		return -EC_COMPRESS_ERROR;
	}
	if (iret)
		return iret;

	return add_operation(n, DField::Set, DField::String, DTCValue::Make(gzip->get_buf(), gzip->get_len()));
}

int NCRequest::add_value(const char *n, uint8_t t, const DTCValue &v)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (cmd != DRequest::Insert && cmd != DRequest::Replace)
		return_err(-EC_BAD_COMMAND);
	int ret = ui.add_value(n, DField::Set, t, v);
	if (ret)
		err = ret;
	return ret;
}

int NCRequest::unset_key(void)
{
	if (haskey)
	{
		if (server && server->keytype == DField::String)
		{

			DELETE_ARRAY(key.bin.ptr);
			key.Set(NULL, 0);
		}
		haskey = 0;
	}
	return 0;
}

int NCRequest::unset_key_value(void)
{
	kvl.Unset();
	flags &= ~DRequest::Flag::MultiKeyValue;
	return 0;
}

int NCRequest::set_key(int64_t k)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (server->keytype != DField::Signed)
		return_err(-EC_BAD_KEY_TYPE);
	key = k;
	haskey = 1;
	unset_key_value();
	return 0;
}

int NCRequest::set_key(const char *name, int l)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (server->keytype != DField::String)
		return_err(-EC_BAD_KEY_TYPE);
	char *a = new char[l];
	memcpy(a, name, l);
	DELETE_ARRAY(key.bin.ptr);
	haskey = 1;
	key.Set(a, l);
	unset_key_value();
	return 0;
}

int NCRequest::add_key_value(const char *name, const DTCValue &v, uint8_t type)
{
	if (server == NULL)
		return_err(-EC_NOT_INITIALIZED);
	if (server->allow_batch_key() == 0)
		return_err(-EC_NOT_INITIALIZED);

	int ret = kvl.add_value(name, v, type);
	if (ret < 0)
		return err = ret;

	flags |= DRequest::Flag::MultiKeyValue;
	unset_key();
	return 0;
}

int NCRequest::set_tab_def(void)
{
	int ret = 0;
	if (server)
	{
		switch (cmd)
		{
		case DRequest::Nop:
		case DRequest::Get:
		case DRequest::Purge:
		case DRequest::Flush:
		case DRequest::Insert:
		case DRequest::Update:
		case DRequest::Delete:
		case DRequest::Replace:
			ret = tdef != server->tdef;
			tdef = server->tdef;
			tablename = server->tablename;
			keytype = server->keytype;
			break;
		//跨IDC分布支持
		case DRequest::SvrAdmin:
		case DRequest::Invalidate:
			ret = tdef != server->admin_tdef;
			tdef = server->admin_tdef;
			tablename = (char *)"@HOT_BACKUP";
			keytype = DField::Unsigned;
			break;
		default:
			break;
		}
	}
	return ret;
}

int NCRequest::Encode(const DTCValue *kptr, Packet *pkt)
{
	int err;
	int force = set_tab_def();

	if (tdef && ((err = ui.Resolve(tdef, force)) ||
				 (err = ci.Resolve(tdef, force)) ||
				 (err = fs.Resolve(tdef, force))))
	{
		return err;
	}

	if ((err = pkt->encode_request(*this, kptr)) != 0)
	{
		return err;
	}
	return 0;
}

NCResult *NCRequest::execute_stream(const DTCValue *kptr)
{
	int resend = 1;
	int nrecv = 0;
	int nsent = 0;

	while (1)
	{
		int err;

		if (resend)
		{
			Packet pk;

			if ((err = Encode(kptr, &pk)) < 0)
			{
				return_err_res(err, "API::encoding", "client api encode error");
			}

			if ((err = server->Connect()) != 0)
			{
				if (err == -EAGAIN)
				{
					// unix socket return EAGAIN if listen queue overflow
					err = -EC_SERVER_BUSY;
				}
				return_err_res(err, "API::connecting", "client api connect server error");
			}

			nsent++;
			if ((err = server->send_packet_stream(pk)) != 0)
			{
				if (server->autoReconnect && nsent <= 1 && (err == -ECONNRESET || err == -EPIPE))
				{
					resend = 1;
					continue;
				}
				return_err_res(err, "API::sending", "client api send packet error");
			}
		}

		NCResult *res = new NCResult(tdef);
		res->versionInfo.set_serial_nr(server->LastSerialNr());

		err = server->decode_result_stream(*res);

		if (err < 0)
		{
			if (cmd != DRequest::Nop && err == -EAGAIN && nsent <= 1)
			{
				resend = 1;
				delete res;
				continue;
			}
			// network error always aborted
			return res;
		}
		nrecv++;

		if (res->versionInfo.serial_nr() != server->LastSerialNr())
		{
			log_debug("SN different, receive again. my SN: %lu, result SN: %lu",
					  (unsigned long)server->LastSerialNr(),
					  (unsigned long)res->versionInfo.serial_nr());
			// receive again
			resend = 0;
		}
		else if (res->result_code() == -EC_CHECKSUM_MISMATCH && nrecv <= 1)
		{
			resend = 1;
			// tabledef changed, resend
			//agent restart reconnect
			if (res->versionInfo.ReConnect())
			{
				server->Close();
			}
		}
		else
		{
			// got valid result
			if (res->result_code() >= 0 && cmd == DRequest::Get)
				res->set_virtual_map(fs);

			uint64_t time = 0;
			if (res->resultInfo.tag_present(7))
			{
				char *t = res->resultInfo.time_info();
				if (t)
				{
					DTCTimeInfo *t_info = (DTCTimeInfo *)t;
					time = (t_info->time) > 0 ? (t_info->time) : 0;
					server->set_agent_time(time);
				}
			}
			//agent restart reconnect
			if (res->versionInfo.ReConnect())
			{
				server->Close();
			}
			return res;
		}
		delete res; // delete invalid result and loop
	}

	// UNREACHABLE
	return NULL;
}

NCResult *NCRequest::execute_dgram(SocketAddress *peer, const DTCValue *kptr)
{
	int resend = 1;
	int nrecv = 0;
	int nsent = 0;

	while (1)
	{
		int err;

		if (resend)
		{
			Packet pk;

			if ((err = Encode(kptr, &pk)) < 0)
			{
				return_err_res(err, "API::encoding", "client api encode error");
			}

			nsent++;
			if ((err = server->send_packet_dgram(peer, pk)) != 0)
			{
				if (peer == NULL && server->autoReconnect && nsent <= 1 &&
					(err == -ECONNRESET || err == -EPIPE || err == -ECONNREFUSED || err == -ENOTCONN))
				{
					if ((err = server->Reconnect()) != 0)
					{
						if (err == -EAGAIN)
						{
							// unix socket return EAGAIN if listen queue overflow
							err = -EC_SERVER_BUSY;
						}
						return_err_res(err, "API::connecting", "client api connect server error");
					}
					resend = 1;
					continue;
				}
				return_err_res(err, "API::sending", "client api send packet error");
			}
		}

		NCResult *res = new NCResult(tdef);
		res->versionInfo.set_serial_nr(server->LastSerialNr());
		err = server->decode_result_dgram(peer, *res);
		if (err < 0)
		{
			// network error encountered
			return res;
		}
		nrecv++;

		if (res->versionInfo.serial_nr() != server->LastSerialNr())
		{
			log_debug("SN different, receive again. my SN: %lu, result SN: %lu",
					  (unsigned long)server->LastSerialNr(),
					  (unsigned long)res->versionInfo.serial_nr());
			// receive again
			resend = 0;
		}
		else if (res->result_code() == -EC_CHECKSUM_MISMATCH && nrecv <= 1)
		{
			resend = 1;
			// tabledef changed, resend
		}
		else
		{
			// got valid result
			if (res->result_code() >= 0 && cmd == DRequest::Get)
				res->set_virtual_map(fs);

			uint64_t time = 0;
			if (res->resultInfo.tag_present(7))
			{
				char *t = res->resultInfo.time_info();
				if (t)
				{
					DTCTimeInfo *t_info = (DTCTimeInfo *)t;
					time = (t_info->time) > 0 ? (t_info->time) : 0;
					server->set_agent_time(time);
				}
			}
			return res;
		}
		delete res; // delete invalid result and loop
	}

	/* UNREACHABLE */
	return NULL;
}

NCResult *NCRequest::execute_network(const DTCValue *kptr)
{
	NCResult *res = NULL;
	if (!server->is_dgram())
	{
		res = execute_stream(kptr);
	}
	else if (server->is_dgram())
	{
		NCUdpPort *port = server->get_global_port();
		//get UDPPORT{SN,fd} from udp pool
		if (port == NULL)
		{
			return_err_res(err, "API::execute", "get udp port error.udp port may exhaust");
		}
		res = execute_dgram(&server->addr, kptr);
		server->put_global_port(port);
	}
	else
	{
		int err;
		if ((err = server->Connect()) != 0)
		{
			if (err == -EAGAIN)
			{
				// unix socket return EAGAIN if listen queue overflow
				err = -EC_SERVER_BUSY;
			}
			return_err_res(err, "API::connecting", "client api connect server error");
		}

		res = execute_dgram(NULL, kptr);
	}
	return res;
}

NCResult *NCRequest::execute_internal(const DTCValue *kptr)
{
	if (tdef == NULL)
	{
		return_err_res(err, "API::encoding", "internal error: request has no tdef");
	}

	int force = set_tab_def();

	int err;
	if (
		(err = ui.Resolve(tdef, force)) ||
		(err = ci.Resolve(tdef, force)) ||
		(err = fs.Resolve(tdef, force)))
	{
		return_err_res(err, "API::encoding", "client api encode error");
	}

	NCResultInternal *res = server->execute_internal(*this, kptr);

	if (res == NULL)
	{
		return_err_res(err, "API::sending", "internal client api execute error");
	}

	if (res->result_code() >= 0 && cmd == DRequest::Get)
		res->set_virtual_map(fs);

	return reinterpret_cast<NCResult *>(res);
}

NCResult *NCRequest::pre_check(const DTCValue *kptr)
{

	if (cmd == DRequest::Monitor)
	{
		//Monitor Request need not check
		cmd = DRequest::SvrAdmin; //MonitorRequest is a stub of RequestSvrAdmin.Agent should add MonitorRequest type later
		return NULL;
	}
	if (err)
		return_err_res(err, "API::encoding", "Init Operation Error");
	if (server == NULL)
		return_err_res(-EC_NOT_INITIALIZED, "API::encoding", "Server Not Initialized");
	if (cmd == DRequest::result_code)
		return_err_res(-EC_BAD_COMMAND, "API::encoding", "Unknown Request Type");
	if (server->badkey)
		return_err_res(-EC_BAD_KEY_TYPE, "API::encoding", "Key Type Mismatch");
	if (server->badname)
		return_err_res(-EC_BAD_TABLE_NAME, "API::encoding", "Table Name Mismatch");
	if (cmd != DRequest::Insert && cmd != DRequest::SvrAdmin &&
		!(flags & DRequest::Flag::MultiKeyValue) && kptr == NULL)
		return_err_res(-EINVAL, "API::encoding", "Missing Key");
	if ((flags & DRequest::Flag::MultiKeyValue) && kvl.IsFlat() == 0)
		return_err_res(-EINVAL, "API::encoding", "Missing Key Value");
	if (tdef == NULL)
	{
		int ret = 0;
		uint64_t time_before = GET_TIMESTAMP();
		ret = server->ping(); //ping and get tabledef
		uint64_t time_after = GET_TIMESTAMP();
		tdef = server->tdef;

		uint64_t timeInterval = time_after - time_before;
		if (timeInterval < 0)
			timeInterval = 0;
		//log_info("timeInterval: %lu", timeInterval);
		std::string accessKey = server->accessToken;
		if (ret == -ETIMEDOUT)
		{
			server->dc->set_report_info(accessKey, CLIENT_CURVE, server->get_timeout());
			server->dc->set_report_info(accessKey, AGENT_CURVE, server->get_timeout());
		}
		else
		{
			server->dc->set_report_info(accessKey, CLIENT_CURVE, timeInterval);
			server->dc->set_report_info(accessKey, AGENT_CURVE, (server->get_agent_time() != 0) ? server->get_agent_time() : timeInterval);
		}
		log_info("NCRequest::pre_check,seq:%lu, clientTime:%lu, agentTime:%lu", server->LastSerialNr(), timeInterval, server->get_agent_time());
		// 清除  ping 之后获取的 agent-dtc 响应耗时
		server->set_agent_time(0);
	}
	return NULL;
}

int NCRequest::set_compress_field_name()
{
	int iret = 0;
	if (tdef && tdef->compress_field_id() > 0) //启用压缩必定有这个字段
	{
		if (gzip == NULL) //读请求需要need字段
		{
			if (cmd != DRequest::Get)
				return 0;
			iret = Need(tdef->field_name(tdef->compress_field_id()), 0);
			if (iret)
			{
				snprintf(_errmsg, sizeof(_errmsg), "need CompressField error,errorcode is %d", iret);
				return -1;
			}
		}
		else //写请求
		{
			if (cmd == DRequest::Insert || cmd == DRequest::Replace)
			{
				iret = add_operation(tdef->field_name(tdef->compress_field_id()), //name
									DField::Set, DField::Signed, DTCValue::Make(compressFlag));
			}
			else if (cmd == DRequest::Update)
			{
				iret = add_operation(tdef->field_name(tdef->compress_field_id()), //name
									DField::OR, DField::Signed, DTCValue::Make(compressFlag));
			}
			else
			{
				snprintf(_errmsg, sizeof(_errmsg), "Request type[%d] not support compressset", cmd);
				return -1;
			}
			if (iret)
			{
				snprintf(_errmsg, sizeof(_errmsg), "set CompressField error,errorcode is %d", iret);
				return -1;
			}
		}
	}
	return 0;
}
//if get,need compressfield,if write set compressfield;
NCResult *NCRequest::execute(const DTCValue *kptr)
{
	NCResult *ret = NULL;
	if (kptr == NULL && haskey)
		kptr = &key;
	ret = pre_check(kptr);
	if (ret == NULL)
	{
		if (set_compress_field_name())
			return_err_res(-EC_COMPRESS_ERROR, "API::execute", "set_compress_field_name error. please check request.error_message");
		;

		uint64_t time_before = GET_TIMESTAMP();
		if (server->has_internal_executor())
			ret = execute_internal(kptr);
		else
			ret = execute_network(kptr);
		uint64_t time_after = GET_TIMESTAMP();
		uint64_t timeInterval = time_after - time_before;
		if (timeInterval < 0)
			timeInterval = 0;
		std::string accessKey = server->accessToken;

		if (ret != NULL && ret->resultInfo.result_code() == -ETIMEDOUT)
		{
			server->dc->set_report_info(accessKey, CLIENT_CURVE, server->get_timeout());
			server->dc->set_report_info(accessKey, AGENT_CURVE, server->get_timeout());
		}
		else
		{
			server->dc->set_report_info(accessKey, CLIENT_CURVE, timeInterval);
			server->dc->set_report_info(accessKey, AGENT_CURVE, (server->get_agent_time() != 0) ? server->get_agent_time() : timeInterval);
		}
		std::string stemp = accessKey.substr(0, 8);
		uint32_t bid = 0;
		sscanf(stemp.c_str(), "%u", &bid);
		log_info("NCRequest::execute,seq:%lu, clientTime:%lu, agentTime:%lu, bid:%u", server->LastSerialNr(), timeInterval, server->get_agent_time(), bid);
		// 清除  解包 之后获取的 agent-dtc 响应耗时
		server->set_agent_time(0);
	}
	else
		log_info("NCRequest::execute,seq:%lu, pre_check return NULL", server->LastSerialNr());
	return ret;
}

NCResult *NCRequest::execute(int64_t k)
{
	if (server == NULL)
		return_err_res(-EC_NOT_INITIALIZED, "API::encoding", "Server Not Initialized");
	if (server->keytype != DField::Signed)
		return_err_res(-EC_BAD_KEY_TYPE, "API::encoding", "Key Type Mismatch");
	DTCValue v(k);
	return execute(&v);
}

NCResult *NCRequest::execute(const char *k, int l)
{
	if (server == NULL)
		return_err_res(-EC_NOT_INITIALIZED, "API::encoding", "Server Not Initialized");
	if (server->keytype != DField::String)
		return_err_res(-EC_BAD_KEY_TYPE, "API::encoding", "Key Type Mismatch");
	DTCValue v(k, l);
	return execute(&v);
}

// packet encoding don't cache error code
int NCRequest::encode_buffer(char *&ptr, int &len, int64_t &magic, const DTCValue *kptr)
{
	if (kptr == NULL && haskey)
		kptr = &key;
	int err = 0;
	NCResult *ret = pre_check(kptr);
	if (ret != NULL)
	{
		err = ret->result_code();
		delete ret;
		return err;
	}

	set_tab_def();
	if (tdef && ((err = ui.Resolve(tdef, 0)) ||
				 (err = ci.Resolve(tdef, 0)) ||
				 (err = fs.Resolve(tdef, 0))))
	{
		return err;
	}

	if ((err = Packet::encode_simple_request(*this, kptr, ptr, len)) != 0)
	{
		return err;
	}

	magic = server->LastSerialNr();
	return 0;
}

int NCRequest::encode_buffer(char *&ptr, int &len, int64_t &magic, int64_t k)
{
	if (server == NULL)
		return -EC_NOT_INITIALIZED;
	if (server->keytype != DField::Signed)
		return -EC_BAD_KEY_TYPE;
	if (cmd == DRequest::SvrAdmin)
		return -EC_BAD_COMMAND;
	DTCValue v(k);
	return encode_buffer(ptr, len, magic, &v);
}

int NCRequest::encode_buffer(char *&ptr, int &len, int64_t &magic, const char *k, int l)
{
	if (server == NULL)
		return -EC_NOT_INITIALIZED;
	if (server->keytype != DField::String)
		return -EC_BAD_KEY_TYPE;
	DTCValue v(k, l);
	return encode_buffer(ptr, len, magic, &v);
}
/*
 * @descr: 无源模式设置超时时间
 * @param: key, dtc key value
 * @param: t, 超时时间，相对时间
 */
int NCRequest::set_expire_time(const char *key, int t)
{
	int ret = 0;
	if (key == NULL)
	{
		log_error("invalid key value! key is null");
		return -EC_INVALID_KEY_VALUE;
	}
	if (t <= 0 || t > MAX_EXPIRETIME)
	{
		log_error("invalid expireTime! expireTime:%d", t);
		return -EC_INVALID_EXPIRETIME;
	}
	if (server->tdef == NULL)
	{
		ret = server->ping();
		if (ret != 0)
		{
			log_error("set_expire_time ping fail! ret:%d", ret);
		}
	}
	if (server->tdef == NULL)
	{
		log_error("set_expire_time get server tdef fail!");
		return -EC_EMPTY_TBDEF;
	}
	// field "_dtc_sys_expiretime" in array with index=1
	const char *fdname = server->tdef->field_name(1);
	if (strcmp(fdname, "_dtc_sys_expiretime"))
	{
		log_error("bussiness error: invalid option without field '_dtc_sys_expiretime', set_expire_time fail!");
		return -EC_BUSINESS_WITHOUT_EXPIRETIME;
	}
	DTC::Server srv;
	int kt = server->keytype;
	switch (kt)
	{
	case DField::Signed:
	case DField::Unsigned:
		ret = srv.int_key();
		break;
	case DField::String:
		ret = srv.string_key();
		break;
	case DField::Binary:
		ret = srv.binary_key();
		break;
	default:
		break;
	}
	const char *address = server->get_address();
	const char *tbname = server->get_table_name();
	int timeout = server->get_timeout();
	ret = srv.set_address(address);
	ret = srv.set_table_name(tbname);
	srv.set_access_key(server->accessToken.c_str());
	srv.set_timeout(timeout);
	DTC::UpdateRequest req(&srv);
	switch (kt)
	{
	case DField::Signed:
	case DField::Unsigned:
		ret = req.set_key(atoi(key));
		break;
	case DField::String:
	case DField::Binary:
		ret = req.set_key(key);
		break;
	default:
		break;
	}

	ret = req.Set("_dtc_sys_expiretime", t);
	DTC::Result rst;
	ret = req.execute(rst);
	if (ret != 0)
	{
		log_error("set expireTime fail, errmsg:%s, errfrom:%s", rst.error_message(), rst.error_from());
	}
	return ret;
}

/*
 * @descr: 无源模式获取超时时间
 * @param: key, dtc key value
 */
int NCRequest::get_expire_time(const char *key)
{
	int ret = 0;
	if (key == NULL)
	{
		log_error("invalid key value! key is null");
		return -EC_INVALID_KEY_VALUE;
	}
	if (server->tdef == NULL)
	{
		ret = server->ping();
		if (ret != 0)
		{
			log_error("get_expire_time ping fail! ret:%d", ret);
		}
	}
	if (server->tdef == NULL)
	{
		log_error("get_expire_time get server tdef fail!");
		return -EC_EMPTY_TBDEF;
	}
	// field "_dtc_sys_expiretime" in array with index=1
	const char *fdname = server->tdef->field_name(1);
	if (strcmp(fdname, "_dtc_sys_expiretime"))
	{
		log_error("bussiness error: invalid option without field '_dtc_sys_expiretime', get_expire_time fail!");
		return -EC_BUSINESS_WITHOUT_EXPIRETIME;
	}
	DTC::Server srv;
	int kt = server->keytype;
	switch (kt)
	{
	case DField::Signed:
	case DField::Unsigned:
		ret = srv.int_key();
		break;
	case DField::String:
		ret = srv.string_key();
		break;
	case DField::Binary:
		ret = srv.binary_key();
		break;
	default:
		break;
	}
	const char *address = server->get_address();
	const char *tbname = server->get_table_name();
	int timeout = server->get_timeout();
	ret = srv.set_address(address);
	ret = srv.set_table_name(tbname);
	srv.set_access_key(server->accessToken.c_str());
	srv.set_timeout(timeout);
	DTC::GetRequest req(&srv);
	switch (kt)
	{
	case DField::Signed:
	case DField::Unsigned:
		ret = req.set_key(atoi(key));
		break;
	case DField::String:
	case DField::Binary:
		ret = req.set_key(key);
		break;
	default:
		break;
	}
	ret = req.Need("_dtc_sys_expiretime");
	DTC::Result rst;
	ret = req.execute(rst);
	if (ret != 0)
	{
		log_error("get expireTime fail, errmsg:%s, errfrom:%s", rst.error_message(), rst.error_from());
		return ret;
	}
	if (rst.num_rows() <= 0)
	{
		log_error("get expireTime fail, no data exist in dtc for key:%s", key);
		return -EC_GET_EXPIRETIME_END_OF_RESULT;
	}
	ret = rst.fetch_row();
	if (ret < 0)
	{
		log_error("get expireTime fail, fetch_row error, errmsg:%s, errfrom:%s", rst.error_message(), rst.error_from());
		return ret;
	}
	int expiretime = 0;
	expiretime = rst.int_value("_dtc_sys_expiretime");
	return expiretime;
}
