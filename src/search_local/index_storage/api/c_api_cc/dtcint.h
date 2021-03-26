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
#include "udppool.h"
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


#define HUNDRED 100
#define THOUSAND 1000
#define TENTHOUSAND 10000
#define HUNDREDTHOUSAND 100000
#define MILLION 1000000


enum E_REPORT_TYPE
{
	RT_MIN,
	RT_SHARDING,
	RT_ALL,
	RT_MAX
};


//tp99 统计区间，单位us
static const uint32_t kpi_sample_count[] =
{
	200, 500, 1000,
	2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
	20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000,
	200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000,
	2000000, 500000, 10000000
};


class DTCTask;
class Packet;

class NCResult;
class NCResultInternal;
class NCPool;
struct NCTransation;
class IDTCTaskExecutor;
class IDTCService;

class NCBase {
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
class NCServer: public NCBase {
public:	// global server state
	NCServer();
	NCServer(const NCServer &);
	~NCServer(void);

	// base settings
	SocketAddress addr;
	char *tablename;
	char *appname;

	static DataConnector *dc;

    //for compress
    void SetCompressLevel(int level){compressLevel=level;}
    int GetCompressLevel(void){return compressLevel;}

    //dtc set _server_address and _server_tablename for plugin
    static char * _server_address;
    static char * _server_tablename;

	int keytype;
	int autoUpdateTable;
	int autoReconnect;
    static int _network_mode;
	NCKeyInfo keyinfo;

	void CloneTabDef(const NCServer& source);
	int SetAddress(const char *h, const char *p=NULL);
	int SetTableName(const char *);
	const char * GetAddress(void) const { return addr.Name(); }//this addres is set by user
	const char * GetServerAddress(void) const { return _server_address; }//this address is set by dtc
	const char * GetServerTableName(void) const { return _server_tablename;}
	int IsDgram(void) const { return addr.socket_type()==SOCK_DGRAM; }
	int IsUDP(void) const { return addr.socket_type()==SOCK_DGRAM && addr.socket_family()==AF_INET; }
	const char * GetTableName(void) const { return tablename; }
	int IntKey(void);
	int StringKey(void);
	int FieldType(const char*);
	int IsCompleted(void) const { return completed; }
	int AddKey(const char* name, uint8_t type);
	int KeyFieldCnt(void) const { return keyinfo.KeyFields() ? : keytype != DField::None ? 1 : 0; }
	int AllowBatchKey(void) const { return keyinfo.KeyFields(); }
	int SimpleBatchKey(void) const { return keyinfo.KeyFields()==1 && keytype == keyinfo.KeyType(0); }

	void SetAutoUpdateTab(bool autoUpdate){ autoUpdateTable = autoUpdate?1:0; }
	void SetAutoReconnect(int reconnect){ autoReconnect = reconnect; }
	
	// error state
	unsigned completed:1;
	unsigned badkey:1;
	unsigned badname:1;
	unsigned autoping:1;
	const char *errstr;

	const char *ErrorMessage(void) const { return errstr; }

	// table definition
	DTCTableDefinition *tdef;
	DTCTableDefinition *admin_tdef;
	
	void SaveDefinition(NCResult *);
	DTCTableDefinition* GetTabDef(int cmd) const;
	
	/*date:2014/06/04, author:xuxinxin*/
	std::string accessToken;
	int SetAccessKey(const char *token);
private: // serialNr manupulation
	uint64_t lastSN;
public:
	uint64_t NextSerialNr(void) { ++lastSN; if(lastSN==0) lastSN++; return lastSN; }
	uint64_t LastSerialNr(void) { return lastSN; }

private: // timeout settings
	int timeout;
	int realtmo;
public:
	void SetMTimeout(int n);
	int GetTimeout(void) const { return timeout; }
private:
    int compressLevel;

private: // sync execution
	IDTCService         *iservice;
	IDTCTaskExecutor    *executor;
	int                 netfd;
	time_t              lastAct;
	NCRequest           *pingReq;

private:
	uint64_t agentTime;

public: 
	void SetAgentTime(int t){agentTime=t;}
	uint64_t GetAgentTime(void){return agentTime;}

public:
	int Connect(void);
	int Reconnect(void);
	void Close(void);
	void SetFD(int fd) { Close(); netfd = fd; UpdateTimeout(); }
	// stream, connected
	int SendPacketStream(Packet &);
	int DecodeResultStream(NCResult &);
	// dgram, connected or connectless
	int SendPacketDgram(SocketAddress *peer, Packet &);
	int DecodeResultDgram(SocketAddress *peer, NCResult &);
	// connectless
	NCUdpPort *GetGlobalPort(void);
	void PutGlobalPort(NCUdpPort *);

