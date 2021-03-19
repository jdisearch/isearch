/*
 * =====================================================================================
 *
 *       Filename:  dtc_int.h
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
#ifndef __CHC_CLI_H
#define __CHC_CLI_H

#include <table_def.h>
#include <field_api.h>
#include <packet.h>
#include <unistd.h>
#include <buffer_error.h>
#include <log.h>
#include "key_list.h"
#include <task_request.h>
#include <container.h>
#include <net_addr.h>
#include "udp_pool.h"
#include "compress.h"
#include "poll_thread.h"
#include "lock.h"
#include <map>

/*
 * Goal:
 * 	single transation (*)
 * 	batch transation (*)
 * 	async transation
 */

class DTCTask;
class Packet;

class NCResult;
class NCResultInternal;
class NCPool;
struct NCTransation;
class IDTCTaskExecutor;
class IDTCService;

class NCBase
{
private:
	AtomicU32 _count;

public:
	NCBase(void) {}
	~NCBase(void) {}
	int INC(void) { return ++_count; }
	int DEC(void) { return --_count; }
	int CNT(void) { return _count.get(); };
};

class NCRequest;
class DataConnector;
class NCServer : public NCBase
{
public: // global server state
	NCServer();
	NCServer(const NCServer &);
	~NCServer(void);

	// base settings
	SocketAddress addr;
	char *tablename;
	char *appname;

	static DataConnector *dc;

	//for compress
	void set_compress_level(int level) { compressLevel = level; }
	int get_compress_level(void) { return compressLevel; }

	//dtc set _server_address and _server_tablename for plugin
	static char *_server_address;
	static char *_server_tablename;

	int keytype;
	int autoUpdateTable;
	int autoReconnect;
	static int _network_mode;
	NCKeyInfo keyinfo;

	void clone_tab_def(const NCServer &source);
	int set_address(const char *h, const char *p = NULL);
	int set_table_name(const char *);
	const char *get_address(void) const { return addr.Name(); }			 //this addres is set by user
	const char *get_server_address(void) const { return _server_address; } //this address is set by dtc
	const char *get_server_table_name(void) const { return _server_tablename; }
	int is_dgram(void) const { return addr.socket_type() == SOCK_DGRAM; }
	int is_udp(void) const { return addr.socket_type() == SOCK_DGRAM && addr.socket_family() == AF_INET; }
	const char *get_table_name(void) const { return tablename; }
	int int_key(void);
	int string_key(void);
	int field_type(const char *);
	int is_completed(void) const { return completed; }
	int add_key(const char *name, uint8_t type);
	int key_field_cnt(void) const { return keyinfo.key_fields() ?: keytype != DField::None ? 1 : 0; }
	int allow_batch_key(void) const { return keyinfo.key_fields(); }
	int simple_batch_key(void) const { return keyinfo.key_fields() == 1 && keytype == keyinfo.key_type(0); }

	void set_auto_update_tab(bool autoUpdate) { autoUpdateTable = autoUpdate ? 1 : 0; }
	void set_auto_reconnect(int reconnect) { autoReconnect = reconnect; }

	// error state
	unsigned completed : 1;
	unsigned badkey : 1;
	unsigned badname : 1;
	unsigned autoping : 1;
	const char *errstr;

	const char *error_message(void) const { return errstr; }

	// table definition
	DTCTableDefinition *tdef;
	DTCTableDefinition *admin_tdef;

	void save_definition(NCResult *);
	DTCTableDefinition *get_tab_def(int cmd) const;

	std::string accessToken;
	int set_access_key(const char *token);

private: // serialNr manupulation
	uint64_t lastSN;

public:
	uint64_t NextSerialNr(void)
	{
		++lastSN;
		if (lastSN == 0)
			lastSN++;
		return lastSN;
	}
	uint64_t LastSerialNr(void) { return lastSN; }

private: // timeout settings
	int timeout;
	int realtmo;

public:
	void set_m_timeout(int n);
	int get_timeout(void) const { return timeout; }

private:
	int compressLevel;

private: // sync execution
	IDTCService *iservice;
	IDTCTaskExecutor *executor;
	int netfd;
	time_t lastAct;
	NCRequest *pingReq;

private:
	uint64_t agentTime;

public:
	void set_agent_time(int t) { agentTime = t; }
	uint64_t get_agent_time(void) { return agentTime; }

public:
	int Connect(void);
	int Reconnect(void);
	void Close(void);
	void SetFD(int fd)
	{
		Close();
		netfd = fd;
		update_timeout();
	}
	// stream, connected
	int send_packet_stream(Packet &);
	int decode_result_stream(NCResult &);
	// dgram, connected or connectless
	int send_packet_dgram(SocketAddress *peer, Packet &);
	int decode_result_dgram(SocketAddress *peer, NCResult &);
	// connectless
	NCUdpPort *get_global_port(void);
	void put_global_port(NCUdpPort *);

