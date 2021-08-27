
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_conn.cc 
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
#include "rocksdb_conn.h"
#include "adaptive_prefix_extractor.h"
#include "rocksdb_key_comparator.h"
#include "rocksdb_block_filter.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/trace_reader_writer.h"
#include <log.h>

#include <string>
#include <sstream>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#define USE_PREFIX_EXTRATOR
// #define USE_ROCKS_TRACER
// #define USE_STATISTIC_COUNTER

RocksDBConn::RocksDBConn()
    : mRocksEnv(NULL),
      mRocksDb(NULL),
      mHasConnected(false),
      mDtckey_type(-1),
      mFixedKeyLen(0)
{
}

int RocksDBConn::get_errno()
{
  if (dberr >= 2000)
    return -(dberr - 100);
  return -dberr;
}

int RocksDBConn::get_raw_errno()
{
  return dberr;
}

const char *RocksDBConn::get_err_msg()
{
  return achErr;
}

int RocksDBConn::Open(const std::string &dbPath)
{
  if (mHasConnected)
  {
    log_info("rocksdb has been opened yet! path:%s", mRocksDbPath.c_str());
    return 0;
  }

  mRocksDbPath = dbPath;

  Options options;

  // not check the underlying sstfile during DB open
  //options.skip_checking_sst_files_sizes_on_db_open = true;
  options.skip_stats_update_on_db_open = true;
  options.level_compaction_dynamic_level_bytes = true;

  init_db_options(options);

  // create rocksdb logger
  // if the log dir not exist, create it
  int ret = create_rocks_dir(options.db_log_dir);
  if (ret != 0)
    return -1;

  Status s = CreateLoggerFromOptions(mRocksDbPath, options, &mRocksdbLogger);
  mRocksdbLogger->SetInfoLogLevel(InfoLogLevel::DEBUG_LEVEL); // application may want
                                                              // the diffrent log level with rocksdb, set it manually
  if (!s.ok() || !mRocksdbLogger)
  {
    log_error("create rocksdb logger failed, code:%d, subcode:%d, errmsg:%s",
              s.code(), s.subcode(), s.getState());
    return -1;
  }
  options.info_log = mRocksdbLogger;

  ret = create_tracer();
  if (ret != 0)
    return -1;

  // set memtable(include immutable memtable) related for flushing
  init_flushing_options(options);

  // kernal environment
  init_environment_options(options);

  // init table options
  init_table_options(options);

  // due to its single-threaded compaction
  init_compaction_options(options);

  // deprecated, use default case sensitive compare
  ColumnFamilyOptions colOptions;
  init_column_family_options(colOptions);

  // enfoce Rocksdb work on prefix search mode, create user defined prefix extrator
  // for prefix search
  // open db should have 'prefix_extractor' been set
#ifdef USE_PREFIX_EXTRATOR
  {
    if (!mPrefixExtractor)
      mPrefixExtractor.reset(new AdaptivePrefixExtractor(mDtckey_type));
    options.prefix_extractor = mPrefixExtractor;
  }
#endif
  init_prefix_extractor(colOptions);

  // column family compaction behavior
  options.max_subcompactions = 3; // concurrently compacte in Level0 -> level1,

  // create with two column families
  // get column family from storage if they exist
  std::vector<std::string> columnFamiliesNames;
  s = DB::ListColumnFamilies(options, mRocksDbPath, &columnFamiliesNames);
  if (!s.ok() && !s.IsPathNotFound())
    goto REPORT_ERROR;

  if (s.IsPathNotFound() || columnFamiliesNames.size() == 0)
  {
    // no column families in the storage, need to create it
    // enforce open to create the 'default' column family with customer comparator
    // options.comparator = mCommKeyComparator;
    s = DB::Open(options, mRocksDbPath, &mRocksDb);
    if (!s.ok())
      goto REPORT_ERROR;

    columnFamiliesNames.push_back("COLUMN_REAL_DATA");
    columnFamiliesNames.push_back("COLUMN_META_DATA");
    // in this open style, rocksdb will create the default column, no need to create it
    // in user space explicitly
    // columnFamiliesNames.push_back(kDefaultColumnFamilyName);
    s = mRocksDb->CreateColumnFamilies(colOptions, columnFamiliesNames, &mColumnHandles);
    if (!s.ok())
      goto REPORT_ERROR;
  }
  else
  {
    // check column families
    assert(columnFamiliesNames.size() == COLUMN_FAMILIES_NUM);

    // user need to specifically list all column families those aready lying in the rocksdb storage,
    // include the 'default' one, and the column family name order must be same with the previous
    // user-defined 'ColumnFamiliesIndex' enum
    std::vector<ColumnFamilyDescriptor> columnFamilies;
    columnFamilies.push_back(ColumnFamilyDescriptor("COLUMN_REAL_DATA", colOptions));
    columnFamilies.push_back(ColumnFamilyDescriptor("COLUMN_META_DATA", colOptions));
    columnFamilies.push_back(ColumnFamilyDescriptor(kDefaultColumnFamilyName, colOptions));

    // open db with all existed column families
    s = DB::Open(options, mRocksDbPath, columnFamilies, &mColumnHandles, &mRocksDb);
    if (!s.ok())
      goto REPORT_ERROR;
  }

  mHasConnected = true;
  log_error("open rocksdb success! path:%s", mRocksDbPath.c_str()); 
  return 0;

REPORT_ERROR:
  log_error("open rocksdb failed! path:%s, code:%d, subcode:%d, errmsg:%s",
            mRocksDbPath.c_str(), s.code(), s.subcode(), s.getState());
  return -1;
}