	void TryPing(void);
	int Ping(void);
	void AutoPing(void) { if(!IsDgram()) autoping = 1; }
	NCResultInternal *ExecuteInternal(NCRequest &rq, const DTCValue *kptr) { return executor->task_execute(rq, kptr); }
	int HasInternalExecutor(void) const { return executor != 0; }
private:
	int BindTempUnixSocket(void);
	void UpdateTimeout(void);
	void UpdateTimeoutAnyway(void);
	// this method is weak, and don't exist in libdtc.a
	__attribute__((__weak__)) void CheckInternalService(void);

public: // transation manager, impl at dtcpool.cc
	int AsyncConnect(int &);
	NCPool *ownerPool;
	int ownerId;
	void SetOwner(NCPool *, int);

	NCResult *DecodeBuffer(const char *, int);
	static int CheckPacketSize(const char *, int);

//add by neolv to QOS
private:
	//原有的不变增加三个参数用于做QOS
	uint64_t m_iErrCount;
	//用于统计请求总数， 请求总数只对一个周期有效
	uint64_t m_iTotalCount;
	//用于统计请求总耗时， 请求总数只对一个周期有效
	uint64_t m_iTotalElaps;
	//被摘次数
	int m_RemoveCount;
public:
	void IncErrCount()
	{
		++m_iErrCount;
	}
	uint64_t GetErrCount()
	{
		return m_iErrCount;
	}
	void ClearErrCount()
	{
		m_iErrCount = 0;
	}

	void IncRemoveCount()
	{
		++m_RemoveCount;
	}
	int GetRemoveCount()
	{
		return m_RemoveCount;
	}
	void ClearRemoveCount()
	{
		m_RemoveCount = 0;
	}

	void IncTotalReq()
	{
		++m_iTotalCount;
	}
	uint64_t GetTotalReq()
	{
		return  m_iTotalCount;
	}
	void ClearTotalReq()
	{
		m_iTotalCount = 0;
	}
	
	void AddTotalElaps(uint64_t iElaps)
	{
		m_iTotalElaps += iElaps;
	}
	uint64_t GetTotalElaps()
	{
		return m_iTotalElaps;
	}
	void ClearTotalElaps()
	{
		m_iTotalElaps = 0;
	}

};


class NCRequest {
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
	uint64_t MasterHBTimestamp;
	uint64_t SlaveHBTimestamp;

public:
	NCRequest(NCServer *, int op);
	~NCRequest(void);
	int AttachServer(NCServer *);

	void EnableNoCache(void) { flags |= DRequest::Flag::no_cache; }
	void EnableNoNextServer(void) { flags |= DRequest::Flag::no_next_server; }
	void EnableNoResult(void) { flags |= DRequest::Flag::NoResult; }
	int AddCondition(const char *n, uint8_t op, uint8_t t, const DTCValue &v);
	int AddOperation(const char *n, uint8_t op, uint8_t t, const DTCValue &v);
    int CompressSet(const char *n,const char * v,int len);
    int CompressSetForce(const char *n,const char * v,int len);
	int AddValue(const char *n, uint8_t t, const DTCValue &v);
	int Need(const char *n, int);
	void Limit(unsigned int st, unsigned int cnt) {
		if(cnt==0) st = 0;
		limitStart = st;
		limitCount = cnt;
	}
	
	int SetKey(int64_t);
	int SetKey(const char *, int);
	int UnsetKey(void);
	int UnsetKeyValue(void);
	int FieldType(const char* name){ return server?server->FieldType(name):DField::None; }
	int AddKeyValue(const char* name, const DTCValue &v, uint8_t type);
	int SetCacheID(int dummy) { return err = -EINVAL; }
	void SetAdminCode(int code){ adminCode = code; }
	void SetHotBackupID(uint64_t v){ hotBackupID=v; }
	void SetMasterHBTimestamp(uint64_t t){ MasterHBTimestamp=t; }
	void SetSlaveHBTimestamp(uint64_t t){ SlaveHBTimestamp=t; }
	
	// never return NULL
	NCResult *Execute(const DTCValue *key=NULL);
	NCResult *ExecuteStream(const DTCValue *key=NULL);
	NCResult *ExecuteDgram(SocketAddress *peer, const DTCValue *key = NULL);
	NCResult *ExecuteNetwork(const DTCValue *key=NULL);
	NCResult *ExecuteInternal(const DTCValue *key=NULL);
	NCResult *Execute(int64_t);
	NCResult *Execute(const char *, int);
	NCResult *PreCheck(const DTCValue *key); // return error result, NULL==success
    int SetCompressFieldName(void);//Need compress flag for read,or set compressFlag for write
	int Encode(const DTCValue *key, Packet *);
	// return 1 if tdef changed...
	int SetTabDef(void);