	void TryPing(void);
	int ping(void);
	void auto_ping(void)
	{
		if (!is_dgram())
			autoping = 1;
	}
	NCResultInternal *execute_internal(NCRequest &rq, const DTCValue *kptr) { return executor->task_execute(rq, kptr); }
	int has_internal_executor(void) const { return executor != 0; }

private:
	int bind_temp_unix_socket(void);
	void update_timeout(void);
	// this method is weak, and don't exist in libdtc.a
	__attribute__((__weak__)) void check_internal_service(void);

public: // transation manager, impl at dtcpool.cc
	int async_connect(int &);
	NCPool *ownerPool;
	int ownerId;
	void set_owner(NCPool *, int);

	NCResult *decode_buffer(const char *, int);
	static int check_packet_size(const char *, int);
};

class NCRequest
{
public:
	NCServer *server;
	uint8_t cmd;
	uint8_t haskey;
	uint8_t flags;
	int err;
	DTCValue key;
	NCKeyValueList kvl;
	FieldValueByName ui;
	FieldValueByName ci;
	FieldSetByName fs;

	DTCTableDefinition *tdef;
	char *tablename;
	int keytype;

	unsigned int limitStart;
	unsigned int limitCount;
	int adminCode;

	uint64_t hotBackupID;
	uint64_t master_hb_timestamp;
	uint64_t slave_hb_timestamp;

public:
	NCRequest(NCServer *, int op);
	~NCRequest(void);
	int attach_server(NCServer *);

	void enable_no_cache(void) { flags |= DRequest::Flag::no_cache; }
	void enable_no_next_server(void) { flags |= DRequest::Flag::no_next_server; }
	void enable_no_result(void) { flags |= DRequest::Flag::NoResult; }
	int add_condition(const char *n, uint8_t op, uint8_t t, const DTCValue &v);
	int add_operation(const char *n, uint8_t op, uint8_t t, const DTCValue &v);
	int compress_set(const char *n, const char *v, int len);
	int compress_set_force(const char *n, const char *v, int len);
	int add_value(const char *n, uint8_t t, const DTCValue &v);
	int Need(const char *n, int);
	void limit(unsigned int st, unsigned int cnt)
	{
		if (cnt == 0)
			st = 0;
		limitStart = st;
		limitCount = cnt;
	}

	int set_key(int64_t);
	int set_key(const char *, int);
	int unset_key(void);
	int unset_key_value(void);
	int field_type(const char *name) { return server ? server->field_type(name) : DField::None; }
	int add_key_value(const char *name, const DTCValue &v, uint8_t type);
	int set_cache_id(int dummy) { return err = -EINVAL; }
	void set_admin_code(int code) { adminCode = code; }
	void set_hot_backup_id(uint64_t v) { hotBackupID = v; }
	void set_master_hb_timestamp(uint64_t t) { master_hb_timestamp = t; }
	void set_slave_hb_timestamp(uint64_t t) { slave_hb_timestamp = t; }

	// never return NULL
	NCResult *execute(const DTCValue *key = NULL);
	NCResult *execute_stream(const DTCValue *key = NULL);
	NCResult *execute_dgram(SocketAddress *peer, const DTCValue *key = NULL);
	NCResult *execute_network(const DTCValue *key = NULL);
	NCResult *execute_internal(const DTCValue *key = NULL);
	NCResult *execute(int64_t);
	NCResult *execute(const char *, int);
	NCResult *pre_check(const DTCValue *key); // return error result, NULL==success
	int set_compress_field_name(void);			 //Need compress flag for read,or set compressFlag for write
	int Encode(const DTCValue *key, Packet *);
	// return 1 if tdef changed...
	int set_tab_def(void);

	int encode_buffer(char *&ptr, int &len, int64_t &magic, const DTCValue *key = NULL);
	int encode_buffer(char *&ptr, int &len, int64_t &magic, int64_t);
	int encode_buffer(char *&ptr, int &len, int64_t &magic, const char *, int);
	const char *error_message(void) const
	{
		return _errmsg;
	}

	int set_expire_time(const char *key, int t);
	int get_expire_time(const char *key);

private:
	int set_compress_flag(const char *name)
	{
		if (tdef == NULL)
			return -EC_NOT_INITIALIZED;
		if (tdef->field_id(name) >= 64)
		{
			snprintf(_errmsg, sizeof(_errmsg), "compress error:field id must less than 64");
			return -EC_COMPRESS_ERROR;
		}
		compressFlag |= (1 << tdef->field_id(name));
		return 0;
	}
	uint64_t compressFlag; //field flag
	DTCCompress *gzip;
	int init_compress(void);
	char _errmsg[1024];
};

