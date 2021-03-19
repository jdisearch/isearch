/*
 * =====================================================================================
 *
 *       Filename:  buffer_pool.h
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
#ifndef __DTC_CACHE_POOL_H
#define __DTC_CACHE_POOL_H

#include <stddef.h>
#include "stat_dtc.h"
#include "namespace.h"
#include "pt_malloc.h"
#include "shmem.h"
#include "global.h"
#include "node_list.h"
#include "node_index.h"
#include "node_set.h"
#include "feature.h"
#include "ng_info.h"
#include "hash.h"
#include "col_expand.h"
#include "node.h"
#include "timer_list.h"
#include "data_chunk.h"

DTC_BEGIN_NAMESPACE

/* time-marker node in dirty lru list */
//#define TIME_MARKER_NEXT_NODE_ID INVALID_NODE_ID
#define TIME_MARKER_NEXT_NODE_ID (INVALID_NODE_ID-1)

/* cache基本信息 */
typedef struct _CacheInfo{
	int		ipcMemKey;		// 共享内存key
	uint64_t	ipcMemSize;		// 共享内存大小
	unsigned short	keySize;		// key大小
	unsigned char	version;		// 内存版本号
	unsigned char	syncUpdate:1; 		// 同异步模式
	unsigned char	readOnly:1;		// 只读模式打开
	unsigned char	createOnly:1;		// 供mem_tool使用
	unsigned char   emptyFilter:1;		// 是否启用空节点过滤功能
	unsigned char   autoDeleteDirtyShm:1;	// 是否需要在检出到内存不完整时自动删除并重建共享内存
	unsigned char	forceUpdateTableConf:1;	// 是否需要强制使用table.conf更新共享内存中的配置

	inline void Init(int keyFormat, unsigned long cacheSize, unsigned int createVersion)
	{
		// calculate buckettotal
		keySize = keyFormat;
		ipcMemSize = cacheSize;
		version = createVersion;
	}

} CacheInfo;

class PurgeNodeNotifier {
	public:
		PurgeNodeNotifier(){};
		virtual ~PurgeNodeNotifier(){};
		virtual void purge_node_notify(const char *key, Node node) = 0;
};

class BufferProcess;
class RawDataProcess;
class TreeDataProcess;
class DTCBufferPool : private TimerObject
{
	protected:
		PurgeNodeNotifier  *_purge_notifier;
		SharedMemory   _shm;		//共享内存管理器
		CacheInfo       _cacheInfo;	//cache基本信息

		DTCHash          *_hash;	        // hash桶
		NGInfo        *_ngInfo;	// node管理
		Feature       *_feature;	// 特性抽象
		NodeIndex     *_nodeIndex;   	// NodeID转换
		//CTableInfo   *_tableInfo;	// Table信息
		DTCColExpand     *_colExpand;

		char            _errmsg[256];
		int		_need_set_integrity;

		/* 待淘汰节点数目 */
		unsigned _need_purge_node_count;

		TimerList * _delay_purge_timerlist;
		unsigned firstMarkerTime;
		unsigned lastMarkerTime;
		int emptyLimit;
        /**********for purge alert*******/
		int disableTryPurge;
        //如果自动淘汰的数据最后更新时间比当前时间减DataExpireAlertTime小则报警
		int dateExpireAlertTime;
		

	protected:
		/* for statistic*/
		StatItemU32	statCacheSize;
		StatItemU32	statCacheKey;
		StatItemU32	statCacheVersion;
		StatItemU32	statUpdateMode;
		StatItemU32	statEmptyFilter;
		StatItemU32	statHashSize;
		StatItemU32	statFreeBucket;
		StatItemU32	statDirtyEldest;
		StatItemU32	statDirtyAge;
		StatSample	statTryPurgeCount;
		StatItemU32	statTryPurgeNodes;
		StatItemU32	statLastPurgeNodeModTime;//最后被淘汰的节点的lastcmod的最大值(如果多行)
		StatItemU32	statDataExistTime;//当前时间减去statLastPurgeNodeModTime
		StatSample  survival_hour;  	
		StatSample  statPurgeForCreateUpdateCount;  	
	private:
		int app_storage_open();
		int dtc_mem_open(APP_STORAGE_T *);
		int dtc_mem_attach(APP_STORAGE_T *);
		int dtc_mem_init(APP_STORAGE_T *);
		int verify_cache_info(CacheInfo *);
		unsigned int hash_bucket_num(uint64_t);

		int remove_from_hash_base(const char *key, Node node, int newhash);
		int remove_from_hash(const char *key, Node node);
		int move_to_new_hash(const char *key, Node node);
		int Insert2Hash(const char *key, Node node);

		int purge_node(const char *key, Node purge_node);
		int purge_node_everything(const char* key, Node purge_node);
        
