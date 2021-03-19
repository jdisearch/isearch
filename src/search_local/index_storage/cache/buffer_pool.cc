/*
 * =====================================================================================
 *
 *       Filename:  buffer_pool.cc
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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "pt_malloc.h"
#include "namespace.h"
#include "buffer_pool.h"
#include "data_chunk.h"
#include "empty_filter.h"
#include "task_request.h"
#include "dtc_global.h"
#include "relative_hour_calculator.h"
#include "table_def_manager.h"

extern DTCTableDefinition *gTableDef[];
extern int hashChanging;
extern int targetNewHash;

DTC_USING_NAMESPACE

DTCBufferPool::DTCBufferPool(PurgeNodeNotifier *pn) : _purge_notifier(pn)
{
    memset(&_cacheInfo, 0x00, sizeof(CacheInfo));

    _hash = 0;
    _ngInfo = 0;
    _feature = 0;
    _nodeIndex = 0;
    _colExpand = 0;

    memset(_errmsg, 0, sizeof(_errmsg));
    _need_set_integrity = 0;
    _need_purge_node_count = 0;

    _delay_purge_timerlist = NULL;
    firstMarkerTime = lastMarkerTime = 0;
    emptyLimit = 0;
    disableTryPurge = 0;
    survival_hour = statmgr.get_sample(DATA_SURVIVAL_HOUR_STAT);
}

DTCBufferPool::~DTCBufferPool()
{
    _hash->Destroy();
    _ngInfo->Destroy();
    _feature->Destroy();
    _nodeIndex->Destroy();

    /* 运行到这里，说明程序是正常stop的，设置共享内存完整性标记 */
    if (_need_set_integrity)
    {
        log_notice("Share Memory Integrity... ok");
        DTCBinMalloc::Instance()->set_share_memory_integrity(1);
    }
}

/* 检查lru链表是否cross-link了，一旦发生这种情况，没法处理了 :( */
static inline int check_cross_linked_lru(Node node)
{
    Node v = node.Prev();

    if (v == node)
    {
        log_crit("BUG: cross-linked lru list");
        return -1;
    }
    return 0;
}

/* 验证cacheInfo合法性, 避免出现意外 */
int DTCBufferPool::verify_cache_info(CacheInfo *info)
{
    if (INVALID_HANDLE != 0UL)
    {
        snprintf(_errmsg, sizeof(_errmsg), "PANIC: invalid handle must be 0UL");
        return -1;
    }

    if (INVALID_NODE_ID != (NODE_ID_T)(-1))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "PANIC: invalid node id must be %u, but it is %u now",
                 (NODE_ID_T)(-1), INVALID_NODE_ID);
        return -1;
    }

    if (info->version != 4)
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "only support cache version >= 4");
        return -1;
    }

    /* 系统可工作的最小内存 */
    /* 1. emptyFilter = 0  Min=64M */
    /* 2. emptyFilter = 1  Min=256M, 初步按照1.5G用户来计算 */

    if (info->emptyFilter)
    {
        if (info->ipcMemSize < (256UL << 20))
        {
            snprintf(_errmsg, sizeof(_errmsg),
                     "Empty-Node Filter function need min 256M mem");
            return -1;
        }
    }

    if (info->ipcMemSize < (64UL << 20))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "too small mem size, need min 64M mem");
        return -1;
    }

    /* size check on 32bits platform*/
    if (sizeof(long) == 4)
    {
        if (info->ipcMemSize >= UINT_MAX)
        {
            snprintf(_errmsg, sizeof(_errmsg),
                     "cache size " UINT64FMT "exceed 4G, Please upgrade to 64 bit version",
                     info->ipcMemSize);
            return -1;
        }
    }

    /* support max 64G memory size*/
    if (info->ipcMemSize > (64ULL << 30))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "cache size exceed 64G, unsupported");
        return -1;
    }

    return 0;
}

int DTCBufferPool::check_expand_status()
{
    if (!_colExpand)
    {
        snprintf(_errmsg, sizeof(_errmsg), "column expand not support");
        return -2;
    }
    if (_colExpand->is_expanding())
    {
        snprintf(_errmsg, sizeof(_errmsg), "column expanding, please later");
        return -1;
    }
    return 0;
}

unsigned char DTCBufferPool::shm_table_idx()
{
    if (!_colExpand)
    {
        // column not supported, always return 0
        return 0;
    }

    return _colExpand->cur_table_idx();
}

bool DTCBufferPool::col_expand(const char *table, int len)
{
    if (!_colExpand->expand(table, len))
    {
        snprintf(_errmsg, sizeof(_errmsg), "column expand error");
        return false;
    }

    return true;
}

int DTCBufferPool::try_col_expand(const char *table, int len)
{
    return _colExpand->try_expand(table, len);
}

bool DTCBufferPool::reload_table()
{
    if (!_colExpand)
    {
        return true;
    }
    if (!_colExpand->reload_table())
    {
        snprintf(_errmsg, sizeof(_errmsg), "reload table error");
        return false;
    }
    return true;
}