// sync memory value into disk
int RocksDBConn::Close()
{
  if (!mHasConnected)
  {
    log_info("rocksdb has been closed yet! path:%s", mRocksDbPath.c_str());
    return 0;
  }

  Status s = mRocksDb->SyncWAL();
  if (!s.ok())
  {
    // there has unreleased snapshot in the system, user should release the unreleased snapshots and try again
    log_error("close rocksdb failed! code:%d, subCode:%d, errMsg:%s", s.code(), s.subcode(), s.getState());
    return -1;
  }

  // drop column families
  // s = mRocksDb->DropColumnFamily(mColumnHandles[1]);
  // assert( s.ok() );
  for (auto handler : mColumnHandles)
  {
    // close the column families
    s = mRocksDb->DestroyColumnFamilyHandle(handler);
    assert(s.ok());
  }
  mColumnHandles.clear();

  s = mRocksDb->Close();
  assert(s.ok());

  mRocksDb = NULL;
  mHasConnected = false;

  // flush log
  if (mRocksdbLogger)
    mRocksdbLogger->Flush();

  return 0;
}

int RocksDBConn::get_entry(
    const std::string &key,
    std::string &value,
    ColumnFamiliesIndex colIndex)
{
  assert(mRocksDb);

  ReadOptions options = ReadOptions();
  // force entire key matched even in prefix search mode
  // ig. if column family is set to be work on prefix_mode(set prefix_extractor), when
  // customer call get("foo"), it will return the key that prefix matched which satisfy
  // the prefix_extractor rule
  options.total_order_seek = true;

  Status s = mRocksDb->Get(options, mColumnHandles[colIndex], key, &value);
  if (s.ok())
    return 0;

  // handle error
  unsigned char code = s.code();
  return code == rocksdb::Status::kNotFound ? -ERROR_KEY_NOT_FOUND : -code;
}

/* if there has partial concept, the same key must be in the same partion, otherwise, 
 * 'insert_entry' operation will be no longer atomic in case of the Rocks support mutil thread*/
int RocksDBConn::insert_entry(
    const std::string &key,
    const std::string &value,
    bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  int ret = key_exist(key, colIndex);
  if (ret == 1)
  {
    // value has been exist in the db
    log_info("value already exist in the db, key:%s", key.c_str());
    return -ERROR_DUPLICATE_KEY;
  }
  else if (ret < 0)
  {
    // encounter error
    return ret;
  }

  // key not found
  ret = replace_entry(key, value, syncMode, colIndex);
  if (ret != 0)
  {
    log_error("insert key failed, key:%s, value:%s", key.c_str(), value.c_str());
  }

  return ret;
}