	int EncodeBuffer(char *&ptr, int&len, int64_t &magic, const DTCValue *key=NULL);
	int EncodeBuffer(char *&ptr, int&len, int64_t &magic, int64_t);
	int EncodeBuffer(char *&ptr, int&len, int64_t &magic, const char *, int);
    const char* ErrorMessage(void) const
    {
        return _errmsg;
    }

    int SetExpireTime(const char* key, int t);
    int GetExpireTime(const char* key);

private:
		int CheckKey(const DTCValue *kptr);
        int setCompressFlag(const char * name)
        {
            if (tdef==NULL)
                return -EC_NOT_INITIALIZED;
            if (tdef->field_id(name) >= 64)
            {
                snprintf(_errmsg, sizeof(_errmsg), "compress error:field id must less than 64"); 
                return -EC_COMPRESS_ERROR;
            }
            compressFlag|=(1<<tdef->field_id(name));
            return 0;
        }
        uint64_t compressFlag;//field flag
        DTCCompress *gzip;
        int initCompress(void);
        char _errmsg[1024];
};

class NCResultLocal {
	public:
		const uint8_t *vidmap;
		long long apiTag;
		int maxvid;
        DTCCompress *gzip;

	public:
		NCResultLocal(DTCTableDefinition* tdef) :
            vidmap(NULL),
            apiTag(0),
            maxvid(0),
            gzip (NULL),
            _tdef(tdef),
	    compressid(-1)
	{
	}

		virtual ~NCResultLocal(void) {
			FREE_CLEAR(vidmap);
			DELETE (gzip);
		}

		virtual int FieldIdVirtual(int id) const {
			return id > 0 && id <= maxvid ? vidmap[id-1] : 0;
		}

		virtual void SetApiTag(long long t) { apiTag = t; }
		virtual long long GetApiTag(void) const { return apiTag; }

        void SetVirtualMap(FieldSetByName &fs)
        {
			if(fs.max_virtual_id()){
				fs.Resolve(_tdef, 0);
				vidmap = fs.virtual_map();
				maxvid = fs.max_virtual_id();
			}
		}

        virtual int initCompress()
        {
            if (NULL == _tdef)
            {
                return -EC_CHECKSUM_MISMATCH;
            }

            int iret = 0;
            compressid = _tdef->compress_field_id();
            if (compressid<0) return 0;
            if (gzip==NULL)
                NEW(DTCCompress,gzip);
            if (gzip==NULL)
                return -ENOMEM;
            iret = gzip->set_buffer_len(_tdef->max_field_size());
            if (iret) return iret;
            return 0;
        }

        virtual const int CompressId (void)const {return compressid;}

    private:
        DTCTableDefinition* _tdef;
        uint64_t compressid;

};

class NCResult: public NCResultLocal, public DTCTask
{
	public:
		NCResult(DTCTableDefinition *tdef) : NCResultLocal(tdef), DTCTask(tdef, TaskRoleClient, 0) {
			if(tdef) tdef->INC();
			mark_allow_remote_table();
		}

		NCResult(int err, const char *from, const char *msg) : NCResultLocal(NULL), DTCTask(NULL, TaskRoleClient, 1) {
			resultInfo.set_error_dup(err, from, msg);
		}
		virtual ~NCResult() {
			DTCTableDefinition *tdef = table_definition();
			DEC_DELETE(tdef);
		}
};

class NCResultInternal: public NCResultLocal, public TaskRequest
{
	public:
		NCResultInternal(DTCTableDefinition* tdef=NULL) : NCResultLocal (tdef)
		{
		}

		virtual ~NCResultInternal() 
		{
		}

		static inline int VerifyClass(void)
		{
			NCResultInternal *ir = 0;
			NCResult *er = reinterpret_cast<NCResult *>(ir);

			NCResultLocal *il = (NCResultLocal *) ir;
			NCResultLocal *el = (NCResultLocal *) er;

			DTCTask *it = (DTCTask *) ir;
			DTCTask *et = (DTCTask *) er;

			long dl = reinterpret_cast<char *>(il) - reinterpret_cast<char *>(el);
			long dt = reinterpret_cast<char *>(it) - reinterpret_cast<char *>(et);
			return dl==0 && dt==0;
		}
};

/*date:2014/06/09, author:xuxinxin 模调上报 */
class DataConnector
{
	struct businessStatistics
	{
		uint64_t TotalTime;			// 10s内请求总耗时
		uint32_t TotalRequests;		// 10s内请求总次数