int DTCBufferPool::cache_open(CacheInfo *info)
{
TRY_CACHE_INIT_AGAIN:
    if (info->readOnly == 0)
    {
        if (verify_cache_info(info) != 0)
            return -1;
        memcpy((char *)&_cacheInfo, info, sizeof(CacheInfo));
    }
    else
    {
        memset((char *)&_cacheInfo, 0, sizeof(CacheInfo));
        _cacheInfo.readOnly = 1;
        _cacheInfo.keySize = info->keySize;
        _cacheInfo.ipcMemKey = info->ipcMemKey;
    }

    //初始化统计对象
    statCacheSize = statmgr.get_item_u32(DTC_CACHE_SIZE);
    statCacheKey = statmgr.get_item_u32(DTC_CACHE_KEY);
    statCacheVersion = statmgr.get_item(DTC_CACHE_VERSION);
    statUpdateMode = statmgr.get_item_u32(DTC_UPDATE_MODE);
    statEmptyFilter = statmgr.get_item_u32(DTC_EMPTY_FILTER);
    statHashSize = statmgr.get_item_u32(DTC_BUCKET_TOTAL);
    statFreeBucket = statmgr.get_item_u32(DTC_FREE_BUCKET);
    statDirtyEldest = statmgr.get_item_u32(DTC_DIRTY_ELDEST);
    statDirtyAge = statmgr.get_item_u32(DTC_DIRTY_AGE);
    statTryPurgeCount = statmgr.get_sample(TRY_PURGE_COUNT);
    statPurgeForCreateUpdateCount = statmgr.get_sample(PURGE_CREATE_UPDATE_STAT);
    statTryPurgeNodes = statmgr.get_item_u32(TRY_PURGE_NODES);
    statLastPurgeNodeModTime = statmgr.get_item_u32(LAST_PURGE_NODE_MOD_TIME);
    statDataExistTime = statmgr.get_item_u32(DATA_EXIST_TIME);

    //打开共享内存
    if (_shm.Open(_cacheInfo.ipcMemKey) > 0)
    {
        //共享内存已存在

        if (_cacheInfo.createOnly)
        {
            snprintf(_errmsg, sizeof(_errmsg), "m_shm already exist");
            return -1;
        }

        if (_cacheInfo.readOnly == 0 && _shm.Lock() != 0)
        {
            snprintf(_errmsg, sizeof(_errmsg), "m_shm.Lock() failed");
            return -1;
        }

        if (_shm.Attach(_cacheInfo.readOnly) == NULL)
        {
            snprintf(_errmsg, sizeof(_errmsg), "m_shm.Attach() failed");
            return -1;
        }

        //底层分配器
        if (DTCBinMalloc::Instance()->Attach(_shm.Ptr(), _shm.Size()) != 0)
        {
            snprintf(_errmsg,
                     sizeof(_errmsg),
                     "binmalloc attach failed: %s",
                     M_ERROR());
            return -1;
        }

        //内存版本检测, 目前因为底层分配器的缘故，只支持version >= 4的版本
        _cacheInfo.version = DTCBinMalloc::Instance()->detect_version();
        if (_cacheInfo.version != 4)
        {
            snprintf(_errmsg, sizeof(_errmsg), "unsupport version, %d", _cacheInfo.version);
            return -1;
        }

        /* 检查共享内存完整性，通过*/
        if (DTCBinMalloc::Instance()->share_memory_integrity())
        {
            log_notice("Share Memory Integrity Check.... ok");
            /* 
             * 设置共享内存不完整标记
             *
             * 这样可以在程序coredump引起内存混乱时，再次重启后dtc能发现内存已经写乱了。
             */
            if (_cacheInfo.readOnly == 0)
            {
                _need_set_integrity = 1;
                DTCBinMalloc::Instance()->set_share_memory_integrity(0);
            }
        }
        /* 不通过 */
        else
        {
            log_warning("Share Memory Integrity Check... failed");

            if (_cacheInfo.autoDeleteDirtyShm)
            {
                if (_cacheInfo.readOnly == 1)
                {
                    log_error("ReadOnly Share Memory is Confuse");
                    return -1;
                }

                /* 删除共享内存，重新启动cache初始化流程 */
                if (_shm.Delete() < 0)
                {
                    log_error("Auto Delete Share Memory failed: %m");
                    return -1;
                }

                log_notice("Auto Delete Share Memory Success, Try Rebuild");

                _shm.Unlock();

                DTCBinMalloc::Destroy();

                /* 重新初始化 */
                goto TRY_CACHE_INIT_AGAIN;
            }
        }
    }

    //共享内存不存在，需要创建
    else
    {
        //只读，失败
        if (_cacheInfo.readOnly)
        {
            snprintf(_errmsg, sizeof(_errmsg), "readonly m_shm non-exists");
            return -1;
        }

        //创建
        if (_shm.Create(_cacheInfo.ipcMemKey, _cacheInfo.ipcMemSize) <= 0)
        {
            if (errno == EACCES || errno == EEXIST)
                snprintf(_errmsg, sizeof(_errmsg), "m_shm exists but unwritable");
            else
                snprintf(_errmsg, sizeof(_errmsg), "create m_shm failed: %m");
            return -1;
        }

        if (_shm.Lock() != 0)
        {
            snprintf(_errmsg, sizeof(_errmsg), "m_shm.Lock() failed");
            return -1;
        }

        if (_shm.Attach() == NULL)
        {
            snprintf(_errmsg, sizeof(_errmsg), "m_shm.Attach() failed");
            return -1;
        }

        //底层分配器初始化
        if (DTCBinMalloc::Instance()->Init(_shm.Ptr(), _shm.Size()) != 0)
        {
            snprintf(_errmsg,
                     sizeof(_errmsg),
                     "binmalloc init failed: %s",
                     M_ERROR());
            return -1;
        }

        /* 
         * 设置共享内存不完整标记
         */
        _need_set_integrity = 1;
        DTCBinMalloc::Instance()->set_share_memory_integrity(0);
    }

    /* statistic */
    statCacheSize = _cacheInfo.ipcMemSize;
    statCacheKey = _cacheInfo.ipcMemKey;
    statCacheVersion = _cacheInfo.version;
    statUpdateMode = _cacheInfo.syncUpdate;
    statEmptyFilter = _cacheInfo.emptyFilter;
    /*set minchunksize*/
    DTCBinMalloc::Instance()->set_min_chunk_size(DTCGlobal::_min_chunk_size);

    //attention: invoke app_storage_open() must after DTCBinMalloc init() or attach().
    return app_storage_open();
}