int RocksDBConn::update_entry(
    const std::string &key,
    const std::string &value,
    bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  int ret = key_exist(key, colIndex);
  if (ret == 0)
  {
    // can not update non-exist row
    log_info("value not exist in the db, key:%s", key.c_str());
    return -ERROR_KEY_NOT_FOUND;
  }
  else if (ret < 0)
    return ret;

  return replace_entry(key, value, syncMode, colIndex);
}

int RocksDBConn::replace_entry(
    const std::string &key,
    const std::string &value,
    bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  WriteOptions opt;
  opt.sync = syncMode;

  Status s = mRocksDb->Put(opt, mColumnHandles[colIndex], key, value);
  if (s.ok())
    return 0;

  log_error("update value failed, key:%s, value:%s, errMsg:%s, code:%d, subCode:%d", key.c_str(), value.c_str(), s.getState(), s.code(), s.subcode());
  return -s.code();
}

int RocksDBConn::delete_entry(
    const std::string &key,
    bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  WriteOptions opt;
  opt.sync = syncMode;

  Status s = mRocksDb->Delete(opt, mColumnHandles[colIndex], key);
  if (s.ok())
    return 0;

  log_error("remove key failed, key:%s, errMsg:%s, code:%d, subCode:%d", key.c_str(), s.getState(), s.code(), s.subcode());
  return -s.code();
}

int RocksDBConn::single_delete_entry(
    const std::string &key,
    bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  WriteOptions opt;
  opt.sync = syncMode;

  Status s = mRocksDb->SingleDelete(opt, mColumnHandles[colIndex], key);
  if (s.ok())
    return 0;

  log_error("single remove key failed, key:%s, errMsg:%s, code:%d, subCode:%d", key.c_str(), s.getState(), s.code(), s.subcode());
  return -s.code();
}

int RocksDBConn::batch_update(
    const std::set<std::string> &deletedKeys,
    const std::map<std::string, std::string> &newEntries,
    const bool syncMode,
    ColumnFamiliesIndex colIndex)
{
  WriteOptions opt;
  opt.sync = syncMode;

  WriteBatch batch;
  auto sItr = deletedKeys.begin();
  while (sItr != deletedKeys.end())
  {
    batch.Delete(mColumnHandles[colIndex], *sItr);
    sItr++;
  }

  auto mitr = newEntries.begin();
  while (mitr != newEntries.end())
  {
    batch.Put(mColumnHandles[colIndex], mitr->first, mitr->second);
    mitr++;
  }

  TraceOptions traceOpt;

#ifdef USE_ROCKS_TRACER
  assert(mRocksDb->StartTrace(traceOpt, std::move(mTraceWriter)).ok());
#endif

  Status s = mRocksDb->Write(opt, &batch);
  if (s.ok())
    return 0;

#ifdef USE_ROCKS_TRACER
  assert(mRocksDb->EndTrace().ok());
#endif

  log_error("do atomic write failed!");
  return -s.code();
}

