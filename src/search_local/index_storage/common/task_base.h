/*
 * =====================================================================================
 *
 *       Filename:  task_base.h
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
#ifndef __CH_TASK_H__
#define __CH_TASK_H__

#include "table_def.h"
#include "section.h"
#include "field.h"
#include "packet.h"
#include "result.h"
#include "buffer_error.h"
#include "timestamp.h"
#include <sys/time.h>
#include "log.h"
#include "buffer.h"
#include "receiver.h"

class NCRequest;

enum DecodeResult
{
	DecodeFatalError, // no response needed
	DecodeDataError,  // response with error code
	DecodeIdle,		  // no data received
	DecodeWaitData,	  // partial decoding
	DecodeDone		  // full packet
};

enum DecodeStage
{						   // internal use
	DecodeStageFatalError, // same as result
	DecodeStageDataError,  // same as result
	DecodeStageIdle,	   // same as result
	DecodeStageWaitHeader, // partial header
	DecodeStageWaitData,   // partial data
	DecodeStageDiscard,	   // error mode, discard remain data
	DecodeStageDone		   // full packet
};

enum TaskRole
{
	TaskRoleServer = 0,	 /* server, for incoming thread */
	TaskRoleClient,		 /* client, reply from server */
	TaskRoleHelperReply, /* helper, reply from server */
};

enum TaskType
{
	TaskTypeAdmin = 0,
	TaskTypeRead,	/* Read Only operation */
	TaskTypeWrite,	/* Modify data */
	TaskTypeCommit, /* commit dirty data */
	TaskTypeWriteHbLog,
	TaskTypeReadHbLog,
	TaskTypeWriteLruHbLog,
	TaskTypeRegisterHbLog,
	TaskTypeQueryHbLogInfo,
	TaskTypeHelperReloadConfig,
};

enum CHITFLAG
{
	HIT_INIT = 0,	 // hit init flag
	HIT_SUCCESS = 1, // hit success flag
};

class DTCTask : public TableReference
{
public:
	static const DecodeResult stage2result[];
	DecodeResult get_decode_result(void) { return stage2result[stage]; };
	static const uint32_t validcmds[];
	static const uint16_t cmd2type[];
	static const uint16_t validsections[][2];
	static const uint8_t validktype[DField::TotalType][DField::TotalType];
	static const uint8_t validxtype[DField::TotalOperation][DField::TotalType][DField::TotalType];
	static const uint8_t validcomps[DField::TotalType][DField::TotalComparison];

protected:
	class BufferPool
	{
		// simple buffer pool, only keep 2 buffers
	public:
		char *ptr[2];
		int len[2];
		BufferPool()
		{
			ptr[0] = NULL;
			ptr[1] = NULL;
		}
		~BufferPool()
		{
			FREE_IF(ptr[0]);
			FREE_IF(ptr[1]);
		}
		void Push(char *v)
		{
			ptr[ptr[0] ? 1 : 0] = v;
		}

		inline char *Allocate(int size, TaskRole role)
		{
			if (role == TaskRoleServer || role == TaskRoleClient)
			{
				CreateBuff(size, len[0], &ptr[0]);
				if (ptr[0] == NULL)
					return NULL;
				return ptr[0];
			}
			else
			{
				CreateBuff(size, len[1], &ptr[1]);
				if (ptr[1] == NULL)
					return NULL;
				return ptr[1];
			}
		}
		char *Clone(char *buff, int size, TaskRole role)
		{
			if (role != TaskRoleClient)
				return NULL;
			char *p;
			if (ptr[0])
				p = ptr[1] = (char *)MALLOC(size);
			else
				p = ptr[0] = (char *)MALLOC(size);

			if (p)
				memcpy(p, buff, size);
			return p;
		}
	};

public:
	char *migratebuf;

protected: // decoder informations
	DecodeStage stage;
	TaskRole role;
	BufferPool packetbuf;

	//don't use it except packet decoding
	DTCTableDefinition *dataTableDef;
	DTCTableDefinition *hotbackupTableDef;