int DTCBufferPool::app_storage_open()
{
    APP_STORAGE_T *storage = M_POINTER(APP_STORAGE_T, DTCBinMalloc::Instance()->get_reserve_zone());
    if (!storage)
    {
        snprintf(_errmsg,
                 sizeof(_errmsg),
                 "get reserve zone from binmalloc failed: %s",
                 M_ERROR());

        return -1;
    }

    return dtc_mem_open(storage);
}

int DTCBufferPool::dtc_mem_open(APP_STORAGE_T *storage)
{
    if (storage->need_format())
    {
        log_debug("starting init dtc mem");
        return dtc_mem_init(storage);
    }

    return dtc_mem_attach(storage);
}

/* hash size = 1% total memory size */
/* return hash bucket num*/

uint32_t DTCBufferPool::hash_bucket_num(uint64_t size)
{
    int h = (uint32_t)(size / 100 - 16) / sizeof(NODE_ID_T);
    h = (h / 9) * 9;
    return h;
}

int DTCBufferPool::dtc_mem_init(APP_STORAGE_T *storage)
{
    _feature = Feature::Instance();
    if (!_feature || _feature->Init(MIN_FEATURES))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "init feature failed, %s",
                 _feature->Error());
        return -1;
    }

    if (storage->Format(_feature->Handle()))
    {
        snprintf(_errmsg, sizeof(_errmsg), "format storage failed");
        return -1;
    }

    /* Node-Index*/
    _nodeIndex = NodeIndex::Instance();
    if (!_nodeIndex || _nodeIndex->Init(_cacheInfo.ipcMemSize))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "init node-index failed, %s",
                 _nodeIndex->Error());
        return -1;
    }

    /* Hash-Bucket */
    _hash = DTCHash::Instance();
    if (!_hash || _hash->Init(hash_bucket_num(_cacheInfo.ipcMemSize), _cacheInfo.keySize))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "init hash-bucket failed, %s",
                 _hash->Error());
        return -1;
    }
    statHashSize = _hash->hash_size();
    statFreeBucket = _hash->free_bucket();

    /* NS-Info */
    _ngInfo = NGInfo::Instance();
    if (!_ngInfo || _ngInfo->Init())
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "init ns-info failed, %s",
                 _ngInfo->Error());
        return -1;
    }

    /* insert features*/
    if (_feature->add_feature(NODE_INDEX, _nodeIndex->Handle()))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "add node-index feature failed, %s",
                 _feature->Error());
        return -1;
    }

    if (_feature->add_feature(HASH_BUCKET, _hash->Handle()))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "add hash-bucket feature failed, %s",
                 _feature->Error());
        return -1;
    }

    if (_feature->add_feature(NODE_GROUP, _ngInfo->Handle()))
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "add node-group feature failed, %s",
                 _feature->Error());
        return -1;
    }

    /* Empty-Node Filter*/
    if (_cacheInfo.emptyFilter)
    {
        EmptyNodeFilter *p = EmptyNodeFilter::Instance();
        if (!p || p->Init())
        {
            snprintf(_errmsg, sizeof(_errmsg),
                     "start Empty-Node Filter failed, %s",
                     p->Error());
            return -1;
        }

        if (_feature->add_feature(EMPTY_FILTER, p->Handle()))
        {
            snprintf(_errmsg, sizeof(_errmsg),
                     "add empty-filter feature failed, %s",
                     _feature->Error());
            return -1;
        }
    }

    // column expand
    _colExpand = DTCColExpand::Instance();
    if (!_colExpand || _colExpand->Init())
    {
        snprintf(_errmsg, sizeof(_errmsg), "init column expand failed, %s", _colExpand->Error());
        return -1;
    }
    if (_feature->add_feature(COL_EXPAND, _colExpand->Handle()))
    {
        snprintf(_errmsg, sizeof(_errmsg), "add column expand feature failed, %s", _feature->Error());
        return -1;
    }

    statDirtyEldest = 0;
    statDirtyAge = 0;

    return 0;
}