// start a transaction, caller should hold the iterator and call 'next_entry' with it the
// next time
// return value:
// -1 : error occur
// 0 : get value success and not reach to the end
// 1: reach to the end, no more key left
int RocksDBConn::retrieve_start(
    std::string &startKey,
    std::string &value,
    RocksItr_t &itr,
    bool searchMode,
    bool forwardDirection,
    ColumnFamiliesIndex colIndex)
{
  if ( startKey.empty() )
  {
    if (!forwardDirection)
    {
    	log_info("empty retrieve key!");
    	return -1;
    }
    // search from first
    return get_first_entry(startKey, value, itr, colIndex);
  }

  ReadOptions options = ReadOptions();

  // this option is effective only when customer both define 'prefix_extractor' and the
  // 'total_order_seek' is false
  options.prefix_same_as_start = searchMode;

  // if open rocksdb with prefix bloom filter mode, related search operation will be work on
  // prefix bloom filter, and it only guarantee the order for the partial keys those with the
  // same prefix, the total key order will not been guaranteed when iteration the whole keys,
  // if we want to scan the keys with total order, set 'total_order_seek' to true
  if (!searchMode)
    options.total_order_seek = true;
  options.fill_cache = false;

  // start retrieving
  itr = mRocksDb->NewIterator(options, mColumnHandles[colIndex]);
  if (forwardDirection)
  {
    // point at the first key that equal or larger then the 'startKey', search forward
    itr->Seek(startKey);
  }
  else
  {
    // point at the last key that equal or less than the 'startKey', search backward
    itr->SeekForPrev(startKey);
  }

  if (!itr->Valid())
  {
    if (itr->status().ok())
    {
      // reach to the end, no more key left
      log_info("retrieing reach to the end!");
      return 1;
    }

    // internal error
    log_error("retrieving internal error, not found the given key, it maybe deleted\
        during we held the snapshot! key:%s, errMsg:%s",
              startKey.c_str(), itr->status().getState());
    return -1;
  }

  // bring back the key
  startKey = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

// release the snapshot that the iterator held
int RocksDBConn::retrieve_end(RocksItr_t itr)
{
  if (itr)
    delete itr;
  // mIterator = NULL;

  return 0;
}

// return value:
// -1 : error occur
// 0 : get value success and not reach to the end
// 1: reach to the end, no more key left
int RocksDBConn::prev_entry(
    RocksItr_t itr,
    std::string &key,
    std::string &value)
{
  assert(itr);

  itr->Prev();
  if (!itr->Valid())
  {
    if (itr->status().ok())
    {
      // reach to the end, no more key left
      log_info("retrieing reach to the end!");
      return 1;
    }

    // internal error
    log_error("retrieving internal error, not found the given key, it maybe deleted\
        during we held the snapshot! errMsg:%s",
              itr->status().getState());
    return -1;
  }

  key = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

// return value:
// -1 : error occur
// 0 : get value success and not reach to the end
// 1: reach to the end, no more key left
int RocksDBConn::next_entry(
    RocksItr_t itr,
    std::string &key,
    std::string &value)
{
  assert(itr);

  itr->Next();
  if (!itr->Valid())
  {
    if (itr->status().ok())
    {
      // reach to the end, no more key left
      log_info("retrieing reach to the end!");
      return 1;
    }

    // internal error
    log_error("retrieving internal error, not found the given key, it maybe deleted during we held the snapshot! errMsg:%s", itr->status().getState());
    return -1;
  }

  key = itr->key().ToString();

  /*
  // check whether read to the end(prefix search finished)
  size_t len = prefixKey.length();
  if ( len > key.length() || key.compare(0, len, prefixKey) != 0 )
  {
    log_info("retrieve over!");
    return 1;
  }
  */
  value = itr->value().ToString();

  return 0;
}

// searching not in prefix mode
int RocksDBConn::search_lower_bound(
    std::string &targetKey,
    std::string &value,
    RocksItr_t &itr,
    ColumnFamiliesIndex colIndex)
{
  if (targetKey.empty())
  {
    log_error("not permited to search empty key!");
    return -1;
  }

  ReadOptions options = ReadOptions();
  options.fill_cache = false;
  options.total_order_seek = true;

  // start retrieving
  itr = mRocksDb->NewIterator(options, mColumnHandles[colIndex]);
  {
    // iterator point at the first element which >= 'targetKey'
    itr->Seek(targetKey);
  }

  if (!itr->Valid())
  {
    if (itr->status().ok())
    {
      // reach to the end, no more key left
      log_info("retrieing reach to the end! key:%s", targetKey.c_str());
      return 1;
    }

    // internal error
    log_error("retrieving internal error, not found the given key, it maybe deleted during we held the snapshot! key:%s, errMsg:%s", targetKey.c_str(), itr->status().getState());
    return -1;
  }

  targetKey = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

int RocksDBConn::search_upper_bound(
    std::string &targetKey,
    std::string &value,
    RocksItr_t &itr,
    ColumnFamiliesIndex colIndex)
{
  if (targetKey.empty())
  {
    log_error("not permited to search empty key!");
    return -1;
  }

  ReadOptions options = ReadOptions();
  options.fill_cache = false;
  options.total_order_seek = true;

  // start retrieving
  itr = mRocksDb->NewIterator(options, mColumnHandles[colIndex]);
  {
    // iterator point at the last element that <= 'targetKey'
    itr->SeekForPrev(targetKey);
  }

  if (!itr->Valid())
  {
    if (itr->status().ok())
    {
      // reach to the end, no more key left
      log_info("retrieing reach to the end! key:%s", targetKey.c_str());
      return 1;
    }

    // internal error
    log_error("retrieving internal error, not found the given key, it maybe deleted during we held the snapshot! key:%s, errMsg:%s", targetKey.c_str(), itr->status().getState());
    return -1;
  }

  targetKey = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

int RocksDBConn::get_first_entry(
    std::string &key,
    std::string &value,
    RocksItr_t &itr,
    ColumnFamiliesIndex colIndex)
{
  ReadOptions options = ReadOptions();
  options.fill_cache = false;
  options.readahead_size = 1;
  options.total_order_seek = true;

  // start retrieving
  itr = mRocksDb->NewIterator(options, mColumnHandles[colIndex]);
  {
    // search the firsr key
    itr->SeekToFirst();
  }

  if (!itr->Valid())
  {
    // empty database
    log_info("empty database! errMsg:%s", itr->status().getState());
    return 1;
  }

  key = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

int RocksDBConn::get_last_entry(
    std::string &key,
    std::string &value,
    RocksItr_t &itr,
    ColumnFamiliesIndex colIndex)
{
  ReadOptions options = ReadOptions();
  options.fill_cache = false;
  options.readahead_size = 1; // disable readahead
  options.total_order_seek = true;

  // start retrieving
  itr = mRocksDb->NewIterator(options, mColumnHandles[colIndex]);
  {
    // search the firsr key
    itr->SeekToLast();
  }

  if (!itr->Valid())
  {
    // empty database
    log_info("empty database! errMsg:%s", itr->status().getState());
    return 1;
  }

  key = itr->key().ToString();
  value = itr->value().ToString();

  return 0;
}

uint32_t RocksDBConn::escape_string(char To[], const char *From)
{

  // not support now
  return -1;
}

RocksDBConn::~RocksDBConn()
{
  Close();
}

int RocksDBConn::key_exist(
    const std::string &key,
    ColumnFamiliesIndex colIndex)
{
  // bool hasValue = false;

  ReadOptions options = ReadOptions();
  options.fill_cache = false;

  std::string originValue;
  // bool rslt = mRocksDb->KeyMayExist(options, mColumnHandles[colIndex], key, &originValue, &hasValue);
  bool rslt = mRocksDb->KeyMayExist(options, mColumnHandles[colIndex], key, &originValue);
  if (!rslt)
  {
    // key definitely does not exist in the database
    return 0;
  }

  // use default bloom filter to figure out the key exist or not but still have
  // 1% false positive rate, double check
  // if ( !hasValue ) return false;
  int ret = get_entry(key, originValue, colIndex);
  if (ret == -ERROR_KEY_NOT_FOUND)
    return 0;
  if (ret == 0)
  {
    // value has been exist in the db
    log_error("value already exist in the db, ret:%d, key:%s, value:%s", ret, key.c_str(), originValue.c_str());
    return 1;
  }

  log_error("find key failed, ret:%d, key:%s", ret, key.c_str());
  return ret;
}

int RocksDBConn::create_rocks_dir(const std::string &stuffDir)
{
  int ret = access(stuffDir.c_str(), F_OK);
  if (ret != 0)
  {
    int err = errno;
    if (errno == ENOENT)
    {
      // create log dir
      if (mkdir(stuffDir.c_str(), 0755) != 0)
      {
        log_error("create rocksdb dir failed! path:%s, errno:%d", stuffDir.c_str(), errno);
        return -1;
      }
    }
    else
    {
      log_error("access rocksdb stuff dir failed!, path:%s, errno:%d", stuffDir.c_str(), errno);
      return -1;
    }
  }

  return 0;
}

/* open statistic switch maybe lead 5% ~ 10% performance hit */
void RocksDBConn::init_statistic_info(Options &option)
{
  option.statistics = rocksdb::CreateDBStatistics();

  // time period to print statistic info into log file, set it to 30min
  option.stats_dump_period_sec = (60 * 30);

  return;
}

void RocksDBConn::init_prefix_extractor(ColumnFamilyOptions &option)
{
#ifdef USE_PREFIX_EXTRATOR
  {
    if (!mPrefixExtractor)
      mPrefixExtractor.reset(new AdaptivePrefixExtractor(mDtckey_type));
  }
  option.prefix_extractor = mPrefixExtractor;
#endif

  // enable prefix bloom filter for memtable, bloom filter size is base on write_buffer_size
  option.memtable_prefix_bloom_size_ratio = 0.25;

  // create bloom filter for entire key in memtable
  option.memtable_whole_key_filtering = true;

  // disable the bloom filter in the last level
  option.optimize_filters_for_hits = true;

  return;
}

// storing data in block-based table with hard disk and in Plain table for pure-memory or ssd
void RocksDBConn::init_table_options(Options &option)
{
  BlockBasedTableOptions tableOpt;

  // false: hold all bloom filter in the memory
  // true: evict corrupt bloom filter and index blocks from cache when memory exhausted
  tableOpt.cache_index_and_filter_blocks = true;
  tableOpt.pin_l0_filter_and_index_blocks_in_cache = true;

  // init the datablock index type, friendly for prefix search
  // tableOpt.index_type = BlockBasedTableOptions::kHashSearch;

  // init datablock to hash index feature, even though use this feature is very hard(must make
  // sure that the key's with different byte content not be equal, case sensitive), but it can
  // backward compatible to the binary search
  tableOpt.data_block_index_type = BlockBasedTableOptions::kDataBlockBinaryAndHash;
  tableOpt.data_block_hash_table_util_ratio = 0.75;

  // enable block cache and use evict algorithm
  tableOpt.no_block_cache = false; // disable block cache
  // tableOpt.block_restart_interval = 4;

  tableOpt.read_amp_bytes_per_bit = 0;
  tableOpt.block_size = 64 << 10;

  // also build bloom filter for the entire key
  tableOpt.whole_key_filtering = true;

  // create bloom filter for SST file to reduce read amplification, and use 10 bits to store
  // one key in bloom filter.
  // true: block-based filter, create bloom filter for each seprated block in sst file
  // false: full filter, only create one bloom filter for the whole sst file
  // full filter will be 40% performance improve for consulting
  tableOpt.filter_policy.reset(NewBloomFilterPolicy(10, false));

  option.table_factory.reset(NewBlockBasedTableFactory(tableOpt));

  return;
}

// set option for controlling rocksdb writing and flushing
void RocksDBConn::init_flushing_options(Options &option)
{
  // single memtable size(the same as immutable memtable)
  option.write_buffer_size = 64 << 20;

  // total number of memtable in memory, include memtable and immutable-memtable
  option.max_write_buffer_number = 5;

  //  minimum number of memtables to be merged before flushing them to storage, if assign a
  //  If multiple memtables are merged together, less data may be written to storage since two
  //  updates are merged to a single key. However, every Get() must traverse all immutable
  //  memtables linearly to check if the key is there. Setting this option too high may hurt
  //  read performance.
  option.min_write_buffer_number_to_merge = 2;

  return;
}

// set disk and memory in posix environment
void RocksDBConn::init_environment_options(Options &option)
{
  option.new_table_reader_for_compaction_inputs = true;
}

void RocksDBConn::init_compaction_options(Options &colOptions)
{
  colOptions.num_levels = 7;

  // use finer control of compression style of each level to instead of options.compression
  // configure compression type for each level
  // It usually makes sense to avoid compressing levels 0 and 1 and to compress data only in
  // higher levels. You can even set slower compression in highest level and faster compression
  // in lower levels (by highest we mean Lmax).
  std::vector<CompressionType> levelCompressionStyles;
  levelCompressionStyles.push_back(CompressionType::kNoCompression);     // no compression in level 0
  levelCompressionStyles.push_back(CompressionType::kNoCompression);     // level 1 also not compression
  levelCompressionStyles.push_back(CompressionType::kSnappyCompression); // level 2 use quick snappy compress
  levelCompressionStyles.push_back(CompressionType::kSnappyCompression);
  levelCompressionStyles.push_back(CompressionType::kSnappyCompression);
  levelCompressionStyles.push_back(CompressionType::kSnappyCompression);
  levelCompressionStyles.push_back(CompressionType::kZlibCompression);
  colOptions.compression_per_level = levelCompressionStyles;

  // Usually the bottommost level contains majority of the data, so users get an almost
  // optimal space setting, without paying CPU for compressing all the data at any level.
  // We recommend ZSTD. If it is not available, Zlib is the second choice.
  //colOptions.bottommost_compression = CompressionType::kZSTD;

  // use level style compaction to reduce read amplification and the universal style compaction
  // for reducing write amplification.
  colOptions.compaction_style = rocksdb::CompactionStyle::kCompactionStyleLevel;

  // when number of files reside on level 0 bigger than it, trigger compaction
  colOptions.level0_file_num_compaction_trigger = 2;

  colOptions.level0_slowdown_writes_trigger = 20;
  colOptions.level0_stop_writes_trigger = 36;
  colOptions.write_buffer_size = 64 << 20;

  // file size in level1
  colOptions.target_file_size_base = 32 << 20;
  colOptions.max_bytes_for_level_base = 64 << 20;

  colOptions.max_compaction_bytes = 100 << 20;
  colOptions.hard_pending_compaction_bytes_limit = 512ULL << 30;

  return;
}

void RocksDBConn::init_column_family_options(ColumnFamilyOptions &colOptions)
{
  colOptions.max_write_buffer_number = 5;

  // number should follwed the 'max_write_buffer_number'
  colOptions.min_write_buffer_number_to_merge = 1;

  // due to its single-threaded compaction
  // init_compaction_options(colOptions);

  // use default bytewise comparator
  // mCommKeyComparator = new CaseSensitiveFreeComparator();
  // colOptions.comparator = mCommKeyComparator;
}

void RocksDBConn::init_db_options(Options &options)
{
  // set concurrently operate
  options.IncreaseParallelism(5);

  mRocksEnv = Env::Default();
  // To increase the number of threads in each pool manually
  // mRocksEnv->SetBackgroundThreads(1, Env::BOTTOM);
  // mRocksEnv->SetBackgroundThreads(1, Env::LOW);
  // mRocksEnv->SetBackgroundThreads(1, Env::HIGH); // flush thread is
  // usually good enough to be set to 1 by rocksdb automatically, no need to add
  options.env = mRocksEnv;

  // create the DB if it's not already present
  options.create_if_missing = true;

  options.wal_dir = std::move(std::string(mRocksDbPath).append("/wallog"));
  options.max_total_wal_size = 4 << 30;

  // init rocksdb statistic info for monitoring the performance bottleneck
#ifdef USE_STATISTIC_COUNTER
  init_statistic_info(options);
#endif

  options.compaction_readahead_size = 2 << 20; // 2M

  // 1.in LSM architecture, there are two background process:flush and compaction,
  //   they can execute concurrently, the flush thread are in the HIGH priority thread
  //   pool and the compaction threads are in the LOW priority thread pool

  // max_background_compactions and max_background_flushes is no longer supported now,
  // they automatically decides by 'max_background_jobs'
  options.max_background_jobs = 8;

  options.max_file_opening_threads = 3;
  options.max_manifest_file_size = 20 << 20;

  options.db_log_dir = std::move(std::string(mRocksDbPath).append("/log"));
  options.max_log_file_size = 50 << 20; // 50M per log file size

  options.keep_log_file_num = 100;                    // keep 1GB log in disk
  options.info_log_level = InfoLogLevel::ERROR_LEVEL; // set default log level to ERROR

  options.target_file_size_base = 32 << 20;

  options.max_successive_merges = 1000;

  return;
}

int RocksDBConn::create_tracer()
{
#ifndef USE_ROCKS_TRACER
  return 0;
#endif

  std::string tracerFilePath = std::move(std::string(mRocksDbPath).append("/tracer"));
  int ret = create_rocks_dir(tracerFilePath);
  if (ret != 0)
    return -1;

  EnvOptions envOptions;
  NewFileTraceWriter(mRocksEnv, envOptions, tracerFilePath, &mTraceWriter);
}