	// used by replicate table definition
	DTCTableDefinition *replicateTableDef;

protected: // packet info, read-only
	DTCFieldValue *updateInfo;
	DTCFieldValue *conditionInfo;
	DTCFieldSet *fieldList;

public:
	ResultSet *result;

public: // packet info, read-write
	DTCVersionInfo versionInfo;
	DTCRequestInfo requestInfo;
	DTCResultInfo resultInfo;

protected:				  // working data
	uint64_t serialNr;	  /* derived from packet */
	const DTCValue *key;  /* derived from packet */
	const DTCValue *rkey; /* processing */
	/* resultWriter only create once in task entire life */
	ResultWriter *resultWriter; /* processing */
	int resultWriterReseted;
	uint8_t requestCode;  /* derived from packet */
	uint8_t requestType;  /* derived from packet */
	uint8_t requestFlags; /* derived from packet */
	uint8_t replyCode;	  /* processing */
	uint8_t replyFlags;	  /* derived from packet */
	enum
	{
		PFLAG_REMOTETABLE = 1,
		PFLAG_ALLROWS = 2,
		PFLAG_PASSTHRU = 4,
		PFLAG_ISHIT = 8,
		PFLAG_FETCHDATA = 0x10,
		PFLAG_ALLOWREMOTETABLE = 0x20,
		PFLAG_FIELDSETWITHKEY = 0x40,
		PFLAG_BLACKHOLED = 0x80,
	};
	uint8_t processFlags; /* processing */

protected:
	// return total packet size
	int decode_header(const PacketHeader &in, PacketHeader *out = NULL);
	int validate_section(PacketHeader &header);
	void decode_request(PacketHeader &header, char *p);
	int decode_field_value(char *d, int l, int m);
	int decode_field_set(char *d, int l);
	int decode_result_set(char *d, int l);

public:
	DTCTask(DTCTableDefinition *tdef = NULL, TaskRole r = TaskRoleServer, int final = 0) : TableReference(tdef),
																						   migratebuf(NULL),
																						   stage(final ? DecodeStageDataError : DecodeStageIdle),
																						   role(r),
																						   dataTableDef(tdef),
																						   hotbackupTableDef(NULL),
																						   replicateTableDef(NULL),
																						   updateInfo(NULL),
																						   conditionInfo(NULL),
																						   fieldList(NULL),
																						   result(NULL),
																						   serialNr(0),
																						   key(NULL),
																						   rkey(NULL),
																						   resultWriter(NULL),
																						   resultWriterReseted(0),
																						   requestCode(0),
																						   requestType(0),
																						   requestFlags(0),
																						   replyCode(0),
																						   replyFlags(0),
																						   processFlags(PFLAG_ALLROWS)
	{
	}

	virtual ~DTCTask(void)
	{
		DELETE(updateInfo);
		DELETE(conditionInfo);
		DELETE(fieldList);
		DELETE(resultWriter);
		DELETE(result);
		FREE_IF(migratebuf);
	}
	// linked clone
	inline DTCTask(const DTCTask &orig)
	{
		DTCTask();
		Copy(orig);
	}

	// these Copy()... only apply to empty DTCTask
	// linked clone
	int Copy(const DTCTask &orig);
	// linked clone with replace key
	int Copy(const DTCTask &orig, const DTCValue *newkey);
	// replace row
	int Copy(const RowValue &);
	// internal API
	int Copy(NCRequest &rq, const DTCValue *key);

	inline void Clean()
	{
		TableReference::set_table_definition(NULL);
		if (updateInfo)
			updateInfo->Clean();
		if (conditionInfo)
			conditionInfo->Clean();
		if (fieldList)
			fieldList->Clean();
		if (result)
			result->Clean();
		versionInfo.Clean();
		requestInfo.Clean();
		resultInfo.Clean();

		//serialNr = 0;
		key = NULL;
		rkey = NULL;
		if (resultWriter)
		{
			resultWriter->Clean();
			resultWriterReseted = 0;
		}
		requestCode = 0;
		requestType = 0;
		requestFlags = 0;
		replyCode = 0;
		replyFlags = 0;
		processFlags = PFLAG_ALLROWS;
	}

	//////////// some API access request property
	inline void set_data_table(DTCTableDefinition *t)
	{
		TableReference::set_table_definition(t);
		dataTableDef = t;
	}
	inline void set_hotbackup_table(DTCTableDefinition *t) { hotbackupTableDef = t; }
	inline void set_replicate_table(DTCTableDefinition *t) { replicateTableDef = t; }
	inline DTCTableDefinition *get_replicate_table() { return replicateTableDef; }
#if 0
		DTCTableDefinition *table_definition(void) const { return tableDef; }
		int key_format(void) const { return tableDef->key_format(); }
		int key_fields(void) const { return tableDef->key_fields(); }
		int key_auto_increment(void) const { return tableDef->key_auto_increment(); }
		int auto_increment_field_id(void) const { return tableDef->auto_increment_field_id(); }
		int field_type(int n) const { return tableDef->field_type(n); }
		int field_offset(int n) const { return tableDef->field_type(n); }
		int field_id(const char *n) const { return tableDef->field_id(n); }
		const char* field_name(int id) const { return tableDef->field_name(id); }
#endif