int DTCBufferPool::dtc_mem_attach(APP_STORAGE_T *storage)
{

    _feature = Feature::Instance();
    if (!_feature || _feature->Attach(storage->as_extend_info))
    {
        snprintf(_errmsg, sizeof(_errmsg), "%s", _feature->Error());
        return -1;
    }

    /*hash-bucket*/
    FEATURE_INFO_T *p = _feature->get_feature_by_id(HASH_BUCKET);
    if (!p)
    {
        snprintf(_errmsg, sizeof(_errmsg), "not found hash-bucket feature");
        return -1;
    }
    _hash = DTCHash::Instance();
    if (!_hash || _hash->Attach(p->fi_handle))
    {
        snprintf(_errmsg, sizeof(_errmsg), "%s", _hash->Error());
        return -1;
    }
    statHashSize = _hash->hash_size();
    statFreeBucket = _hash->free_bucket();

    /*node-index*/
    p = _feature->get_feature_by_id(NODE_INDEX);
    if (!p)
    {
        snprintf(_errmsg, sizeof(_errmsg), "not found node-index feature");
        return -1;
    }
    _nodeIndex = NodeIndex::Instance();
    if (!_nodeIndex || _nodeIndex->Attach(p->fi_handle))
    {
        snprintf(_errmsg, sizeof(_errmsg), "%s", _nodeIndex->Error());
        return -1;
    }

    /*ns-info*/
    p = _feature->get_feature_by_id(NODE_GROUP);
    if (!p)
    {
        snprintf(_errmsg, sizeof(_errmsg), "not found ns-info feature");
        return -1;
    }
    _ngInfo = NGInfo::Instance();
    if (!_ngInfo || _ngInfo->Attach(p->fi_handle))
    {
        snprintf(_errmsg, sizeof(_errmsg), "%s", _ngInfo->Error());
        return -1;
    }

    Node stLastTime = last_time_marker();
    Node stFirstTime = first_time_marker();
    if (!(!stLastTime) && !(!stFirstTime))
    {
        statDirtyEldest = stLastTime.Time();
        statDirtyAge = stFirstTime.Time() - stLastTime.Time();
    }

    //TODO tableinfo
    // column expand
    p = _feature->get_feature_by_id(COL_EXPAND);
    if (p)
    {
        _colExpand = DTCColExpand::Instance();
        if (!_colExpand || _colExpand->Attach(p->fi_handle, _cacheInfo.forceUpdateTableConf))
        {
            // if _colExpand if null
            snprintf(_errmsg, sizeof(_errmsg), "%s", _colExpand->Error());
            return -1;
        }
    }
    else
    {
        log_error("column expand feature not enable, do not support column expand");
        _colExpand = NULL;
    }
    return 0;
}

// Sync the empty node statstics
int DTCBufferPool::init_empty_node_list(void)
{
    if (_ngInfo->empty_startup_mode() == NGInfo::ATTACHED)
    {
        // iterate through empty lru list
        // re-counting the total empty lru statstics

        // empty lru header
        int count = 0;
        Node header = _ngInfo->empty_node_head();
        Node pos;

        for (pos = header.Prev(); pos != header; pos = pos.Prev())
        {
            /* check whether cross-linked */
            if (check_cross_linked_lru(pos) < 0)
                break;
            count++;
        }
        _ngInfo->inc_empty_node(count);
        log_info("found %u empty nodes inside empty lru list", count);
    }
    return 0;
}

// migrate empty node from clean list to empty list
int DTCBufferPool::upgrade_empty_node_list(void)
{
    if (_ngInfo->empty_startup_mode() != NGInfo::CREATED)
    {
        int count = 0;
        Node header = _ngInfo->clean_node_head();
        Node next;

        for (Node pos = header.Prev(); pos != header; pos = next)
        {
            /* check whether cross-linked */
            if (check_cross_linked_lru(pos) < 0)
                break;
            next = pos.Prev();

            if (node_rows_count(pos) == 0)
            {
                _ngInfo->remove_from_lru(pos);
                _ngInfo->insert2_empty_lru(pos);
                count++;
            }
        }
        _ngInfo->inc_empty_node(count);
        log_info("found %u empty nodes inside clean lru list, move to empty lru", count);
    }

    return 0;
}

// migrate empty node from empty list to clean list
int DTCBufferPool::merge_empty_node_list(void)
{
    if (_ngInfo->empty_startup_mode() != NGInfo::CREATED)
    {
        int count = 0;
        Node header = _ngInfo->empty_node_head();
        Node next;

        for (Node pos = header.Prev(); pos != header; pos = next)
        {
            /* check whether cross-linked */
            if (check_cross_linked_lru(pos) < 0)
                break;
            next = pos.Prev();

            _ngInfo->remove_from_lru(pos);
            _ngInfo->insert2_clean_lru(pos);
            count++;
        }
        log_info("found %u empty nodes, move to clean lru", count);
    }

    return 0;
}

// prune all empty nodes
int DTCBufferPool::prune_empty_node_list(void)
{
    if (_ngInfo->empty_startup_mode() == NGInfo::ATTACHED)
    {
        int count = 0;
        Node header = _ngInfo->empty_node_head();
        Node next;

        for (Node pos = header.Prev(); pos != header; pos = next)
        {
            /* check whether cross-linked */
            if (check_cross_linked_lru(pos) < 0)
                break;
            next = pos.Prev();

            count++;
            purge_node_everything(pos);
        }

        log_info("fullmode: total %u empty nodes purged", count);
    }

    return 0;
}

int DTCBufferPool::shrink_empty_node_list(void)
{
    if (emptyLimit && _ngInfo->empty_count() > emptyLimit)
    {
        //bug fix recalc empty
        int togo = _ngInfo->empty_count() - emptyLimit;
        int count = 0;
        Node header = _ngInfo->empty_node_head();
        Node next;

        for (Node pos = header.Prev(); count < togo && pos != header; pos = next)
        {
            /* check whether cross-linked */
            if (check_cross_linked_lru(pos) < 0)
                break;

            next = pos.Prev();

            purge_node_everything(pos);
            _ngInfo->inc_empty_node(-1);
            count++;
        }
        log_info("shrink empty lru, %u empty nodes purged", count);
    }

    return 0;
}