	public:
		businessStatistics(){ TotalTime = 0; TotalRequests = 0; }
	};

	struct bidCurve{
		uint32_t bid;
		uint32_t curve;
        bool operator < (const bidCurve &that) const{
				int sum1 = bid * 10 + curve;
				int sum2 = that.bid * 10 + that.curve;
                return sum1 < sum2;
        }
	};

	struct top_percentile_statistics
	{
		uint32_t uiBid;
		uint32_t uiAgentIP;
		uint16_t uiAgentPort;
		uint32_t uiTotalRequests;	//10s内请求总次数
		uint64_t uiTotalTime;		//10s内请求总耗时
		uint32_t uiFailCount;		//10s内错误次数
		uint64_t uiMaxTime;			//10s内的最大执行时间
		uint64_t uiMinTime;			//10s内的最小执行时间
		uint32_t statArr[sizeof(kpi_sample_count) / sizeof(kpi_sample_count[0])];	//统计值
		
	public:
		top_percentile_statistics()
		{
			uiBid = 0;
			uiAgentIP = 0;
			uiAgentPort = 0;
			uiTotalRequests = 0;
			uiTotalTime = 0;
			uiFailCount = 0;
			uiMaxTime = 0;
			uiMinTime = 0;
			memset(statArr, 0, sizeof(statArr));
		}
		top_percentile_statistics(const top_percentile_statistics &that)
		{
			this->uiBid = that.uiBid;
			this->uiAgentIP = that.uiAgentIP;
			this->uiAgentPort = that.uiAgentPort;
			this->uiTotalRequests = that.uiTotalRequests;
			this->uiTotalTime = that.uiTotalTime;
			this->uiFailCount = that.uiFailCount;
			this->uiMaxTime = that.uiMaxTime;
			this->uiMinTime = that.uiMinTime;
			memcpy(this->statArr, that.statArr, sizeof(this->statArr));
		}
		top_percentile_statistics &operator =(const top_percentile_statistics &that)
		{
			this->uiBid = that.uiBid;
			this->uiAgentIP = that.uiAgentIP;
			this->uiAgentPort = that.uiAgentPort;
			this->uiTotalRequests = that.uiTotalRequests;
			this->uiTotalTime = that.uiTotalTime;
			this->uiFailCount = that.uiFailCount;
			this->uiMaxTime = that.uiMaxTime;
			this->uiMinTime = that.uiMinTime;
			memcpy(this->statArr, that.statArr, sizeof(this->statArr));

			return *this;
		}
	};

	private:
		std::map<bidCurve, businessStatistics> mapBi;
		Mutex _lock;			// 读写  TotalTime、TotalRequests时，加锁，防止脏数据
		std::map<uint64_t, top_percentile_statistics> mapTPStat;
		Mutex m_tp_lock;	//读写 tp99 数据时，加锁，防止脏数据
		static DataConnector *pDataConnector;
		DataConnector();
		~DataConnector();
		pthread_t threadid;

	public:
		static DataConnector* getInstance()
		{
			if( pDataConnector == NULL)
				pDataConnector = new DataConnector();
			return pDataConnector;
		};

	public:
		int SendData();
		int SetReportInfo(const std::string str, const uint32_t curve, const uint64_t t);
		void GetReportInfo(std::map<bidCurve, businessStatistics> &mapData);
		int SetBussinessId(std::string str);
		
		int SendTopPercentileData();
		int SetTopPercentileData(const std::string strAccessKey, const std::string strAgentAddr, const uint64_t elapse, const int status);
		void GetTopPercentileData(std::map<uint64_t, top_percentile_statistics> &mapStat);
		int SetTopPercentileConfig(std::string strAccessKey, const std::string strAgentAddr);
		
private:
		int ParseAddr(const std::string strAddr, uint64_t *uipIP = NULL, uint64_t *uipPort = NULL);
};


class CTopPercentileSection
{
public:
	static int16_t GetTPSection(uint64_t elapse);

private:
	//执行时间小于1000us的
	static int16_t GetLTThousand(uint64_t elapse);
	//执行时间大于等于1000us 小于 10000us的
	static int16_t GetGTEThousandLTTenThousand(uint64_t elapse);
	//执行时间大于等于10000us 小于 100000us的
	static int16_t GetGTETenThousandLTHundredThousand(uint64_t elapse);
	//执行时间大于等于100000us 小于 1000000us的
	static int16_t GetGTEHundredThousandLTMillion(uint64_t elapse);
	//执行时间大于等于1000000us的
	static int16_t GetGTEMillion(uint64_t elapse);
};


#endif