	// This code has to value (not very usefull):
	// DRequest::ResultInfo --> result/error code/key only
	// DRequest::DTCResultSet  --> result_code() >=0, with DTCResultSet
	// please use result_code() for detail error code
	int reply_code(void) const { return replyCode; }

	// Retrieve request key
	int has_request_key(void) const { return key != NULL; }
	const DTCValue *request_key(void) const { return key; }
	unsigned int int_key(void) const { return (unsigned int)(request_key()->u64); }
	void update_key(RowValue *r) { (*r)[0] = *request_key(); }

	// only for test suit
	void set_request_condition(DTCFieldValue *cond) { conditionInfo = cond; }
	void set_request_key(DTCValue *val) { key = val; }

	const DTCFieldValue *request_condition(void) const { return conditionInfo; }
	const DTCFieldValue *request_operation(void) const { return updateInfo; }

	//for migrate
	void set_request_operation(DTCFieldValue *ui) { updateInfo = ui; }
	const DTCFieldSet *request_fields(void) const { return fieldList; }
	const uint64_t request_serial(void) const { return serialNr; }

	// result key
	const DTCValue *result_key(void) const { return rkey; }
	// only if insert w/o key
	void set_result_key(const DTCValue &v)
	{
		resultInfo.set_key(v);
		rkey = &v;
	}

	static int max_header_size(void) { return sizeof(PacketHeader); }
	static int min_header_size(void) { return sizeof(PacketHeader); }
	static int check_packet_size(const char *buf, int len);

	// Decode data from fd
	void decode_stream(SimpleReceiver &receiver);
	DecodeResult Decode(SimpleReceiver &receiver)
	{
		decode_stream(receiver);
		return get_decode_result();
	}

	// Decode data from packet
	//     type 0: clone packet
	//     type 1: eat(keep&free) packet
	//     type 2: use external packet
	void decode_packet(char *packetIn, int packetLen, int type);
	DecodeResult Decode(char *packetIn, int packetLen, int type)
	{
		decode_packet(packetIn, packetLen, type);
		return get_decode_result();
	}

	DecodeResult Decode(const char *packetIn, int packetLen)
	{
		decode_packet((char *)packetIn, packetLen, 0);
		return get_decode_result();
	};

	inline void BeginStage() { stage = DecodeStageIdle; }

	// change role from TaskRoleServer to TaskRoleHelperReply
	// cleanup decode state, prepare reply from helper
	inline void prepare_decode_reply(void)
	{
		role = TaskRoleHelperReply;
		stage = DecodeStageIdle;
	}

	inline void set_role_as_server() { role = TaskRoleServer; }

	// set error code before Packet::encode_result();
	// err is positive errno
	void set_error(int err, const char *from, const char *msg)
	{
		resultInfo.set_error(err, from, msg);
	}
	void set_error_dup(int err, const char *from, const char *msg)
	{
		resultInfo.set_error_dup(err, from, msg);
	}
	// retrieve previous result code
	// >= 0 success
	// < 0 err, negative value of set_error()
	int result_code(void) { return resultInfo.result_code(); }
	int allow_remote_table(void) const { return processFlags & PFLAG_ALLOWREMOTETABLE; }
	void mark_allow_remote_table(void) { processFlags |= PFLAG_ALLOWREMOTETABLE; }
	void mark_has_remote_table(void) { processFlags |= PFLAG_REMOTETABLE; }
	DTCTableDefinition *remote_table_definition(void)
	{
		if (processFlags & PFLAG_REMOTETABLE)
			return table_definition();
		return NULL;
	}