int DTCBufferPool::purge_single_empty_node(void)
{
    Node header = _ngInfo->empty_node_head();
    Node pos = header.Prev();

    if (pos != header)
    {
        /* check whether cross-linked */
        if (check_cross_linked_lru(pos) < 0)
            return -1;

        log_debug("empty node execeed limit, purge node %u", pos.node_id());
        purge_node_everything(pos);
        _ngInfo->inc_empty_node(-1);
    }

    return 0;
}

/* insert node to hash bucket*/
int DTCBufferPool::Insert2Hash(const char *key, Node node)
{
    HASH_ID_T hashslot;

    if (targetNewHash)
    {
        hashslot = _hash->new_hash_slot(key);
    }
    else
    {
        hashslot = _hash->hash_slot(key);
    }

    if (_hash->hash2_node(hashslot) == INVALID_NODE_ID)
    {
        _hash->inc_free_bucket(-1);
        --statFreeBucket;
    }

    _hash->inc_node_cnt(1);

    node.next_node_id() = _hash->hash2_node(hashslot);
    _hash->hash2_node(hashslot) = node.node_id();

    return 0;
}

int DTCBufferPool::remove_from_hash_base(const char *key, Node remove_node, int newhash)
{
    HASH_ID_T hash_slot;

    if (newhash)
    {
        hash_slot = _hash->new_hash_slot(key);
    }
    else
    {
        hash_slot = _hash->hash_slot(key);
    }

    NODE_ID_T node_id = _hash->hash2_node(hash_slot);

    /* hash miss */
    if (node_id == INVALID_NODE_ID)
        return 0;

    /* found in hash head */
    if (node_id == remove_node.node_id())
    {
        _hash->hash2_node(hash_slot) = remove_node.next_node_id();

        // stat
        if (_hash->hash2_node(hash_slot) == INVALID_NODE_ID)
        {
            _hash->inc_free_bucket(1);
            ++statFreeBucket;
        }

        _hash->inc_node_cnt(-1);
        return 0;
    }

    Node prev = I_SEARCH(node_id);
    Node next = I_SEARCH(prev.next_node_id());

    while (!(!next) && next.node_id() != remove_node.node_id())
    {
        prev = next;
        next = I_SEARCH(next.next_node_id());
    }

    /* found */
    if (!(!next))
    {
        prev.next_node_id() = next.next_node_id();
        _hash->inc_node_cnt(-1);
    }
    else
    {
        log_error("remove_from_hash failed, node-id [%d] not found in slot %u ",
                  remove_node.node_id(), hash_slot);
        return -1;
    }

    return 0;
}

int DTCBufferPool::remove_from_hash(const char *key, Node remove_node)
{
    if (hashChanging)
    {
        remove_from_hash_base(key, remove_node, 1);
        remove_from_hash_base(key, remove_node, 0);
    }
    else
    {
        if (targetNewHash)
            remove_from_hash_base(key, remove_node, 1);
        else
            remove_from_hash_base(key, remove_node, 0);
    }

    return 0;
}

int DTCBufferPool::move_to_new_hash(const char *key, Node node)
{
    remove_from_hash(key, node);
    Insert2Hash(key, node);
    return 0;
}

inline int DTCBufferPool::key_cmp(const char *key, const char *other)
{
    int len = _cacheInfo.keySize == 0 ? (*(unsigned char *)key + 1) : _cacheInfo.keySize;

    return memcmp(key, other, len);
}

Node DTCBufferPool::cache_find_auto_chose_hash(const char *key)
{
    int oldhash = 0;
    int newhash = 1;
    Node stNode;

    if (hashChanging)
    {
        if (targetNewHash)
        {
            oldhash = 0;
            newhash = 1;
        }
        else
        {
            oldhash = 1;
            newhash = 0;
        }

        stNode = cache_find(key, oldhash);
        if (!stNode)
        {
            stNode = cache_find(key, newhash);
        }
        else
        {
            move_to_new_hash(key, stNode);
        }
    }
    else
    {
        if (targetNewHash)
        {
            stNode = cache_find(key, 1);
        }
        else
        {
            stNode = cache_find(key, 0);
        }
    }
    return stNode;
}

Node DTCBufferPool::cache_find(const char *key, int newhash)
{
    HASH_ID_T hash_slot;

    if (newhash)
    {
        hash_slot = _hash->new_hash_slot(key);
    }
    else
    {
        hash_slot = _hash->hash_slot(key);
    }

    NODE_ID_T node_id = _hash->hash2_node(hash_slot);

    /* not found */
    if (node_id == INVALID_NODE_ID)
        return Node();

    Node iter = I_SEARCH(node_id);
    while (!(!iter))
    {
        if (iter.vd_handle() == INVALID_HANDLE)
        {
            log_warning("node[%u]'s handle is invalid", iter.node_id());
            Node node = iter;
            iter = I_SEARCH(iter.next_node_id());
            purge_node(key, node);
            continue;
        }

        DataChunk *data_chunk = M_POINTER(DataChunk, iter.vd_handle());
        if (NULL == data_chunk)
        {
            log_warning("node[%u]'s handle is invalid", iter.node_id());
            Node node = iter;
            iter = I_SEARCH(iter.next_node_id());
            purge_node(key, node);
            continue;
        }

        if (NULL == data_chunk->Key())
        {
            log_warning("node[%u]'s handle is invalid, decode key failed", iter.node_id());
            Node node = iter;
            iter = I_SEARCH(iter.next_node_id());
            purge_node(key, node);
            continue;
        }

        /* EQ */
        if (key_cmp(key, data_chunk->Key()) == 0)
        {
            log_debug("found node[%u]", iter.node_id());
            return iter;
        }

        iter = I_SEARCH(iter.next_node_id());
    }

    /* not found*/
    return Node();
}

