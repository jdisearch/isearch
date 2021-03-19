/*
 * =====================================================================================
 *
 *       Filename:  buffer_process.h
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
#ifndef __CACHE_RPOCESS
#define __CACHE_RPOCESS

#include <sys/mman.h>
#include <time.h>

#include "protocol.h"
#include "value.h"
#include "field.h"
#include "section.h"
#include "table_def.h"
#include "task_request.h"
#include "list.h"
#include "fence.h"
#include "buffer_pool.h"
#include "poll_thread.h"
#include "dbconfig.h"
#include "lqueue.h"
#include "stat_dtc.h"
#include "data_process.h"
#include "empty_filter.h"
#include "namespace.h"
#include "task_pendlist.h"
#include "data_chunk.h"
#include "hb_log.h"
#include "lru_bit.h"
#include "hb_feature.h"
#include "blacklist_unit.h"
#include "expire_time.h"

DTC_BEGIN_NAMESPACE

class DTCFlushRequest;
class BufferProcess;
class DTCTableDefinition;
class TaskPendingList;
enum BufferResult {
		BUFFER_PROCESS_ERROR =-1,
		BUFFER_PROCESS_OK =0,
		BUFFER_PROCESS_NEXT =1,
		BUFFER_PROCESS_PENDING =2,
		BUFFER_PROCESS_REMOTE =3 ,
		BUFFER_PROCESS_PUSH_HB = 4
};
typedef unsigned int MARKER_STAMP;

class BufferReplyNotify: public ReplyDispatcher<TaskRequest> {
	private:
		BufferProcess *owner;
	public:
		BufferReplyNotify(BufferProcess *o) :
			owner(o)
	{}
		virtual ~BufferReplyNotify(){}
		virtual void reply_notify(TaskRequest *);
};

class FlushReplyNotify: public ReplyDispatcher<TaskRequest> {
	private:
		BufferProcess *owner;
	public:
		FlushReplyNotify(BufferProcess *o) :
			owner(o)
	{}
		virtual ~FlushReplyNotify(){}
		virtual void reply_notify(TaskRequest *);
};

class HotBackReplay : public ReplyDispatcher<TaskRequest>
{
    public:
	HotBackReplay() {}
	virtual ~HotBackReplay() {}
	virtual void reply_notify(TaskRequest *task);
};

enum {
	LRU_NONE=0,
	LRU_BATCH,
	LRU_READ,
	LRU_WRITE,
	LRU_ALWAYS=999,
};

enum {
	NODESTAT_MISSING,
	NODESTAT_EMPTY,
	NODESTAT_PRESENT
};

struct CacheTransation {
	TaskRequest *curTask;
	const char *ptrKey;
	Node m_stNode;
	int oldRows;
	uint8_t nodeStat;
	uint8_t keyDirty;
	uint8_t nodeEmpty;
	uint8_t lruUpdate;
	int logtype; // OLD ASYNC TRANSATION LOG
	RawData *fstLogRows; // OLD ASYNC TRANSATION LOG
	RawData *pstLogRows; // OLD ASYNC TRANSATION LOG

	void Init(TaskRequest *task) {
		memset(this, 0, sizeof(CacheTransation));
		curTask = task;
	}

	void Free(void) {
		if(fstLogRows) delete fstLogRows;
		fstLogRows = NULL;
		pstLogRows = NULL;
		logtype = 0;

		ptrKey = NULL;
		m_stNode = Node::Empty();
		nodeStat = 0;
		keyDirty = 0;
		oldRows = 0;
		nodeEmpty = 0;
		lruUpdate = 0;
		//curTask = NULL;
	}
};

class BufferProcess :
	public TaskDispatcher<TaskRequest>,
	private TimerObject,
	public PurgeNodeNotifier,
	public CacheTransation
{
	protected: // base members
		// cache chain control
		RequestOutput<TaskRequest> output;
		RequestOutput<TaskRequest> remoteoutput;//将请求传给远端dtc，用于migrate命令	
		RequestOutput<TaskRequest> hblogoutput; // hblog task output 
		BufferReplyNotify cacheReply;

		// table info
		DTCTableDefinition *tableDef;
		// cache memory management
		DTCBufferPool Cache;
		DataProcess* pstDataProcess;
		CacheInfo cacheInfo;

		// no backup db
		bool nodbMode;
		// full cache
		bool fullMode;
		bool lossyMode;
		// treat empty key as default value, flat bitmap emulation
		bool m_bReplaceEmpty;
		// lru update level
		int noLRU;
		// working mode
		EUpdateMode asyncServer;
		EUpdateMode updateMode;
		EUpdateMode insertMode;
		/*indicate mem dirty when start with sync dtc*/
		bool mem_dirty;
		// server side sorting
		unsigned char insertOrder;

		// cache protection
		int nodeSizeLimit;  //node size limit
		int nodeRowsLimit;  //node rows limit
		int nodeEmptyLimit;  //empty nodes limit

		// generated error message
		char szErrMsg[256];

		int maxExpireCount;
		int maxExpireTime;
		

	protected: // stat subsystem
		StatItemU32 statGetCount;
		StatItemU32 statGetHits;
		StatItemU32 statInsertCount;
		StatItemU32 statInsertHits;
		StatItemU32 statUpdateCount;
		StatItemU32 statUpdateHits;
		StatItemU32 statDeleteCount;
		StatItemU32 statDeleteHits;
		StatItemU32 statPurgeCount;

		StatItemU32 statDropCount;
		StatItemU32 statDropRows;
		StatItemU32 statFlushCount;
		StatItemU32 statFlushRows;
		StatSample  statIncSyncStep;

		StatItemU32 statMaxFlushReq;
		StatItemU32 statCurrFlushReq;
		StatItemU32 statOldestDirtyTime;
		StatItemU32 statAsyncFlushCount;

		StatItemU32 statExpireCount;
		StatItemU32 statBufferProcessExpireCount;
	protected: // async flush members
		FlushReplyNotify flushReply;
		TimerList *flushTimer;
		volatile int nFlushReq; // current pending node
		volatile int mFlushReq; // pending node limit
		volatile unsigned short maxFlushReq; // max speed
		volatile unsigned short markerInterval;
		volatile int minDirtyTime;
		volatile int maxDirtyTime;
		// async log writer
		int noAsyncLog;


	protected:
		//空节点过滤
		EmptyNodeFilter *m_pstEmptyNodeFilter;

	protected:
		// Hot Backup
		//记录更新key
		bool      hbLogSwitch;
		//记录lru变更
	
		HBFeature* hbFeature;
		// Hot Backup

	protected:
		// BlackList
		BlackListUnit *blacklist;
		TimerList     *blacklist_timer;
		// BlackList

		ExpireTime	*key_expire;
		TimerList	*key_expire_timer;
		HotBackReplay hotbackReply;
	private:
		// level 1 processing
		// GET entrance
		BufferResult buffer_get_data	(TaskRequest &Task);
		// GET batch entrance
		BufferResult buffer_batch_get_data	(TaskRequest &Task);
		// GET response, DB --> cache
		BufferResult buffer_replace_result	(TaskRequest &Task);
		// GET response, DB --> client
		BufferResult buffer_get_rb	(TaskRequest &Task);

		// implementation some admin/purge/flush function
		BufferResult buffer_process_admin(TaskRequest &Task);
		BufferResult buffer_purge_data	(TaskRequest &Task);
		BufferResult buffer_flush_data(TaskRequest &Task);
		BufferResult buffer_flush_data_before_delete(TaskRequest &Task);
		int buffer_flush_data_timer(Node& stNode, unsigned int& uiFlushRowsCnt);
		BufferResult buffer_flush_data(Node& stNode, TaskRequest* pstTask, unsigned int& uiFlushRowsCnt);

		// sync mode operation, called by reply
		BufferResult buffer_sync_insert_precheck (TaskRequest& task);
		BufferResult buffer_sync_insert (TaskRequest& task);
		BufferResult buffer_sync_update (TaskRequest& task);
		BufferResult buffer_sync_replace (TaskRequest& task);
		BufferResult buffer_sync_delete (TaskRequest& task);

		// async mode operation, called by entrance
		BufferResult buffer_async_insert (TaskRequest& task);
		BufferResult buffer_async_update (TaskRequest& task);
		BufferResult buffer_async_replace (TaskRequest& task);

		// fullcache mode operation, called by entrance
		BufferResult buffer_nodb_insert (TaskRequest& task);
		BufferResult buffer_nodb_update (TaskRequest& task);
		BufferResult buffer_nodb_replace (TaskRequest& task);
		BufferResult buffer_nodb_delete (TaskRequest& task);

		// level 2 operation
		// level 2: INSERT with async compatible, create node & clear empty filter
		BufferResult buffer_insert_row	(TaskRequest &Task, bool async, bool setrows);
		// level 2: UPDATE with async compatible, accept empty node only if EmptyAsDefault
		BufferResult buffer_update_rows	(TaskRequest &Task, bool async, bool setrows);
		// level 2: REPLACE with async compatible, don't allow empty node
		BufferResult buffer_replace_rows	(TaskRequest &Task, bool async, bool setrows);
		// level 2: DELETE has no async mode, don't allow empty node
		BufferResult buffer_delete_rows	(TaskRequest &Task);

		// very low level
		// 空结点inset default值进cache内存
		// auto clear empty filter
		BufferResult InsertDefaultRow(TaskRequest &Task);
		bool InsertEmptyNode(void);

		// 热备操作
		BufferResult buffer_register_hb(TaskRequest &Task);
		BufferResult buffer_logout_hb(TaskRequest &Task);
		BufferResult buffer_get_key_list(TaskRequest &Task);
		BufferResult buffer_get_update_key(TaskRequest &Task);
		BufferResult buffer_get_raw_data(TaskRequest &Task);
		BufferResult buffer_replace_raw_data(TaskRequest &Task);
		BufferResult buffer_adjust_lru(TaskRequest &Task);
		BufferResult buffer_verify_hbt(TaskRequest &Task);
		BufferResult buffer_get_hbt(TaskRequest &Task);

		//内存整理操作
		BufferResult buffer_nodehandlechange(TaskRequest &Task);

		// column expand related
		BufferResult buffer_check_expand_status(TaskRequest &Task);
		BufferResult buffer_column_expand(TaskRequest &Task);
		BufferResult buffer_column_expand_done(TaskRequest &Task);
		BufferResult buffer_column_expand_key(TaskRequest &Task);

		//迁移操作
		BufferResult buffer_migrate(TaskRequest &Task);

		// clear cache(only support nodb mode)
		BufferResult buffer_clear_cache(TaskRequest &Task);

		/* we can still purge clean node if hit ratio is ok */
		BufferResult cache_purgeforhit(TaskRequest &Task);

		//rows限制
		BufferResult check_allowed_insert(TaskRequest &Task);

		BufferResult buffer_query_serverinfo(TaskRequest &Task);

		// 主从复制操作
		BufferResult buffer_process_replicate(TaskRequest &Task);

		// 热备日志
		int write_hb_log(const char* key, char *pstChunk, unsigned int uiNodeSize, int iType);
		int write_hb_log(const char* key, Node& stNode, int iType);
		int write_hb_log(TaskRequest &Task, Node& stNode, int iType);
		int	write_lru_hb_log(const char* key);
	public:
		virtual void purge_node_notify(const char *key, Node node);
		/* inc flush task stat(created by flush dirty node function) */
		void inc_async_flush_stat() { statAsyncFlushCount++; }

	private:
		virtual void task_notify(TaskRequest *);
		void reply_notify(TaskRequest *);

		// flush internal
		virtual void timer_notify(void);
		int flush_next_node(void);
		void delete_tail_time_markers();
		void get_dirty_stat();
		void calculate_flush_speed(int is_flush_timer);
		MARKER_STAMP calculate_current_marker();

		BufferProcess (const BufferProcess& robj);
		BufferProcess& operator= (const BufferProcess& robj);

	public:
		BufferProcess (PollThread *, DTCTableDefinition *, EUpdateMode async);
		~BufferProcess (void);

		const DTCTableDefinition *table_definition(void) const { return tableDef; }
		const char *last_error_message(void) const { return szErrMsg[0] ? szErrMsg : "unknown error"; }

		void set_limit_node_size(int node_size) {
			nodeSizeLimit = node_size;
		}

		/* 0 =  no limit */
		void set_limit_node_rows(int rows) {
			nodeRowsLimit = rows < 0 ? 0 : rows;
			return;
		}

		/*
		 * 0 = no limit,
		 * 1-999: invalid, use 1000 instead
		 * 1000-1G: max empty node count
		 * >1G: invalid, no limit
		 */
		void set_limit_empty_nodes(int nodes) {
			nodeEmptyLimit = nodes <= 0 ? 0 :
				nodes < 1000 ? 1000 :
				nodes > (1<<30) ? 0 :
				nodes;
			return;
		}

		void disable_auto_purge(void) {
			Cache.disable_try_purge();
		}

		void set_date_expire_alert_time(int time) {
			Cache.set_date_expire_alert_time(time);
		}

		/*************************************************
		Description:	设置cache内存大小以及版本
		Input:		cacheSize	共享内存的大小
		createVersion	cache内存版本号
		Output:
		Return:		0为成功，其他值为错误
		 *************************************************/
		int buffer_set_size(unsigned long cacheSize, unsigned int createVersion);

		/*************************************************
		Description:	打开共享内存并初始化
		Input:		iIpcKey	共享内存的key
		Output:
		Return:		0为成功，其他值为错误
		 *************************************************/
		int cache_open(int iIpcKey, int iEnableEmptyFilter, int iEnableAutoDeleteDirtyShm);

		int update_mode(void) const { return updateMode; }
		int enable_no_db_mode(void);
		void enable_lossy_data_source(int v) { lossyMode = v == 0 ? false : true; }
		int disable_lru_update(int);
		int disable_async_log(int);

		/*************************************************
		Description:	处理task请求
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，CACHE_PROCESS_NEXT为转交helper处理，CACHE_PROCESS_PENDING为flush请求需要等待
		BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_process_request(TaskRequest &Task);

		/*************************************************
		Description:	处理helper的回应
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_process_reply(TaskRequest &Task);

		/*************************************************
		Description:	处理helper的回应
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_process_batch(TaskRequest &Task);

		/*************************************************
		Description:	处理helper的回应
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_process_nodb(TaskRequest &Task);

		/*************************************************
		Description:	处理flush请求的helper回应
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_flush_reply(TaskRequest &Task);

		/*************************************************
		Description:	task出错的处理
		Input:		Task	task请求
		Output:
		Return:		BUFFER_PROCESS_OK为成功，BUFFER_PROCESS_ERROR为错误
		 *************************************************/
		BufferResult  buffer_process_error(TaskRequest &Task);

		void print_row(const RowValue *r);
		int set_insert_order(int o);
		void set_replace_empty(bool v) { m_bReplaceEmpty = v; }

		// stage relate
		void bind_dispatcher(TaskDispatcher<TaskRequest> *p) { output.bind_dispatcher(p); }
		void bind_dispatcher_remote(TaskDispatcher<TaskRequest> *p) { remoteoutput.bind_dispatcher(p); }
		void bind_hb_log_dispatcher(TaskDispatcher<TaskRequest> *p) { hblogoutput.bind_dispatcher(p); }

		// flush api
		void set_flush_parameter(int, int, int, int);
		void set_drop_count(int); // to be remove
		int commit_flush_request(DTCFlushRequest *, TaskRequest*);
		void complete_flush_request(DTCFlushRequest *);
		void push_flush_queue(TaskRequest *p) {  p->push_reply_dispatcher(&flushReply); output.indirect_notify(p); }
		inline bool is_mem_dirty() {return mem_dirty;}
		int oldest_dirty_node_alarm();

		// expire
		BufferResult check_and_expire(TaskRequest &Task);


		friend class TaskPendingList;
		friend class BufferReplyNotify;

public:
	// transation implementation
		inline void transation_begin(TaskRequest *task) { CacheTransation::Init(task); }
		void transation_end(void);
		inline int transation_find_node(TaskRequest &task);
		inline void transation_update_lru(bool async, int type);
		void dispatch_hot_back_task(TaskRequest *task)
		{
			
			task->push_reply_dispatcher(&hotbackReply);
			hblogoutput.task_notify(task);		
		}
};


DTC_END_NAMESPACE

#endif