class NCResultLocal
{
public:
	const uint8_t *vidmap;
	long long apiTag;
	int maxvid;
	DTCCompress *gzip;

public:
	NCResultLocal(DTCTableDefinition *tdef) : vidmap(NULL),
											  apiTag(0),
											  maxvid(0),
											  gzip(NULL),
											  _tdef(tdef),
											  compressid(-1)
	{
	}

	virtual ~NCResultLocal(void)
	{
		FREE_CLEAR(vidmap);
		DELETE(gzip);
	}

	virtual int field_id_virtual(int id) const
	{
		return id > 0 && id <= maxvid ? vidmap[id - 1] : 0;
	}

	virtual void set_api_tag(long long t) { apiTag = t; }
	virtual long long get_api_tag(void) const { return apiTag; }

	void set_virtual_map(FieldSetByName &fs)
	{
		if (fs.max_virtual_id())
		{
			fs.Resolve(_tdef, 0);
			vidmap = fs.virtual_map();
			maxvid = fs.max_virtual_id();
		}
	}

	virtual int init_compress()
	{
		if (NULL == _tdef)
		{
			return -EC_CHECKSUM_MISMATCH;
		}

		int iret = 0;
		compressid = _tdef->compress_field_id();
		if (compressid < 0)
			return 0;
		if (gzip == NULL)
			NEW(DTCCompress, gzip);
		if (gzip == NULL)
			return -ENOMEM;
		iret = gzip->set_buffer_len(_tdef->max_field_size());
		if (iret)
			return iret;
		return 0;
	}

	virtual const int compress_id(void) const { return compressid; }

private:
	DTCTableDefinition *_tdef;
	uint64_t compressid;
};

class NCResult : public NCResultLocal, public DTCTask
{
public:
	NCResult(DTCTableDefinition *tdef) : NCResultLocal(tdef), DTCTask(tdef, TaskRoleClient, 0)
	{
		if (tdef)
			tdef->INC();
		mark_allow_remote_table();
	}

	NCResult(int err, const char *from, const char *msg) : NCResultLocal(NULL), DTCTask(NULL, TaskRoleClient, 1)
	{
		resultInfo.set_error_dup(err, from, msg);
	}
	virtual ~NCResult()
	{
		DTCTableDefinition *tdef = table_definition();
		DEC_DELETE(tdef);
	}
};

class NCResultInternal : public NCResultLocal, public TaskRequest
{
public:
	NCResultInternal(DTCTableDefinition *tdef = NULL) : NCResultLocal(tdef)
	{
	}

	virtual ~NCResultInternal()
	{
	}

	static inline int verify_class(void)
	{
		NCResultInternal *ir = 0;
		NCResult *er = reinterpret_cast<NCResult *>(ir);

		NCResultLocal *il = (NCResultLocal *)ir;
		NCResultLocal *el = (NCResultLocal *)er;

		DTCTask *it = (DTCTask *)ir;
		DTCTask *et = (DTCTask *)er;

		long dl = reinterpret_cast<char *>(il) - reinterpret_cast<char *>(el);
		long dt = reinterpret_cast<char *>(it) - reinterpret_cast<char *>(et);
		return dl == 0 && dt == 0;
	}
};

class DataConnector
{
	struct businessStatistics
	{
		uint64_t TotalTime;		// 10s内请求总耗时
		uint32_t TotalRequests; // 10s内请求总次数

	public:
		businessStatistics()
		{
			TotalTime = 0;
			TotalRequests = 0;
		}
	};

	struct bidCurve
	{
		uint32_t bid;
		uint32_t curve;
		bool operator<(const bidCurve &that) const
		{
			int sum1 = bid * 10 + curve;
			int sum2 = that.bid * 10 + that.curve;
			return sum1 < sum2;
		}
	};

private:
	std::map<bidCurve, businessStatistics> mapBi;
	Mutex _lock; // 读写  TotalTime、TotalRequests时，加锁，防止脏数据
	static DataConnector *pDataConnector;
	DataConnector();
	~DataConnector();
	pthread_t threadid;

public:
	static DataConnector *getInstance()
	{
		if (pDataConnector == NULL)
			pDataConnector = new DataConnector();
		return pDataConnector;
	};

public:
	int send_data();
	int set_report_info(const std::string str, const uint32_t curve, const uint64_t t);
	void get_report_info(std::map<bidCurve, businessStatistics> &mapData);
	int set_bussiness_id(std::string str);
};

#endif