unsigned int DTCBufferPool::first_time_marker_time(void)
{
    if (firstMarkerTime == 0)
    {
        Node marker = first_time_marker();
        firstMarkerTime = !marker ? 0 : marker.Time();
    }
    return firstMarkerTime;
}

unsigned int DTCBufferPool::last_time_marker_time(void)
{
    if (lastMarkerTime == 0)
    {
        Node marker = last_time_marker();
        lastMarkerTime = !marker ? 0 : marker.Time();
    }
    return lastMarkerTime;
}

/* insert a time-marker to dirty lru list*/
int DTCBufferPool::insert_time_marker(unsigned int t)
{
    Node tm_node = _ngInfo->allocate_node();
    if (!tm_node)
    {
        log_debug("no mem allocate timemarker, purge 10 clean node");
        /* prepurge clean node for cache is full */
        pre_purge_nodes(10, Node());
        tm_node = _ngInfo->allocate_node();
        if (!tm_node)
        {
            log_crit("can not allocate time marker for dirty lru");
            return -1;
        }
    }

    log_debug("insert time marker in dirty lru, time %u", t);
    tm_node.next_node_id() = TIME_MARKER_NEXT_NODE_ID;
    tm_node.vd_handle() = t;

    _ngInfo->insert2_dirty_lru(tm_node);

    //stat
    firstMarkerTime = t;

    /*in case lastMarkerTime not set*/
    if (lastMarkerTime == 0)
        last_time_marker_time();

    statDirtyAge = firstMarkerTime - lastMarkerTime;
    statDirtyEldest = lastMarkerTime;

    return 0;
}

/* -1: not a time marker
 * -2: this the only time marker

 */
int DTCBufferPool::remove_time_marker(Node node)
{
    /* is not timermarker node */
    if (!is_time_marker(node))
        return -1;

    _ngInfo->remove_from_lru(node);
    _ngInfo->release_node(node);

    //stat
    Node stLastTime = last_time_marker();
    if (!stLastTime)
    {
        lastMarkerTime = firstMarkerTime;
    }
    else
    {
        lastMarkerTime = stLastTime.Time();
    }

    statDirtyAge = firstMarkerTime - lastMarkerTime;
    statDirtyEldest = lastMarkerTime;
    return 0;
}

/* prev <- dirtyhead */
Node DTCBufferPool::last_time_marker() const
{
    Node pos, dirtyHeader = _ngInfo->dirty_node_head();
    NODE_LIST_FOR_EACH_RVS(pos, dirtyHeader)
    {
        if (pos.next_node_id() == TIME_MARKER_NEXT_NODE_ID)
            return pos;
    }

    return Node();
}

/* dirtyhead -> next */
Node DTCBufferPool::first_time_marker() const
{
    Node pos, dirtyHeader = _ngInfo->dirty_node_head();

    NODE_LIST_FOR_EACH(pos, dirtyHeader)
    {
        if (pos.next_node_id() == TIME_MARKER_NEXT_NODE_ID)
            return pos;
    }

    return Node();
}

/* dirty lru list head */
Node DTCBufferPool::dirty_lru_head() const
{
    return _ngInfo->dirty_node_head();
}

/* clean lru list head */
Node DTCBufferPool::clean_lru_head() const
{
    return _ngInfo->clean_node_head();
}

/* empty lru list head */
Node DTCBufferPool::empty_lru_head() const
{
    return _ngInfo->empty_node_head();
}

int DTCBufferPool::is_time_marker(Node node) const
{
    return node.next_node_id() == TIME_MARKER_NEXT_NODE_ID;
}

int DTCBufferPool::try_purge_size(size_t size, Node reserve, unsigned max_purge_count)
{
    log_debug("start try_purge_size");

    if (disableTryPurge)
    {
        static int alert_count = 0;
        if (!alert_count++)
        {
            log_alert("memory overflow, auto purge disabled");
        }
        return -1;
    }
    /*if have pre purge, purge node and continue*/
    /* prepurge should not purge reserved node in try_purge_size */
    pre_purge_nodes(DTCGlobal::_pre_purge_nodes, reserve);

    unsigned real_try_purge_count = 0;

    /* clean lru header */
    Node clean_header = clean_lru_head();

    Node pos = clean_header.Prev();

    for (unsigned iter = 0; iter < max_purge_count && !(!pos) && pos != clean_header; ++iter)
    {
        Node purge_node = pos;

        if (total_used_node() < 10)
            break;

        /* check whether cross-linked */
        if (check_cross_linked_lru(pos) < 0)
            break;

        pos = pos.Prev();

        if (purge_node == reserve)
        {
            continue;
        }

        if (purge_node.vd_handle() == INVALID_HANDLE)
        {
            log_warning("node[%u]'s handle is invalid", purge_node.node_id());
            continue;
        }

        /* ask for data-chunk's size */
        DataChunk *data_chunk = M_POINTER(DataChunk, purge_node.vd_handle());
        if (NULL == data_chunk)
        {
            log_warning("node[%u] handle is invalid, attach DataChunk failed", purge_node.node_id());
            continue;
        }

        unsigned combine_size = data_chunk->ask_for_destroy_size(DTCBinMalloc::Instance());
        log_debug("need_size=%u, combine-size=%u, node-size=%u",
                  (unsigned)size, combine_size, data_chunk->node_size());

        if (combine_size >= size)
        {
            /* stat total rows */
            inc_total_row(0LL - node_rows_count(purge_node));
            check_and_purge_node_everything(purge_node);
            _need_purge_node_count = iter;
            log_debug("try purge size for create or update: %d", iter + 1);
            statPurgeForCreateUpdateCount.push(iter + 1);
            ++statTryPurgeNodes;
            return 0;
        }

        ++real_try_purge_count;
    }

    _need_purge_node_count = real_try_purge_count;
    return -1;
}