	//////////// some API for request processing
	// Client Request Code
	int Role(void) const { return role; }
	int request_code(void) const { return requestCode; }
	void set_request_code(uint8_t code) { requestCode = code; }
	int request_type(void) const { return requestType; }
	void set_request_type(TaskType type) { requestType = type; }
	int flag_keep_alive(void) const { return requestFlags & DRequest::Flag::KeepAlive; }
	int flag_table_definition(void) const { return requestFlags & DRequest::Flag::NeedTableDefinition; }
	int flag_no_cache(void) const { return requestFlags & DRequest::Flag::no_cache; }
	int flag_no_result(void) const { return requestFlags & DRequest::Flag::NoResult; }
	int flag_no_next_server(void) const { return requestFlags & DRequest::Flag::no_next_server; }
	int flag_multi_key_val(void) const { return requestFlags & DRequest::Flag::MultiKeyValue; }
	int flag_multi_key_result(void) const { return replyFlags & DRequest::Flag::MultiKeyValue; }
	int flag_admin_table(void) const { return requestFlags & DRequest::Flag::admin_table; }
	int flag_pass_thru(void) const { return processFlags & PFLAG_PASSTHRU; }
	int flag_fetch_data(void) const { return processFlags & PFLAG_FETCHDATA; }
	int flag_is_hit(void) const { return processFlags & PFLAG_ISHIT; }
	int flag_field_set_with_key(void) const { return processFlags & PFLAG_FIELDSETWITHKEY; }
	int flag_black_hole(void) const { return processFlags & PFLAG_BLACKHOLED; }
	void mark_as_pass_thru(void) { processFlags |= PFLAG_PASSTHRU; }
	void mark_as_fetch_data(void) { processFlags |= PFLAG_FETCHDATA; }
	void mark_as_hit(void) { processFlags |= PFLAG_ISHIT; }
	void mark_field_set_with_key(void) { processFlags |= PFLAG_FIELDSETWITHKEY; }
	void mark_as_black_hole(void) { processFlags |= PFLAG_BLACKHOLED; }
	void set_result_hit_flag(CHITFLAG hitFlag) { resultInfo.set_hit_flag((uint32_t)hitFlag); }

	// API for key expire time
	int update_key_expire_time(int max);

	// this is count only request
	int count_only(void) const { return fieldList == NULL; }
	// this is non-contional request
	void clear_all_rows(void) { processFlags &= ~PFLAG_ALLROWS; }
	int all_rows(void) const { return processFlags & PFLAG_ALLROWS; }
	// apply insertValues, updateOperations to row
	int update_row(RowValue &row)
	{
		return updateInfo == NULL ? 0 : updateInfo->Update(row);
	}
	// checking the condition
	int compare_row(const RowValue &row, int iCmpFirstNRows = 256) const
	{
		return all_rows() ? 1 : conditionInfo->Compare(row, iCmpFirstNRows);
	}

	// prepare an DTCResultSet, for afterward operation
	int prepare_result(int start, int count);
	inline int prepare_result(void)
	{
		return prepare_result(requestInfo.limit_start(), requestInfo.limit_count());
	}
	inline int prepare_result_no_limit(void)
	{
		return prepare_result(0, 0);
	}

	inline void detach_result_in_result_writer()
	{
		if (resultWriter)
			resultWriter->detach_result();
	}

	// new a countonly DTCResultSet with 'nr' rows
	// No extra append_row() allowed
	int set_total_rows(unsigned int nr, int Force = 0)
	{
		if (!Force)
		{
			if (resultWriter || prepare_result() == 0)
				return resultWriter->set_rows(nr);
		}
		else
		{
			resultWriter->set_total_rows(nr);
		}
		return 0;
	}
	void add_total_rows(int n) { resultWriter->add_total_rows(n); }
	int in_range(unsigned int nr, unsigned int begin = 0) const { return resultWriter->in_range(nr, begin); }
	int result_full(void) const { return resultWriter && resultWriter->is_full(); }
	// append_row, from row 'r'
	int append_row(const RowValue &r) { return resultWriter->append_row(r); }
	int append_row(const RowValue *r) { return r ? resultWriter->append_row(*r) : 0; }

	// Append Row from DTCResultSet with condition operation
	int append_result(ResultSet *rs);
	// Append All Row from DTCResultSet
	int pass_all_result(ResultSet *rs);
	// Merge all row from sub-task
	int merge_result(const DTCTask &task);
	// Get Encoded Result Packet
	ResultPacket *get_result_packet(void) { return (ResultPacket *)resultWriter; }
	// Process Internal Results
	int process_internal_result(uint32_t ts = 0);
};

extern int packet_body_len(PacketHeader &header);

#endif