        /* purge alert*/
        int check_and_purge_node_everything(Node purge_node);
        uint32_t get_cmodtime(Node* purge_node);

		uint32_t get_expire_time(Node *node, uint32_t &expire);

		/* lru list op */
		int insert2_dirty_lru(Node node) {return _ngInfo->insert2_dirty_lru(node);}
		int insert2_clean_lru(Node node) {return _ngInfo->insert2_clean_lru(node);}
		int insert2_empty_lru(Node node) {
			return emptyLimit ?
				_ngInfo->insert2_empty_lru(node) :
				_ngInfo->insert2_clean_lru(node) ;

		}
		int remove_from_lru(Node node)   {return _ngInfo->remove_from_lru(node);}
		int key_cmp(const char *key, const char *other);

		/* node|row count statistic for async flush.*/
		void inc_dirty_node(int v){ _ngInfo->inc_dirty_node(v);}
		void inc_dirty_row(int v) { _ngInfo->inc_dirty_row(v); }
		void dec_empty_node(void) { if(emptyLimit) _ngInfo->inc_empty_node(-1); }
		void inc_empty_node(void) {
			if(emptyLimit) {
				_ngInfo->inc_empty_node(1);
				if(_ngInfo->empty_count() > emptyLimit) {
					purge_single_empty_node();
				}
			}
		}

		const unsigned int total_dirty_node() const {return _ngInfo->total_dirty_node();}

		const uint64_t total_dirty_row() const {return _ngInfo->total_dirty_row();}
		const uint64_t total_used_row() const {return _ngInfo->total_used_row();}

		/*定期调度delay purge任务*/
		virtual void timer_notify(void);

	public:
		DTCBufferPool(PurgeNodeNotifier *o = NULL);
		~DTCBufferPool();

		int check_expand_status();
		unsigned char shm_table_idx();
		bool col_expand(const char *table, int len);
		int try_col_expand(const char *table, int len);
		bool reload_table();

		int cache_open(CacheInfo *);
		void set_empty_node_limit(int v) { emptyLimit = v<0?0:v; }
		int init_empty_node_list(void);
		int upgrade_empty_node_list(void);
		int merge_empty_node_list(void);
		int prune_empty_node_list(void);
		int shrink_empty_node_list(void);
		int purge_single_empty_node(void);

		Node cache_find(const char *key, int newhash);
		Node cache_find_auto_chose_hash(const char *key);
		int   cache_purge(const char *key);
		int purge_node_everything(Node purge_node);
		Node cache_allocate(const char *key);
		int   try_purge_size(size_t size, Node purge_node, unsigned count=2500);
		void  disable_try_purge(void) { disableTryPurge = 1; }
        void  set_date_expire_alert_time(int time){dateExpireAlertTime = time<0?0:time;};

		/* 淘汰固定个节点 */
		void delay_purge_notify(const unsigned count=50);
		int pre_purge_nodes(int purge_cnt, Node reserve);
		int purge_by_time(unsigned int oldesttime);
		void start_delay_purge_task(TimerList *);

		int   insert_time_marker(unsigned int);
		int   remove_time_marker(Node node);
		int   is_time_marker(Node node) const;
		Node first_time_marker() const;
		Node last_time_marker()  const;
		unsigned int first_time_marker_time();
		unsigned int last_time_marker_time();

		Node dirty_lru_head() const;
		Node clean_lru_head() const;
		Node empty_lru_head() const;
		int   dirty_lru_empty()const{return NODE_LIST_EMPTY(dirty_lru_head());}

		const CacheInfo* get_cache_info() const { return &_cacheInfo;}
		const char *Error(void) const { return _errmsg; }

		FEATURE_INFO_T* query_feature_by_id(const uint32_t id) 
		{
			return _feature ? _feature->get_feature_by_id(id):(FEATURE_INFO_T *)(0);
		}

		int add_feature(const uint32_t id, const MEM_HANDLE_T v)
		{
			if(_feature == NULL)
				return -1;
			return _feature->add_feature(id, v);
		}

		int clear_create();

		uint32_t max_node_id(void) const{
			return _ngInfo->max_node_id();
		}

		NODE_ID_T min_valid_node_id(void) const {
			return  _ngInfo->min_valid_node_id();
		}

		const unsigned int total_used_node() const {return _ngInfo->total_used_node();}
		void inc_total_row(int v) { _ngInfo->inc_total_row(v); }

		static int32_t node_rows_count(Node node) {
			if(!node || node.vd_handle() == INVALID_HANDLE)
				return 0;

			DataChunk *chunk = ((DataChunk*)(DTCBinMalloc::Instance()->handle_to_ptr(node.vd_handle())));
			if(!chunk) return 0;

			return chunk->total_rows();
		}

		friend class BufferProcess;
		friend class RawDataProcess;
		friend class TreeDataProcess;
};

DTC_END_NAMESPACE

#endif