int DTCBufferPool::purge_node(const char *key, Node purge_node)
{
    /* HB */
    if (_purge_notifier)
        _purge_notifier->purge_node_notify(key, purge_node);

    /*1. Remove from hash */
    remove_from_hash(key, purge_node);

    /*2. Remove from LRU */
    _ngInfo->remove_from_lru(purge_node);

    /*3. Release node, it can auto remove from nodeIndex */
    _ngInfo->release_node(purge_node);

    return 0;
}

int DTCBufferPool::purge_node_everything(Node node)
{
    /* invalid node attribute */
    if (!(!node) && node.vd_handle() != INVALID_HANDLE)
    {
        DataChunk *data_chunk = M_POINTER(DataChunk, node.vd_handle());
        if (NULL == data_chunk || NULL == data_chunk->Key())
        {
            log_error("node[%u]'s handle is invalid, can't attach and decode key", node.node_id());
            //TODO
            return -1;
        }
        uint32_t dwCreatetime = data_chunk->create_time();
        uint32_t dwPurgeHour = RELATIVE_HOUR_CALCULATOR->get_relative_hour();
        log_debug("lru purge node,node[%u]'s createhour is %u, purgeHour is %u", node.node_id(), dwCreatetime, dwPurgeHour);
        survival_hour.push((dwPurgeHour - dwCreatetime));

        char key[256] = {0};
        /* decode key */
        memcpy(key, data_chunk->Key(), _cacheInfo.keySize > 0 ? _cacheInfo.keySize : *(unsigned char *)(data_chunk->Key()) + 1);

        /* destroy data-chunk */
        data_chunk->Destroy(DTCBinMalloc::Instance());

        return purge_node(key, node);
    }

    return 0;
}

uint32_t DTCBufferPool::get_cmodtime(Node *node)
{
    // how init
    RawData *_raw_data = new RawData(DTCBinMalloc::Instance(), 1);
    uint32_t lastcmod = 0;
    uint32_t lastcmod_thisrow = 0;
    int iRet = _raw_data->Attach(node->vd_handle());
    if (iRet != 0)
    {
        log_error("raw-data attach[handle:" UINT64FMT "] error: %d,%s",
                  node->vd_handle(), iRet, _raw_data->get_err_msg());
        return (0);
    }

    unsigned int uiTotalRows = _raw_data->total_rows();
    for (unsigned int i = 0; i < uiTotalRows; i++) //查找
    {
        if ((iRet = _raw_data->get_lastcmod(lastcmod_thisrow)) != 0)
        {
            log_error("raw-data decode row error: %d,%s", iRet, _raw_data->get_err_msg());
            return (0);
        }
        if (lastcmod_thisrow > lastcmod)
            lastcmod = lastcmod_thisrow;
    }
    return lastcmod;
}

//check if node's timestamp max than setting
//and purge_node_everything
int DTCBufferPool::check_and_purge_node_everything(Node node)
{
    int dataExistTime = statDataExistTime;
    if (dateExpireAlertTime)
    {
        struct timeval tm;
        gettimeofday(&tm, NULL);
        unsigned int lastnodecmodtime = get_cmodtime(&node);
        if (lastnodecmodtime > statLastPurgeNodeModTime)
        {
            statLastPurgeNodeModTime = lastnodecmodtime;
            dataExistTime = (unsigned int)tm.tv_sec - statLastPurgeNodeModTime;
            statDataExistTime = dataExistTime;
        }
        if (statDataExistTime < dateExpireAlertTime)
        {
            static int alert_count = 0;
            if (!alert_count++)
            {
                log_alert("DataExistTime:%u is little than setting:%u", dataExistTime, dateExpireAlertTime);
            }
        }
        log_debug("dateExpireAlertTime:%d ,lastnodecmodtime:%d,timenow:%u", dateExpireAlertTime, lastnodecmodtime, (uint32_t)tm.tv_sec);
    }

    return purge_node_everything(node);
}
int DTCBufferPool::purge_node_everything(const char *key, Node node)
{
    DataChunk *data_chunk = NULL;
    if (!(!node) && node.vd_handle() != INVALID_HANDLE)
    {
        data_chunk = M_POINTER(DataChunk, node.vd_handle());
        if (NULL == data_chunk)
        {
            log_error("node[%u]'s handle is invalid, can't attach data-chunk", node.node_id());
            return -1;
        }
        uint32_t dwCreatetime = data_chunk->create_time();
        uint32_t dwPurgeHour = RELATIVE_HOUR_CALCULATOR->get_relative_hour();
        log_debug(" purge node, node[%u]'s createhour is %u, purgeHour is %u", node.node_id(), dwCreatetime, dwPurgeHour);
        survival_hour.push((dwPurgeHour - dwCreatetime));
        /* destroy data-chunk */
        data_chunk->Destroy(DTCBinMalloc::Instance());
    }

    if (!(!node))
        return purge_node(key, node);
    return 0;
}

/* allocate a new node by key */
Node DTCBufferPool::cache_allocate(const char *key)
{
    Node allocate_node = _ngInfo->allocate_node();

    /* allocate failed */
    if (!allocate_node)
        return allocate_node;

    /*1. Insert to hash bucket */
    Insert2Hash(key, allocate_node);

    /*2. Insert to clean Lru list*/
    _ngInfo->insert2_clean_lru(allocate_node);

    return allocate_node;
}

extern int useNewHash;

/* purge key{data-chunk, hash, lru, node...} */
int DTCBufferPool::cache_purge(const char *key)
{
    Node purge_node;

    if (hashChanging)
    {
        purge_node = cache_find(key, 0);
        if (!purge_node)
        {
            purge_node = cache_find(key, 1);
            if (!purge_node)
            {
                return 0;
            }
            else
            {
                if (purge_node_everything(key, purge_node) < 0)
                    return -1;
            }
        }
        else
        {
            if (purge_node_everything(key, purge_node) < 0)
                return -1;
        }
    }
    else
    {
        if (targetNewHash)
        {
            purge_node = cache_find(key, 1);
            if (!purge_node)
                return 0;
            else
            {
                if (purge_node_everything(key, purge_node) < 0)
                    return -1;
            }
        }
        else
        {
            purge_node = cache_find(key, 0);
            if (!purge_node)
                return 0;
            else
            {
                if (purge_node_everything(key, purge_node) < 0)
                    return -1;
            }
        }
    }

    return 0;
}

void DTCBufferPool::delay_purge_notify(const unsigned count)
{
    if (_need_purge_node_count == 0)
        return;
    else
        statTryPurgeCount.push(_need_purge_node_count);

    unsigned purge_count = count < _need_purge_node_count ? count : _need_purge_node_count;
    unsigned real_purge_count = 0;

    log_debug("delay_purge_notify: total=%u, now=%u", _need_purge_node_count, purge_count);

    /* clean lru header */
    Node clean_header = clean_lru_head();
    Node pos = clean_header.Prev();

    while (purge_count-- > 0 && !(!pos) && pos != clean_header)
    {
        Node purge_node = pos;
        check_cross_linked_lru(pos);
        pos = pos.Prev();

        /* stat total rows */
        inc_total_row(0LL - node_rows_count(purge_node));

        check_and_purge_node_everything(purge_node);

        ++statTryPurgeNodes;
        ++real_purge_count;
    }

    _need_purge_node_count -= real_purge_count;

    /* 如果没有请求，重新调度delay purge任务 */
    if (_need_purge_node_count > 0)
        attach_timer(_delay_purge_timerlist);

    return;
}

int DTCBufferPool::pre_purge_nodes(int purge_count, Node reserve)
{
    int realpurged = 0;

    if (purge_count <= 0)
        return 0;
    else
        statTryPurgeCount.push(purge_count);

    /* clean lru header */
    Node clean_header = clean_lru_head();
    Node pos = clean_header.Prev();

    while (purge_count-- > 0 && !(!pos) && pos != clean_header)
    {
        Node purge_node = pos;
        check_cross_linked_lru(pos);
        pos = pos.Prev();

        if (reserve == purge_node)
            continue;

        /* stat total rows */
        inc_total_row(0LL - node_rows_count(purge_node));
        check_and_purge_node_everything(purge_node);
        ++statTryPurgeNodes;
        realpurged++;
    }
    return realpurged;
    ;
}

int DTCBufferPool::purge_by_time(unsigned int oldesttime)
{
    return 0;
}

int DTCBufferPool::clear_create()
{
    if (_cacheInfo.readOnly == 1)
    {
        snprintf(_errmsg, sizeof(_errmsg), "cache readonly, can not clear cache");
        return -2;
    }
    _hash->Destroy();
    _ngInfo->Destroy();
    _feature->Destroy();
    _nodeIndex->Destroy();
    DTCBinMalloc::Instance()->Destroy();
    if (_shm.Delete() < 0)
    {
        snprintf(_errmsg, sizeof(_errmsg), "delete shm memory error");
        return -1;
    }
    log_notice("delete shm memory ok when clear cache");

    if (_shm.Create(_cacheInfo.ipcMemKey, _cacheInfo.ipcMemSize) <= 0)
    {
        snprintf(_errmsg, sizeof(_errmsg), "create shm memory error");
        return -1;
    }
    if (_shm.Attach() == NULL)
    {
        snprintf(_errmsg, sizeof(_errmsg), "attach shm memory error");
        return -1;
    }

    if (DTCBinMalloc::Instance()->Init(_shm.Ptr(), _shm.Size()) != 0)
    {
        snprintf(_errmsg, sizeof(_errmsg),
                 "binmalloc init failed: %s", M_ERROR());
        return -1;
    }
    DTCBinMalloc::Instance()->set_min_chunk_size(DTCGlobal::_min_chunk_size);
    return app_storage_open();
}

void DTCBufferPool::start_delay_purge_task(TimerList *timer)
{
    log_info("start delay-purge task");
    _delay_purge_timerlist = timer;
    attach_timer(_delay_purge_timerlist);

    return;
}
void DTCBufferPool::timer_notify(void)
{
    log_debug("sched delay-purge task");

    delay_purge_notify();
}
