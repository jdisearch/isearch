
/*
 * =====================================================================================
 *
 *       Filename:  db_process_rocks.cc 
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <bitset>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "db_process_rocks.h"
#include <protocol.h>
#include <log.h>

#include "proc_title.h"
#include "table_def_manager.h"

#include "buffer_pool.h"
#include <daemon.h>
#include "mysql_error.h"
#include <sstream>
#include <fstream>

// #define DEBUG_INFO
#define PRINT_STAT
#define BITS_OF_BYTE 8 /* bits of one byte */
#define MAX_REPLICATE_LEN (1UL << 20)
static const std::string gReplicatePrefixKey = "__ROCKS_REPLICAE_PREFIX_KEY__";
CommKeyComparator gInternalComparator;

RocksdbProcess::RocksdbProcess(RocksDBConn *conn)
{
  mDBConn = conn;

  strncpy(name, "helper", 6);
  titlePrefixSize = 0;
  procTimeout = 0;

  mTableDef = NULL;
  mCompoundKeyFieldNums = -1;
  mExtraValueFieldNums = -1;

  mNoBitmapKey = true;

  mPrevMigrateKey = "";
  mCurrentMigrateKey = "";
  mUncommitedMigId = -1;

  mOrderByUnit = NULL;
  mReplUnit = NULL;
}

RocksdbProcess::~RocksdbProcess()
{
}

int RocksdbProcess::check_table()
{
  // no table concept in rocksdb
  return (0);
}

void RocksdbProcess::init_ping_timeout(void)
{
  // only for frame adapt
  return;
}

void RocksdbProcess::use_matched_rows(void)
{
  // only for frame adapt, no actual meanings
  return;
}

int RocksdbProcess::Init(int GroupID, const DbConfig *Config, DTCTableDefinition *tdef, int slave)
{
  int ret;

  SelfGroupID = GroupID;
  mTableDef = tdef;

  std::vector<int> dtcFieldIndex;
  int totalFields = mTableDef->num_fields();
  for (int i = 0; i <= totalFields; i++)
  {
    //bug fix volatile不在db中
    if (mTableDef->is_volatile(i))
      continue;
    dtcFieldIndex.push_back(i);
  }

  totalFields = dtcFieldIndex.size();
  if (totalFields <= 0)
  {
    log_error("field can not be empty!");
    return -1;
  }

  mCompoundKeyFieldNums = mTableDef->uniq_fields();
  if (mCompoundKeyFieldNums <= 0)
  {
    log_error("not found unique constraint in any field!");
    return -1;
  }
  mExtraValueFieldNums = totalFields - mCompoundKeyFieldNums;
  log_info("total fields:%d, uniqKeyNum:%d, valueNum:%d", totalFields, mCompoundKeyFieldNums, mExtraValueFieldNums);

  // create map relationship
  uint8_t keyIndex;
  uint8_t *uniqFields = mTableDef->uniq_fields_list();
  for (int idx = 0; idx < mCompoundKeyFieldNums; idx++)
  {
    keyIndex = *(uniqFields + idx);
    dtcFieldIndex[keyIndex] = -1;
    mFieldIndexMapping.push_back(keyIndex);
  }

  if (dtcFieldIndex.size() <= 0)
  {
    log_error("no value field!");
    return -1;
  }

  // classify the unique keys into two types: Integer fixed len and elastic string type,
  // no need to do collecting if the key is binary
  // int keyType = mTableDef->key_type();
  mKeyfield_types.resize(mCompoundKeyFieldNums);
  // mKeyfield_types[0] = keyType;

  {
    // shrink string keys or integer keys into the head of the array
    int fieldType;
    // int moveHeadIdx = -1;
    for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
    {
      fieldType = mTableDef->field_type(mFieldIndexMapping[idx]);
      mKeyfield_types[idx] = fieldType;
      log_info("fieldId:%d, fieldType:%d", mFieldIndexMapping[idx], fieldType);
      switch (fieldType)
      {
      case DField::Signed:
      case DField::Unsigned:
      case DField::Float:
      case DField::Binary:
        break;

      case DField::String:
      {
        mNoBitmapKey = false;
        break;
      }

      default:
        log_error("unexpected field type! type:%d", fieldType);
        return -1;
      };
    }
  }

  // remove key from vector
  auto start = std::remove_if(dtcFieldIndex.begin(), dtcFieldIndex.end(),
                              [](const int idx) { return idx == -1; });
  dtcFieldIndex.erase(start, dtcFieldIndex.end());

  // append value maps
  mFieldIndexMapping.insert(mFieldIndexMapping.end(), dtcFieldIndex.begin(), dtcFieldIndex.end());

  {
    mReverseFieldIndexMapping.resize(mFieldIndexMapping.size());
    for (size_t idx1 = 0; idx1 < mFieldIndexMapping.size(); idx1++)
    {
      mReverseFieldIndexMapping[mFieldIndexMapping[idx1]] = idx1;
    }
  }

  // init replication tag key
  ret = get_replicate_end_key();

  std::stringstream ss;
  ss << "rocks helper meta info, keysize:" << mCompoundKeyFieldNums << " valuesize:"
     << mExtraValueFieldNums << " rocksdb fields:[";
  for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
  {
    log_info("%d, type:%d", mFieldIndexMapping[idx], idx < mCompoundKeyFieldNums ? mKeyfield_types[idx] : -1);
    if (idx == 0)
      ss << mFieldIndexMapping[idx];
    else
      ss << ", " << mFieldIndexMapping[idx];
  }
  ss << "]";
  log_info("%s", ss.str().c_str());

  return ret;
}

int RocksdbProcess::startReplListener()
{
  // init replication related
  mReplUnit = new RocksdbReplication(mDBConn);
  if (!mReplUnit)
  {
    log_error("%s", "create replication unit failed");
    return -1;
  }

  return mReplUnit->initializeReplication();
}

int RocksdbProcess::get_replicate_end_key()
{
  return 0;
  std::string value;
  std::string fullKey = gReplicatePrefixKey;
  int ret = mDBConn->get_entry(fullKey, value, RocksDBConn::COLUMN_META_DATA);
  if (ret < 0 && ret != -RocksDBConn::ERROR_KEY_NOT_FOUND)
  {
    log_error("query replicate end key failed! ret:%d", ret);
    return -1;
  }
  else
  {
    mReplEndKey = value;
  }

  return 0;
}

inline int RocksdbProcess::value_add_to_str(
    const DTCValue *additionValue,
    int ifield_type,
    std::string &baseValue)
{
  log_debug("value_add_to_str ifield_type[%d]", ifield_type);

  if (additionValue == NULL)
  {
    log_error("value can not be null!");
    return -1;
  }

  switch (ifield_type)
  {
  case DField::Signed:
  {
    long long va = strtoll(baseValue.c_str(), NULL, 10);
    va += (long long)additionValue->s64;
    baseValue = std::to_string(va);
    break;
  }

  case DField::Unsigned:
  {
    unsigned long long va = strtoull(baseValue.c_str(), NULL, 10);
    va += (unsigned long long)additionValue->u64;
    baseValue = std::to_string(va);
    break;
  }

  case DField::Float:
  {
    double va = strtod(baseValue.c_str(), NULL);
    va += additionValue->flt;
    baseValue = std::to_string(va);
    break;
  }

  case DField::String:
  case DField::Binary:
    log_error("string type can not do add operation!");
    break;

  default:
    log_error("unexpected field type! type:%d", ifield_type);
    return -1;
  };

  return 0;
}

inline int RocksdbProcess::value2Str(
    const DTCValue *Value,
    int fieldId,
    std::string &strValue)
{
  const DTCValue *defaultValue;
  bool valueNull = false;

  if (Value == NULL)
  {
    log_info("value is null, use user default value!");
    defaultValue = mTableDef->default_value(fieldId);
    valueNull = true;
  }

  int ifield_type = mTableDef->field_type(fieldId);
  {
    switch (ifield_type)
    {
    case DField::Signed:
    {
      int64_t val;
      if (valueNull)
        val = defaultValue->s64;
      else
        val = Value->s64;
      strValue = std::move(std::to_string((long long)val));
      break;
    }

    case DField::Unsigned:
    {
      uint64_t val;
      if (valueNull)
        val = defaultValue->u64;
      else
        val = Value->u64;
      strValue = std::move(std::to_string((unsigned long long)val));
      break;
    }

    case DField::Float:
    {
      double val;
      if (valueNull)
        val = defaultValue->flt;
      else
        val = Value->flt;
      strValue = std::move(std::to_string(val));
      break;
    }

    case DField::String:
    case DField::Binary:
    {
      // value whether be "" or NULL ????
      // in current, regard NULL as empty string, not support NULL attribute here
      if (valueNull)
        strValue = std::move(std::string(defaultValue->str.ptr, defaultValue->str.len));
      else
      {
        if (Value->str.is_empty())
        {
          log_info("empty str value!");
          strValue = "";
          return 0;
        }

        strValue = std::move(std::string(Value->str.ptr, Value->str.len));
        /*if ( mkey_type == DField::String )
            {
          // case insensitive
          std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
          }*/
      }
      break;
    }
    default:
      log_error("unexpected field type! type:%d", ifield_type);
      return -1;
    };
  }

  return 0;
}

inline int RocksdbProcess::setdefault_value(
    int field_type,
    DTCValue &Value)
{
  switch (field_type)
  {
  case DField::Signed:
    Value.s64 = 0;
    break;

  case DField::Unsigned:
    Value.u64 = 0;
    break;

  case DField::Float:
    Value.flt = 0.0;
    break;

  case DField::String:
    Value.str.len = 0;
    Value.str.ptr = 0;
    break;

  case DField::Binary:
    Value.bin.len = 0;
    Value.bin.ptr = 0;
    break;

  default:
    Value.s64 = 0;
  };

  return (0);
}

inline int RocksdbProcess::str2Value(
    const std::string &Str,
    int field_type,
    DTCValue &Value)
{
  if (Str == NULL)
  {
    log_debug("Str is NULL, field_type: %d. Check mysql table definition.", field_type);
    setdefault_value(field_type, Value);
    return (0);
  }

  switch (field_type)
  {
  case DField::Signed:
    errno = 0;
    Value.s64 = strtoll(Str.c_str(), NULL, 10);
    if (errno != 0)
      return (-1);
    break;

  case DField::Unsigned:
    errno = 0;
    Value.u64 = strtoull(Str.c_str(), NULL, 10);
    if (errno != 0)
      return (-1);
    break;

  case DField::Float:
    errno = 0;
    Value.flt = strtod(Str.c_str(), NULL);
    if (errno != 0)
      return (-1);
    break;

  case DField::String:
    {
      char* p = (char*)calloc(Str.length() , sizeof(char));
      memcpy((void*)p , (void*)Str.data() , Str.length());
      Value.str.ptr = p;
      Value.str.len = Str.length();
    }
    break;

  case DField::Binary:
    {
      char* p = (char*)calloc(Str.length() , sizeof(char));
      memcpy((void*)p , (void*)Str.data() , Str.length());
      Value.bin.ptr = p;
      Value.bin.len = Str.length();
    }
    break;

  default:
    log_error("type[%d] invalid.", field_type);
    return -1;
  }

  return 0;
}

int RocksdbProcess::condition_filter(
    const std::string &rocksValue,
    int fieldid,
    int fieldType,
    const DTCFieldValue *condition)
{
  if (condition == NULL)
    return 0;

  bool matched;
  // find out the condition value
  for (int idx = 0; idx < condition->num_fields(); idx++)
  {
    if (mTableDef->is_volatile(idx))
    {
      log_error("volatile field, idx:%d", idx);
      return -1;
    }

    int fId = condition->field_id(idx);
    if (fId != fieldid)
      continue;

    // DTC support query condition
    /* enum {
      EQ = 0,
      NE = 1,
      LT = 2,
      LE = 3,
      GT = 4,
      GE = 5,
      TotalComparison
    }; */
    int comparator = condition->field_operation(idx);
    const DTCValue *condValue = condition->field_value(idx);

    switch (fieldType)
    {
    case DField::Signed:
      // matched = is_matched_template(strtoll(rocksValue.c_str(), NULL, 10), comparator, condValue.s64);
      matched = is_matched<int64_t>(strtoll(rocksValue.c_str(), NULL, 10), comparator, condValue->s64);
      if (!matched)
      {
        log_info("not match the condition, lv:%s, rv:%lld, com:%d", rocksValue.c_str(),
                 (long long)condValue->s64, comparator);
        return 1;
      }
      break;

    case DField::Unsigned:
      // matched = is_matched_template(strtoull(rocksValue.c_str(), NULL, 10), comparator, condValue.u64);
      matched = is_matched<uint64_t>(strtoull(rocksValue.c_str(), NULL, 10), comparator, condValue->u64);
      if (!matched)
      {
        log_info("not match the condition, lv:%s, rv:%llu, com:%d", rocksValue.c_str(),
                 (unsigned long long)condValue->u64, comparator);
        return 1;
      }
      break;

    case DField::Float:
      // matched = is_matched_template(strtod(rocksValue.c_str(), NULL, 10), comparator, condValue.flt);
      matched = is_matched<double>(strtod(rocksValue.c_str(), NULL), comparator, condValue->flt);
      if (!matched)
      {
        log_info("not match the condition, lv:%s, rv:%lf, com:%d", rocksValue.c_str(),
                 condValue->flt, comparator);
        return 1;
      }
      break;

    case DField::String:
      matched = is_matched(rocksValue.c_str(), comparator, condValue->str.ptr, (int)rocksValue.length(), condValue->str.len, false);
      if (!matched)
      {
        log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
                 std::string(condValue->str.ptr, condValue->str.len).c_str(), comparator);
        return 1;
      }
    case DField::Binary:
      // matched = is_matched_template(rocksValue.c_str(), comparator, condValue.str.ptr, (int)rocksValue.length(), condValue.str.len);
      matched = is_matched(rocksValue.c_str(), comparator, condValue->bin.ptr, (int)rocksValue.length(), condValue->bin.len, true);
      if (!matched)
      {
        log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
                 std::string(condValue->bin.ptr, condValue->bin.len).c_str(), comparator);
        return 1;
      }
      break;

    default:
      log_error("field[%d] type[%d] invalid.", fieldid, fieldType);
      return -1;
    }
  }

  return 0;
}

int RocksdbProcess::condition_filter(
    const std::string &rocksValue,
    const std::string &condValue,
    int fieldType,
    int comparator)
{
  bool matched;
  switch (fieldType)
  {
  case DField::Signed:
    matched = is_matched<int64_t>(strtoll(rocksValue.c_str(), NULL, 10), comparator, strtoll(condValue.c_str(), NULL, 10));
    if (!matched)
    {
      log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
               condValue.c_str(), comparator);
      return 1;
    }
    break;

  case DField::Unsigned:
    matched = is_matched<uint64_t>(strtoull(rocksValue.c_str(), NULL, 10), comparator, strtoull(condValue.c_str(), NULL, 10));
    if (!matched)
    {
      log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
               condValue.c_str(), comparator);
      return 1;
    }
    break;

  case DField::Float:
    matched = is_matched<double>(strtod(rocksValue.c_str(), NULL), comparator, strtod(condValue.c_str(), NULL));
    if (!matched)
    {
      log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
               condValue.c_str(), comparator);
      return 1;
    }
    break;

  case DField::String:
  case DField::Binary:
  {
    matched = is_matched(rocksValue.c_str(), comparator, condValue.c_str(), (int)rocksValue.length(), (int)condValue.length(), false);
    if (!matched)
    {
      log_info("not match the condition, lv:%s, rv:%s, com:%d", rocksValue.c_str(),
               condValue.c_str(), comparator);
      return 1;
    }
  }
    break;

  default:
    log_error("invalid field type[%d].", fieldType);
    return -1;
  }

  return 0;
}

template <class... Args>
bool RocksdbProcess::is_matched_template(Args... len)
{
  return is_matched(len...);
}

template <class T>
bool RocksdbProcess::is_matched(
    const T lv,
    int comparator,
    const T rv)
{
  /* enum {
     EQ = 0,
     NE = 1,
     LT = 2,
     LE = 3,
     GT = 4,
     GE = 5,
     TotalComparison
     }; */
  switch (comparator)
  {
  case 0:
    return lv == rv;
  case 1:
    return lv != rv;
  case 2:
    return lv < rv;
  case 3:
    return lv <= rv;
  case 4:
    return lv > rv;
  case 5:
    return lv >= rv;
  default:
    log_error("unsupport comparator:%d", comparator);
  }

  return false;
}
template bool RocksdbProcess::is_matched<int64_t>(const int64_t lv, int comp, const int64_t rv);
template bool RocksdbProcess::is_matched<uint64_t>(const uint64_t lv, int comp, const uint64_t rv);
template bool RocksdbProcess::is_matched<double>(const double lv, int comp, const double rv);

int RocksdbProcess::memcmp_ignore_case(
    const void* lv, 
    const void* rv, 
    int count)
{
  int iret = 0;
  for (int i = 0; i < count; i++){
    char lv_buffer = tolower(((char*)lv)[i]);
    char rv_buffer = tolower(((char*)rv)[i]);
    iret = memcmp(&lv_buffer , &rv_buffer , sizeof(char));
    if (iret != 0){
      return iret;
    }
  }
  return iret;
}

//template<>
bool RocksdbProcess::is_matched(
    const char *lv,
    int comparator,
    const char *rv,
    int lLen,
    int rLen,
    bool caseSensitive)
{
  /* enum {
     EQ = 0,
     NE = 1,
     LT = 2,
     LE = 3,
     GT = 4,
     GE = 5,
     TotalComparison
     }; */
  int ret;
  int minLen = lLen <= rLen ? lLen : rLen;
  switch (comparator)
  {
  case 0:
    if (caseSensitive)
      return lLen == rLen && !memcmp(lv, rv, minLen);
    return lLen == rLen && !memcmp_ignore_case(lv, rv, minLen);
  case 1:
    if (lLen != rLen)
      return true;
    if (caseSensitive)
      return memcmp(lv, rv, minLen);
    return memcmp_ignore_case(lv, rv, minLen);
  case 2:
    if (caseSensitive)
      ret = memcmp(lv, rv, minLen);
    else
      ret = memcmp_ignore_case(lv, rv, minLen);
    return ret < 0 || (ret == 0 && lLen < rLen);
  case 3:
    if (caseSensitive)
      ret = memcmp(lv, rv, minLen);
    else
      ret = memcmp_ignore_case(lv, rv, minLen);
    log_error("iret:%d , len:%d ,rLen:%d", ret , lLen , rLen);
    return ret < 0 || (ret == 0 && lLen <= rLen);
  case 4:
    if (caseSensitive)
      ret = memcmp(lv, rv, minLen);
    else
      ret = memcmp_ignore_case(lv, rv, minLen);
    return ret > 0 || (ret == 0 && lLen > rLen);
  case 5:
    if (caseSensitive)
      ret = memcmp(lv, rv, minLen);
    else
      ret = memcmp_ignore_case(lv, rv, minLen);
    return ret > 0 || (ret == 0 && lLen >= rLen);
  default:
    log_error("unsupport comparator:%d", comparator);
  }

  return false;
}

int RocksdbProcess::saveRow(
    const std::string &compoundKey,
    const std::string &compoundValue,
    bool countOnly,
    int &totalRows,
    DTCTask *Task)
{
  if (mCompoundKeyFieldNums + mExtraValueFieldNums <= 0)
  {
    log_error("no fields in the table! key:%s");
    return (-1);
  }

  int ret;
  // decode the compoundKey and check whether it is matched
  std::vector<std::string> keys;
  key_format::Decode(compoundKey, mKeyfield_types, keys);
  if (keys.size() != mCompoundKeyFieldNums)
  {
    // unmatched row
    log_error("unmatched row, fullKey:%s, keyNum:%lu, definitionFieldNum:%d",
              compoundKey.c_str(), keys.size(), mCompoundKeyFieldNums);
    return -1;
  }

  if (countOnly)
  {
    totalRows++;
    return 0;
  }

  // decode key bitmap
  int bitmapLen = 0;
  decodeBitmapKeys(compoundValue, keys, bitmapLen);

  //DBConn.Row[0]是key的值，mTableDef->Field[0]也是key，
  //因此从1开始。而结果Row是从0开始的(不包括key)
  RowValue *row = new RowValue(mTableDef);
  const DTCFieldValue *Condition = Task->request_condition();
  std::string fieldValue;
  char *valueHead = const_cast<char *>(compoundValue.data()) + bitmapLen;
  for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
  {
    int fieldId = mFieldIndexMapping[idx];
    if (idx < mCompoundKeyFieldNums)
    {
      fieldValue = keys[idx];
    }
    else
    {
      ret = get_value_by_id(valueHead, fieldId, fieldValue);
      if (ret != 0)
      {
        log_error("parse field value failed! compoundValue:%s", compoundValue.c_str());
        delete row;
        return -1;
      }
    }
    log_info("save row, fieldId:%d, val:%s", fieldId, fieldValue.data());

    // do condition filter
    ret = condition_filter(fieldValue, fieldId, mTableDef->field_type(fieldId), Condition);
    if (ret < 0)
    {
      delete row;
      log_error("string[%s] conver to value[%d] error: %d", fieldValue.c_str(), mTableDef->field_type(fieldId), ret);
      return (-2);
    }
    else if (ret == 1)
    {
      // condition is not matched
      delete row;
      return 0;
    }

    // fill the value
    ret = str2Value(fieldValue, mTableDef->field_type(fieldId), (*row)[fieldId]);
    if (ret < 0)
    {
      delete row;
      log_error("string[%s] conver to value[%d] error: %d", fieldValue.c_str(), mTableDef->field_type(fieldId), ret);
      return (-2);
    }
  }

  // Task->update_key(row);
  ret = Task->append_row(row);

  delete row;

  if (ret < 0)
  {
    log_error("append row to task failed!");
    return (-3);
  }

  // totalRows++;

  return (0);
}

int RocksdbProcess::save_direct_row(
    const std::string &prefixKey,
    const std::string &compoundKey,
    const std::string &compoundValue,
    DirectRequestContext *reqCxt,
    DirectResponseContext *respCxt,
    int &totalRows)
{
  if (mCompoundKeyFieldNums + mExtraValueFieldNums <= 0)
  {
    log_error("no fields in the table! key:%s", prefixKey.c_str());
    return (-1);
  }

  int ret;
  // decode the compoundKey and check whether it is matched
  std::vector<std::string> keys;
  key_format::Decode(compoundKey, mKeyfield_types, keys);
  if (keys.size() != mCompoundKeyFieldNums)
  {
    // unmatched row
    log_error("unmatched row, key:%s, fullKey:%s, keyNum:%lu, definitionFieldNum:%d",
              prefixKey.c_str(), compoundKey.c_str(), keys.size(), mCompoundKeyFieldNums);
    return -1;
  }

  // decode key bitmap
  int bitmapLen = 0;
  decodeBitmapKeys(compoundValue, keys, bitmapLen);

  std::string realValue = compoundValue.substr(bitmapLen);

  std::vector<std::string> values;
  split_values(realValue, values);
  assert(values.size() == mExtraValueFieldNums);

  int fieldId, rocksFId;
  std::string fieldValue;
  std::vector<QueryCond>& condFields = ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sFieldConds;
  for (size_t idx = 0; idx < condFields.size(); idx++)
  {
    fieldId = condFields[idx].sFieldIndex;
    rocksFId = translate_field_idx(fieldId);
    if (rocksFId < mCompoundKeyFieldNums)
    {
      fieldValue = keys[rocksFId];
    }
    else
    {
      fieldValue = values[rocksFId - mCompoundKeyFieldNums];
    }

    // do condition filter
    ret = condition_filter(fieldValue, condFields[idx].sCondValue, mTableDef->field_type(fieldId), condFields[idx].sCondOpr);
    if (ret < 0)
    {
      log_error("condition filt failed! key:%s", prefixKey.c_str());
      return -1;
    }
    else if (ret == 1)
    {
      // condition is not matched
      return 0;
    }
  }

  // deal with order by syntax
  if (mOrderByUnit || ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sOrderbyFields.size() > 0)
  {
    if (!mOrderByUnit)
    {
      // build order by unit
      int heapSize = ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart >= 0 && ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStep > 0 ?
            ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart + ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStep : 50;
      mOrderByUnit = new RocksdbOrderByUnit(mTableDef, heapSize, 
          mReverseFieldIndexMapping, ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sOrderbyFields);
      assert(mOrderByUnit);
    }

    struct OrderByUnitElement element;
    element.mRocksKeys.swap(keys);
    element.mRocksValues.swap(values);
    mOrderByUnit->add_row(element);
    return 0;
  }

  // limit condition control
  ret = 0;
  if (((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart >= 0 && ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStep > 0)
  {
    if (totalRows < ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart)
    {
      // not reach to the range of limitation
      totalRows++;
      return 0;
    }
    
    // leaving from the range of limitaion
    if (((RangeQueryRows_t*)respCxt->sDirectRespValue.uRangeQueryRows)->sRowValues.size() == ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStep - 1) ret = 1;
  }

  // build row
  build_direct_row(keys, values, respCxt);
  totalRows++;

  return ret;
}

void RocksdbProcess::build_direct_row(
    const std::vector<std::string> &keys,
    const std::vector<std::string> &values,
    DirectResponseContext *respCxt)
{
  int rocksFId;
  std::string row, fieldValue;
  for (size_t idx1 = 0; idx1 < mReverseFieldIndexMapping.size(); idx1++)
  {
    rocksFId = mReverseFieldIndexMapping[idx1];
    if (rocksFId < mCompoundKeyFieldNums)
    {
      fieldValue = keys[rocksFId];
    }
    else
    {
      fieldValue = values[rocksFId - mCompoundKeyFieldNums];
    }

    int dataLen = fieldValue.length();
    row.append(std::string((char *)&dataLen, 4)).append(fieldValue);
  }

  ((RangeQueryRows_t*)respCxt->sDirectRespValue.uRangeQueryRows)->sRowValues.push_front(row);
  return;
}

int RocksdbProcess::update_row(
    const std::string &prefixKey,
    const std::string &compoundKey,
    const std::string &compoundValue,
    DTCTask *Task,
    std::string &newKey,
    std::string &newValue)
{
  if (mCompoundKeyFieldNums + mExtraValueFieldNums <= 0)
  {
    log_error("no fields in the table!");
    return (-1);
  }

  int ret;
  // decode the compoundKey and check whether it is matched
  std::vector<std::string> keys;
  key_format::Decode(compoundKey, mKeyfield_types, keys);

  if (keys.size() != mCompoundKeyFieldNums)
  {
    // unmatched row
    log_error("unmatched row, key:%s, fullKey:%s, keyNum:%lu, definitionFieldNum:%d",
              prefixKey.c_str(), compoundKey.c_str(), keys.size(), mCompoundKeyFieldNums);
    return -1;
  }

  bool upKey = false, upVal = false;
  const DTCFieldValue *updateInfo = Task->request_operation();
  whether_update_key(updateInfo, upKey, upVal);

  int keyBitmapLen = 0;
  if (upKey)
  {
    // recover keys for the next update
    decodeBitmapKeys(compoundValue, keys, keyBitmapLen);
  }
  else
  {
    // no need to create bitmap and compound key again
    keyBitmapLen = get_key_bitmap_len(compoundValue);
    assert(keyBitmapLen >= 0);
  }

  std::string bitmapKey = compoundValue.substr(0, keyBitmapLen);
  std::string realValue = compoundValue.substr(keyBitmapLen);

  std::string fieldValue;
  const DTCFieldValue *Condition = Task->request_condition();
  char *valueHead = (char *)realValue.data();
  for (size_t idx = 1; idx < mFieldIndexMapping.size(); idx++)
  {
    int dtcfid = mFieldIndexMapping[idx];
    if (idx < mCompoundKeyFieldNums)
    {
      fieldValue = keys[idx];
    }
    else
    {
      ret = get_value_by_id(valueHead, dtcfid, fieldValue);
      if (ret != 0)
      {
        log_error("parse field value failed! compoundValue:%s, key:%s", realValue.c_str(), prefixKey.c_str());
        return -1;
      }
    }

    // do condition filter
    ret = condition_filter(fieldValue, dtcfid, mTableDef->field_type(dtcfid), Condition);
    if (ret < 0)
    {
      log_error("string[%s] conver to value[%d] error: %d, %m", fieldValue.c_str(), mTableDef->field_type(dtcfid), ret);
      return (-2);
    }
    else if (ret == 1)
    {
      // condition is not matched
      return 1;
    }
  }

  // update value
  std::vector<std::string> values;
  if (upVal)
  {
    // translate the plane raw value to individual field
    split_values(realValue, values);
  }

  for (int i = 0; i < updateInfo->num_fields(); i++)
  {
    const int fid = updateInfo->field_id(i);

    if (mTableDef->is_volatile(fid))
      continue;

    int rocksFId = translate_field_idx(fid);
    assert(rocksFId >= 0 && rocksFId < mCompoundKeyFieldNums + mExtraValueFieldNums);

    // if not update the value field, the rocksfid must be less than 'mCompoundKeyFieldNums', so it would not touch
    // the container of 'values'
    std::string &va = rocksFId < mCompoundKeyFieldNums ? keys[rocksFId] : values[rocksFId - mCompoundKeyFieldNums];

    switch (updateInfo->field_operation(i))
    {
    case DField::Set:
      value2Str(updateInfo->field_value(i), fid, va);
      break;

    case DField::Add:
      value_add_to_str(updateInfo->field_value(i), updateInfo->field_type(i), va);
      break;

    default:
      log_error("unsupport operation, opr:%d", updateInfo->field_operation(i));
      return -1;
    };
  }

  if (upKey)
  {
    bitmapKey.clear();
    encode_bitmap_keys(keys, bitmapKey);

    newKey = std::move(key_format::Encode(keys, mKeyfield_types));
  }
  else
    newKey = compoundKey;

  if (upVal)
    shrink_value(values, newValue);
  else
    newValue = std::move(realValue);

  newValue = std::move(bitmapKey.append(newValue));

  return 0;
}

// query value of the key
int RocksdbProcess::process_select(DTCTask *Task)
{
  log_info("come into process select!");

#ifdef PRINT_STAT
  mSTime = GET_TIMESTAMP();
#endif

  int ret, i;
  int haslimit = !Task->count_only() && (Task->requestInfo.limit_start() || Task->requestInfo.limit_count());

  // create resultWriter
  ret = Task->prepare_result_no_limit();
  if (ret != 0)
  {
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "task prepare-result error");
    log_error("task prepare-result error: %d, %m", ret);
    return (-2);
  }

  // prefix key
  std::string prefixKey;
  ret = value2Str(Task->request_key(), 0, prefixKey);
  if (ret != 0 || prefixKey.empty())
  {
    log_error("dtc primary key can not be empty!");
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get dtc primary key failed!");
    return -1;
  }

  if (mKeyfield_types[0] == DField::String)
    std::transform(prefixKey.begin(), prefixKey.end(), prefixKey.begin(), ::tolower);


  // encode the key to rocksdb format
  std::string fullKey = std::move(key_format::encode_bytes(prefixKey));
  if (fullKey.empty())
  {
    log_error("encode primary key failed! key:%s", prefixKey.c_str());
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "encode primary key failed!");
    return -1;
  }

  std::string encodePreKey = fullKey;

  std::string value;
  RocksDBConn::RocksItr_t rocksItr;
  ret = mDBConn->retrieve_start(fullKey, value, rocksItr, true);
  if (ret < 0)
  {
    log_error("query rocksdb failed! key:%s, ret:%d", prefixKey.c_str(), ret);
    Task->set_error(ret, __FUNCTION__, "retrieve primary key failed!");
    mDBConn->retrieve_end(rocksItr);
    return -1;
  }
  else if (ret == 1)
  {
    // not found the key
    Task->set_total_rows(0);
    log_info("no matched key:%s", prefixKey.c_str());
    mDBConn->retrieve_end(rocksItr);
    return 0;
  }

  log_info("begin to iterate key:%s", prefixKey.c_str());

  // iterate the matched prefix key and find out the real one from start to end
  int totalRows = 0;
  bool countOnly = Task->count_only();
  while (true)
  {
    ret = key_matched(encodePreKey, fullKey);
    if (ret != 0)
    {
      // prefix key not matched, reach to the end
      break;
    }

    // save row
    ret = saveRow(fullKey, value, countOnly, totalRows, Task);
    if (ret < 0)
    {
      // ignore the incorrect key and keep going
      log_error("save row failed! key:%s", prefixKey.c_str());
    }

    // move iterator to the next key
    ret = mDBConn->next_entry(rocksItr, fullKey, value);
    if (ret < 0)
    {
      log_error("iterate rocksdb failed! key:%s", prefixKey.c_str());
      Task->set_error(ret, __FUNCTION__, "iterate rocksdb failed!");
      mDBConn->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 1)
    {
      // reach to the storage end
      break;
    }

    // has remaining value in rocksdb
  }

  if (Task->count_only())
  {
    Task->set_total_rows(totalRows);
  }

  //bug fixed确认客户端带Limit限制
  if (haslimit)
  { // 获取总行数
    Task->set_total_rows(totalRows, 1);
  }

  mDBConn->retrieve_end(rocksItr);

#ifdef PRINT_STAT
  mETime = GET_TIMESTAMP();
  insert_stat(OprType::eQuery, mETime - mSTime);
#endif

  return (0);
}

int RocksdbProcess::process_insert(DTCTask *Task)
{
  log_info("come into process insert!!!");

#ifdef PRINT_STAT
  mSTime = GET_TIMESTAMP();
#endif

  int ret;

  set_title("INSERT...");

  int totalFields = mTableDef->num_fields();

  // initialize the fixed empty string vector
  std::bitset<256> filledMap;
  std::vector<std::string> keys(mCompoundKeyFieldNums);
  std::vector<std::string> values(mExtraValueFieldNums);

  ret = value2Str(Task->request_key(), 0, keys[0]);
  if (ret != 0 || keys[0].empty())
  {
    log_error("dtc primary key can not be empty!");
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get dtc primary key failed!");
    return -1;
  }
  else
  {
    filledMap[0] = 1;
  }
  log_info("insert key:%s", keys[0].c_str());

  if (Task->request_operation())
  {
    int fid, rocksFId;
    const DTCFieldValue *updateInfo = Task->request_operation();
    for (int i = 0; i < updateInfo->num_fields(); ++i)
    {
      fid = updateInfo->field_id(i);
      if (mTableDef->is_volatile(fid))
        continue;

      rocksFId = translate_field_idx(fid);
      assert(rocksFId >= 0 && rocksFId < mCompoundKeyFieldNums + mExtraValueFieldNums);

      if (fid == 0)
        continue;

      std::string &va = rocksFId < mCompoundKeyFieldNums ? keys[rocksFId] : values[rocksFId - mCompoundKeyFieldNums];
      ret = value2Str(updateInfo->field_value(i), fid, va);
      assert(ret == 0);

      filledMap[fid] = 1;
    }
  }

  // set default value
  for (int i = 1; i <= totalFields; ++i)
  {
    int rocksFId;
    if (mTableDef->is_volatile(i))
      continue;

    if (filledMap[i])
      continue;

    rocksFId = translate_field_idx(i);
    assert(rocksFId >= 0 && rocksFId < mCompoundKeyFieldNums + mExtraValueFieldNums);

    std::string &va = rocksFId < mCompoundKeyFieldNums ? keys[rocksFId] : values[rocksFId - mCompoundKeyFieldNums];
    ret = value2Str(mTableDef->default_value(i), i, va);
    assert(ret == 0);
  }

#ifdef DEBUG_INFO
  std::stringstream ss;
  ss << "insert row:[";
  for (size_t idx = 0; idx < mCompoundKeyFieldNums; idx++)
  {
    ss << keys[idx];
    if (idx != mCompoundKeyFieldNums - 1)
      ss << ",";
  }
  ss << "]";
  log_error("%s", ss.str().c_str());
#endif

  // convert string type 'key' into lower case and build case letter bitmap
  std::string keyBitmaps;
  encode_bitmap_keys(keys, keyBitmaps);

  std::string rocksKey, rocksValue;
  rocksKey = std::move(key_format::Encode(keys, mKeyfield_types));

  // value use plane style to organize, no need to encode
  ret = shrink_value(values, rocksValue);
  if (ret != 0)
  {
    std::string rkey;
    value2Str(Task->request_key(), 0, rkey);
    log_error("shrink value failed, key:%s", rkey.c_str());

    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "shrink buff failed!");
    return -1;
  }

  // add key bitmaps to the rocksdb value field
  keyBitmaps.append(rocksValue);

  log_info("save kv, key:%s, value:%s", rocksKey.c_str(), rocksValue.c_str());
  ret = mDBConn->insert_entry(rocksKey, keyBitmaps, true);
  if (ret != 0)
  {
    std::string rkey;
    value2Str(Task->request_key(), 0, rkey);
    if (ret != -ER_DUP_ENTRY)
    {
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "insert key failed!");
      log_error("insert key failed, ret:%d, key:%s", ret, rkey.c_str());
    }
    else
    {
      Task->set_error_dup(ret, __FUNCTION__, "insert entry failed!");
      log_error("insert duplicate key : %s", rkey.c_str());
    }

    return (-1);
  }

  Task->resultInfo.set_affected_rows(1);

#ifdef PRINT_STAT
  mETime = GET_TIMESTAMP();
  insert_stat(OprType::eInsert, mETime - mSTime);
#endif

  return (0);
}

// update the exist rows matched the condition
int RocksdbProcess::process_update(DTCTask *Task)
{
  log_info("come into rocks update");

#ifdef PRINT_STAT
  mSTime = GET_TIMESTAMP();
#endif

  int ret, affectRows = 0;

  // prefix key
  std::string prefixKey;
  ret = value2Str(Task->request_key(), 0, prefixKey);
  if (ret != 0)
  {
    log_error("get dtc primary key failed! key");
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get key failed!");
    return -1;
  }
  log_info("update key:%s", prefixKey.c_str());

  if (Task->request_operation() == NULL)
  {
    log_info("request operation info is null!");
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "update field not found");
    return (-1);
  }

  if (Task->request_operation()->has_type_commit() == 0)
  {
    // pure volatile fields update, always succeed
    log_info("update pure volatile fields!");
    Task->resultInfo.set_affected_rows(affectRows);
    return (0);
  }

  const DTCFieldValue *updateInfo = Task->request_operation();
  if (updateInfo == NULL)
  {
    // no need to do update
    log_info("request update info is null!");
    Task->resultInfo.set_affected_rows(affectRows);
    return (0);
  }

  set_title("UPDATE...");

  bool updateKey = false, updateValue = false;
  whether_update_key(updateInfo, updateKey, updateValue);
  if (!updateKey && !updateValue)
  {
    // no need to do update
    log_info("no change for the row!");
    Task->resultInfo.set_affected_rows(affectRows);
    return (0);
  }

  if (mKeyfield_types[0] == DField::String)
    std::transform(prefixKey.begin(), prefixKey.end(), prefixKey.begin(), ::tolower);

  // encode the key to rocksdb format
  std::string fullKey = std::move(key_format::encode_bytes(prefixKey));
  std::string encodePreKey = fullKey;

  std::string compoundValue;
  RocksDBConn::RocksItr_t rocksItr;
  ret = mDBConn->retrieve_start(fullKey, compoundValue, rocksItr, true);
  if (ret < 0)
  {
    log_info("retrieve rocksdb failed, key:%s", prefixKey.c_str());
    Task->set_error_dup(ret, __FUNCTION__, "retrieve rocksdb failed!");
    mDBConn->retrieve_end(rocksItr);
    return -1;
  }
  else if (ret == 1)
  {
    // not found the key
    log_info("no matched key:%s", prefixKey.c_str());
    Task->resultInfo.set_affected_rows(affectRows);
    mDBConn->retrieve_end(rocksItr);
    return 0;
  }

  // retrieve the range keys one by one
  std::set<std::string> deletedKeys;
  std::map<std::string, std::string> newEntries;
  if (updateKey)
  {
    // Will update the row, so we need to keep the whole row in the memory for checking
    // the unique constraints
    // Due to the limitaion of the memory, we can not hold all rows in the memory sometimes,
    // so use the LRU to evit the oldest row
    std::set<std::string> originKeys; // keys located in rocksdb those before doing update
    while (true)
    {
      // find if the key has been update before, if yes, should rollback the previously update
      auto itr = newEntries.find(fullKey);
      if (itr != newEntries.end())
      {
        log_info("duplicated entry, key:%s", fullKey.c_str());
        // report alarm
        Task->set_error_dup(-ER_DUP_ENTRY, __FUNCTION__, "duplicate key!");
        mDBConn->retrieve_end(rocksItr);
        return -1;
      }

      ret = key_matched(encodePreKey, fullKey);
      if (ret != 0)
      {
        // prefix key not matched, reach to the end
        break;
      }

      // update row
      std::string newKey, newValue;
      ret = update_row(prefixKey, fullKey, compoundValue, Task, newKey, newValue);
      if (ret < 0)
      {
        // ignore the incorrect key and keep going
        log_error("save row failed! key:%s, compoundValue:%s", fullKey.c_str(), compoundValue.c_str());
      }
      else if (ret == 1)
      {
        // key matched, but condition mismatched, keep on retrieve
        originKeys.insert(fullKey);
      }
      else
      {
        {
          ret = rocks_key_matched(fullKey, newKey);
          if (ret == 0)
          {
            log_info("duplicated entry, newkey:%s", newKey.c_str());
            Task->set_error_dup(-ER_DUP_ENTRY, __FUNCTION__, "duplicate key!");
            mDBConn->retrieve_end(rocksItr);
            return -1;
          }
        }

        if ( originKeys.find(newKey) == originKeys.end() 
            && newEntries.find(newKey) == newEntries.end()
            && deletedKeys.find(newKey) == deletedKeys.end() )
        {
          // there are so many duplcate string save in the different containers, this will
          // waste too much space, we can optimize it in the future
          affectRows++;
          deletedKeys.insert(fullKey);
          newEntries[newKey] = newValue;
        }
        else
        {
          // duplicate key, ignore it
          log_info("duplicated entry, newkey:%s", newKey.c_str());
          Task->set_error_dup(-ER_DUP_ENTRY, __FUNCTION__, "duplicate key!");
          mDBConn->retrieve_end(rocksItr);
          return -1;
        }
      }

      // move iterator to the next key
      ret = mDBConn->next_entry(rocksItr, fullKey, compoundValue);
      if (ret < 0)
      {
        log_error("retrieve compoundValue failed, key:%s", prefixKey.c_str());
        Task->set_error_dup(ret, __FUNCTION__, "get next entry failed!");
        mDBConn->retrieve_end(rocksItr);
        return -1;
      }
      else if (ret == 1)
      {
        // no remaining rows in the storage
        break;
      }

      // has remaining compoundValue in rocksdb
    }
  }
  else
  {
    // do not break the unique key constraints, no need to hold the entire row in memory
    // iterate the matched prefix key and find out the real one from start to end
    while (true)
    {
      ret = key_matched(encodePreKey, fullKey);
      if (ret != 0)
      {
        // prefix key not matched, reach to the end
        break;
      }

      // update row
      std::string newKey, newValue;
      ret = update_row(prefixKey, fullKey, compoundValue, Task, newKey, newValue);
      if (ret < 0)
      {
        // ignore the incorrect key and keep going
        log_error("save row failed! key:%s, compoundValue:%s", fullKey.c_str(), compoundValue.c_str());
      }
      else if (ret == 1)
      {
        // key matched, but condition mismatched, keep on retrieve
      }
      else
      {
        affectRows++;
        newEntries[fullKey] = newValue;
      }

      // move iterator to the next key
      ret = mDBConn->next_entry(rocksItr, fullKey, compoundValue);
      if (ret < 0)
      {
        log_error("retrieve compoundValue failed, key:%s", prefixKey.c_str());
        Task->set_error_dup(ret, __FUNCTION__, "get next entry failed!");
        mDBConn->retrieve_end(rocksItr);
        return -1;
      }
      else if (ret == 1)
      {
        // reach to the storage end
        break;
      }

      // has remaining compoundValue in rocksdb
    }
  }

#ifdef DEBUG_INFO
  std::vector<std::string> keys;
  std::stringstream ss;

  ss << "delete keys:[";
  auto itr = deletedKeys.begin();
  while (itr != deletedKeys.end())
  {
    keys.clear();
    key_format::Decode(*itr, mKeyfield_types, keys);

    ss << "(";
    for (size_t idx = 0; idx < keys.size(); idx++)
    {
      ss << keys[idx];
      if (idx != keys.size() - 1)
        ss << ",";
    }
    ss << ") ";

    itr++;
  }
  ss << "]";

  ss << "new keys:[";
  auto itrM = newEntries.begin();
  while (itrM != newEntries.end())
  {
    keys.clear();
    key_format::Decode(itrM->first, mKeyfield_types, keys);

    ss << "(";
    for (size_t idx = 0; idx < keys.size(); idx++)
    {
      ss << keys[idx];
      if (idx != keys.size() - 1)
        ss << ",";
    }
    ss << ") ";

    itrM++;
  }
  ss << "]";

  log_error("%s", ss.str().c_str());
#endif

  // do atomic update
  ret = mDBConn->batch_update(deletedKeys, newEntries, true);
  if (ret != 0)
  {
    log_error("batch update rocksdb failed! key:%s", prefixKey.c_str());
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "update failed!");
    return -1;
  }

  mDBConn->retrieve_end(rocksItr);

  Task->resultInfo.set_affected_rows(affectRows);

#ifdef PRINT_STAT
  mETime = GET_TIMESTAMP();
  insert_stat(OprType::eUpdate, mETime - mSTime);
#endif

  return (0);
}

int RocksdbProcess::whether_update_key(
    const DTCFieldValue *UpdateInfo,
    bool &updateKey,
    bool &updateValue)
{
  int fieldId;
  updateKey = false;
  updateValue = false;
  for (int i = 0; i < UpdateInfo->num_fields(); i++)
  {
    const int fid = UpdateInfo->field_id(i);

    if (mTableDef->is_volatile(fid))
      continue;

    assert(fid < mFieldIndexMapping.size());

    for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
    {
      if (fid == mFieldIndexMapping[idx])
      {
        if (idx < mCompoundKeyFieldNums)
          updateKey = true;
        else
          updateValue = true;

        break;
      }
    }
  }

  return 0;
}

int RocksdbProcess::process_delete(DTCTask *Task)
{
  int ret, affectRows = 0;

  // prefix key
  std::string prefixKey;
  ret = value2Str(Task->request_key(), 0, prefixKey);
  if (ret != 0)
  {
    log_error("get dtc primary key failed! key");
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get key failed!");
    return -1;
  }

  if (mKeyfield_types[0] == DField::String)
    std::transform(prefixKey.begin(), prefixKey.end(), prefixKey.begin(), ::tolower);

  // encode the key to rocksdb format
  std::string fullKey = std::move(key_format::encode_bytes(prefixKey));
  std::string encodePreKey = fullKey;

  std::string compoundValue;
  RocksDBConn::RocksItr_t rocksItr;
  ret = mDBConn->retrieve_start(fullKey, compoundValue, rocksItr, true);
  if (ret < 0)
  {
    log_error("retrieve rocksdb failed! key:%s", fullKey.c_str());
    Task->set_error_dup(ret, __FUNCTION__, "retrieve failed!");
    mDBConn->retrieve_end(rocksItr);
    return -1;
  }
  else if (ret == 1)
  {
    // not found the key
    Task->resultInfo.set_affected_rows(affectRows);
    mDBConn->retrieve_end(rocksItr);
    log_info("no matched key:%s", prefixKey.c_str());
    return 0;
  }

  // iterate the matched prefix key and find out the real one from start to end
  bool condMatch = true;
  int bitmapLen = 0;
  DTCFieldValue *condition;
  std::set<std::string> deleteKeys;
  while (true)
  {
    // check whether the key is in the range of the prefix of the 'fullKey'
    ret = key_matched(encodePreKey, fullKey);
    if (ret != 0)
    {
      // prefix key not matched, reach to the end
      break;
    }

    // decode the compoundKey and check whether it is matched
    std::string realValue;
    std::vector<std::string> keys;
    std::vector<std::string> values;
    key_format::Decode(fullKey, mKeyfield_types, keys);
    assert(keys.size() > 0);

    ret = prefixKey.compare(keys[0]);
    // if ( ret != 0 ) goto NEXT_ENTRY;
    if (ret != 0)
      break;

    if (keys.size() != mCompoundKeyFieldNums)
    {
      // unmatched row
      log_error("unmatched row, fullKey:%s, keyNum:%lu, definitionFieldNum:%d",
                fullKey.c_str(), keys.size(), mCompoundKeyFieldNums);
      ret = 0;
    }

    // decode key bitmap
    decodeBitmapKeys(compoundValue, keys, bitmapLen);

    realValue = compoundValue.substr(bitmapLen);
    split_values(realValue, values);
    if (values.size() != mExtraValueFieldNums)
    {
      log_info("unmatched row, fullKey:%s, value:%s, keyNum:%lu, valueNum:%lu",
               fullKey.c_str(), compoundValue.c_str(), keys.size(), values.size());
      ret = 0;
    }

    // condition filter
    condition = (DTCFieldValue *)Task->request_condition();
    for (size_t idx = 1; idx < mFieldIndexMapping.size(); idx++)
    {
      int fieldId = mFieldIndexMapping[idx];
      std::string &fieldValue = idx < mCompoundKeyFieldNums ? keys[idx] : values[idx - mCompoundKeyFieldNums];

      // do condition filter
      ret = condition_filter(fieldValue, fieldId, mTableDef->field_type(fieldId), condition);
      if (ret < 0)
      {
        log_error("string[%s] conver to value[%d] error: %d, %m", fieldValue.c_str(), mTableDef->field_type(fieldId), ret);
        condMatch = false;
        break;
      }
      else if (ret == 1)
      {
        // condition is not matched
        condMatch = false;
        break;
      }
    }

    if (condMatch)
    {
#ifdef DEBUG_INFO
      std::stringstream ss;
      ss << "delete key:[";
      for (size_t idx = 0; idx < mCompoundKeyFieldNums; idx++)
      {
        ss << keys[idx];
        if (idx != mCompoundKeyFieldNums - 1)
          ss << ",";
      }
      ss << "]";
      log_error("%s", ss.str().c_str());
#endif
      deleteKeys.insert(fullKey);
      affectRows++;
    }

  NEXT_ENTRY:
    // move iterator to the next key
    ret = mDBConn->next_entry(rocksItr, fullKey, compoundValue);
    if (ret < 0)
    {
      Task->set_error_dup(ret, __FUNCTION__, "get next entry failed!");
      mDBConn->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 1)
    {
      // reach to the end of the storage
      break;
    }
  }

  // delete key from Rocksdb storage, inside the 'retrieve end' for transaction isolation, this delete will be not seen before release this retrieve
  ret = mDBConn->batch_update(deleteKeys, std::map<std::string, std::string>(), true);
  if (ret != 0)
  {
    log_error("batch update rocksdb failed! key:%s", prefixKey.c_str());
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "update failed!");
    return -1;
  }

  mDBConn->retrieve_end(rocksItr);
  Task->resultInfo.set_affected_rows(affectRows);

  return (0);
}

int RocksdbProcess::process_task(DTCTask *Task)
{
  if (Task == NULL)
  {
    log_error("Task is NULL!%s", "");
    return (-1);
  }

  mTableDef = TableDefinitionManager::Instance()->get_cur_table_def();

  switch (Task->request_code())
  {
  case DRequest::Nop:
  case DRequest::Purge:
  case DRequest::Flush:
    return 0;

  case DRequest::Get:
    return process_select(Task);

  case DRequest::Insert:
    return process_insert(Task);

  case DRequest::Update:
    return process_update(Task);

  case DRequest::Delete:
    return process_delete(Task);

  case DRequest::Replace:
    return process_replace(Task);

  case DRequest::ReloadConfig:
    return process_reload_config(Task);

  // master-slave replication
  case DRequest::Replicate:
    return ProcessReplicate(Task);
    // cluster scaleable
    //case DRequest::Migrate:
    //  return ProcessMigrate();

  default:
    Task->set_error(-EC_BAD_COMMAND, __FUNCTION__, "invalid request-code");
    return (-1);
  }
}

//add by frankyang 处理更新过的交易日志
int RocksdbProcess::process_replace(DTCTask *Task)
{
  int ret, affectRows = 0;

  set_title("REPLACE...");

  // primary key in DTC
  std::vector<std::string> keys, values;
  keys.resize(mCompoundKeyFieldNums);
  values.resize(mExtraValueFieldNums);

  std::string strKey;
  value2Str(Task->request_key(), 0, strKey);
  keys[0] = strKey;

  // deal update info
  const DTCFieldValue *updateInfo = Task->request_operation();
  if (updateInfo != NULL)
  {
    for (int idx = 0; idx < updateInfo->num_fields(); idx++)
    {
      const int fid = updateInfo->field_id(idx);

      if (mTableDef->is_volatile(fid))
        continue;

      std::string fieldValue;
      switch (updateInfo->field_operation(idx))
      {
      case DField::Set:
      {
        ret = value2Str(updateInfo->field_value(idx), fid, fieldValue);
        if (ret != 0)
        {
          Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get value failed!");
          log_error("translate value failed");
          return (-1);
        }
        break;
      }

      case DField::Add:
      {
        // add additional value to the defaule value
        const DTCValue *addValue = updateInfo->field_value(idx);
        const DTCValue *defValue = mTableDef->default_value(idx);
        switch (updateInfo->field_type(idx))
        {
        case DField::Signed:
          fieldValue = std::move(std::to_string((long long)(addValue->s64 + defValue->s64)));
          break;

        case DField::Unsigned:
          fieldValue = std::move(std::to_string((unsigned long long)(addValue->u64 + defValue->u64)));
          break;

        case DField::Float:
          fieldValue = std::move(std::to_string(addValue->flt + defValue->flt));
          break;
        default:
          Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "unkonwn field type!");
          log_error("unsupport field data type:%d", updateInfo->field_type(idx));
          return -1;
        }
        break;
      }

      default:
        log_error("unsupport field operation:%d", updateInfo->field_operation(idx));
        break;
      }

      int rocksidx = translate_field_idx(fid);
      assert(rocksidx >= 0 && rocksidx < mCompoundKeyFieldNums + mExtraValueFieldNums);
      rocksidx < mCompoundKeyFieldNums ? keys[rocksidx] = std::move(fieldValue) : values[rocksidx - mCompoundKeyFieldNums] = std::move(fieldValue);
    }
  }

  // deal default value
  uint8_t mask[32];
  FIELD_ZERO(mask);
  if (updateInfo)
    updateInfo->build_field_mask(mask);

  for (int i = 1; i <= mTableDef->num_fields(); i++)
  {
    if (FIELD_ISSET(i, mask) || mTableDef->is_volatile(i))
      continue;

    std::string fieldValue;
    ret = value2Str(mTableDef->default_value(i), i, fieldValue);
    if (ret != 0)
    {
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get value failed!");
      log_error("translate value failed");
      return (-1);
    }

    int rocksidx = translate_field_idx(i);
    assert(rocksidx >= 0 && rocksidx < mCompoundKeyFieldNums + mExtraValueFieldNums);
    rocksidx < mCompoundKeyFieldNums ? keys[rocksidx] = std::move(fieldValue) : values[rocksidx - mCompoundKeyFieldNums] = std::move(fieldValue);
  }

  // convert string type 'key' into lower case and build case letter bitmap
  std::string keyBitmaps;
  encode_bitmap_keys(keys, keyBitmaps);

  std::string rocksKey, rocksValue;
  rocksKey = std::move(key_format::Encode(keys, mKeyfield_types));

  shrink_value(values, rocksValue);

  // add key bitmaps to the rocksdb value field
  keyBitmaps.append(rocksValue);

  ret = mDBConn->replace_entry(rocksKey, keyBitmaps, true);
  if (ret == 0)
  {
    Task->resultInfo.set_affected_rows(1);
    return 0;
  }

  log_error("replace key failed, key:%s, code:%d", rocksKey.c_str(), ret);
  Task->set_error_dup(ret, __FUNCTION__, "replace key failed!");
  return -1;
}

int RocksdbProcess::ProcessReplicate(DTCTask *Task)
{
  log_info("come into rocksdb replicate!");

  int ret, totalRows = 0;

  // create resultWriter
  ret = Task->prepare_result_no_limit();
  if (ret != 0)
  {
    Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "task prepare-result error");
    log_error("task prepare-result error: %d, %m", ret);
    return (-2);
  }

  // key for replication start:
  std::string startKey, prevPrimaryKey, compoundKey, compoundValue;
  RocksDBConn::RocksItr_t rocksItr;

  // whether start a newly replication or not
  uint32_t start = Task->requestInfo.limit_start();
  uint32_t count = Task->requestInfo.limit_count();
  // if full replicate start from 0 and the start key is empty, means it's a newly replication
  bool isBeginRepl = (start == 0);
  if (likely(!isBeginRepl))
  {
    // replicate with user given key
    ret = value2Str(Task->request_key(), 0, startKey);
    if (ret != 0 || startKey.empty())
    {
      log_error("replicate key can not be empty!");
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get replicate key failed!");
      return -1;
    }

    // encode the key to rocksdb format
    compoundKey = std::move(key_format::encode_bytes(startKey));
    if (compoundKey.empty())
    {
      log_error("encode primary key failed! key:%s", startKey.c_str());
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "encode replicate key failed!");
      return -1;
    }

    prevPrimaryKey = compoundKey;

    ret = mDBConn->search_lower_bound(compoundKey, compoundValue, rocksItr);
  }
  else
  {
#if 0
    // get the last key for replication finished tag
    ret = mDBConn->get_last_entry(compoundKey, compoundValue, rocksItr);
    if ( ret < 0 )
    {
      // replicate error, let the user decide to try again
      log_error("get last key failed!");
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "get last replicate key failed!");
      return -1;
    }
    else if ( ret == 1 )
    {
      // empty database
      Task->set_total_rows(0);
      return 0;
    }

    // set the finished key of replicating into meta data column family
    // delete the odd migrate-end-key from that may insert by the previous slave
    mReplEndKey = compoundKey;
    // use replace api to instead of insert, in case there has multi slave replicator, all
    // of them should always replicate to the latest one key
    ret = mDBConn->replace_entry(gReplicatePrefixKey, mReplEndKey, true, RocksDBConn::COLUMN_META_DATA);
    if ( ret != 0 )
    {
      log_error("save replicating-finished-key failed! key:%s", mReplEndKey.c_str());
      Task->set_error(-EC_ERROR_BASE, __FUNCTION__, "save replicating finished key failed!");
      return -1;
    }
#endif

    // do forward retrieving for reducing duplicate replication
    startKey = "";
    prevPrimaryKey = "";

    ret = mDBConn->get_first_entry(compoundKey, compoundValue, rocksItr);
  }

  if (ret < 0)
  {
    log_error("query rocksdb failed! isBeginRepl:%d, key:%s", isBeginRepl, startKey.c_str());
    Task->set_error_dup(ret, __FUNCTION__, "do replication failed!");
    mDBConn->retrieve_end(rocksItr);
    return -1;
  }
  else if (ret == 1)
  {
    // not found the key
    Task->set_total_rows(0);
    log_info("do full replication finished! %s", startKey.c_str());
    mDBConn->retrieve_end(rocksItr);
    return 0;
  }

  // iterate the matched prefix key and find out the real one from start to end
  int replLen = 0;
  while (true)
  {
    // 1.skip the user given key
    // 2.the same prefix key only get once
    if (key_matched(prevPrimaryKey, compoundKey) == 0)
    {
      // ignore the matched key that has been migrated in the previous call
    }
    else
    {
      // save row
      ret = saveRow(compoundKey, compoundValue, false, totalRows, Task);
      if (ret < 0)
      {
        // ignore the incorrect key and keep going
        log_error("save row failed! key:%s, value:%s", compoundKey.c_str(), compoundValue.c_str());
      }

      key_format::get_format_key(compoundKey, mKeyfield_types[0], prevPrimaryKey);
    }

    // move iterator to the next key
    ret = mDBConn->next_entry(rocksItr, compoundKey, compoundValue);
    if (ret < 0)
    {
      log_error("iterate rocksdb failed! key:%s", startKey.c_str());
      Task->set_error(ret, __FUNCTION__, "iterate rocksdb failed!");
      mDBConn->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 1)
    {
      // reach to the end
      break;
    }

    // has remaining value in rocksdb
    if (totalRows >= count)
      break;
    // replLen += (compoundKey,length() + compoundValue.length());
    // if ( relpLen >= MAX_REPLICATE_LEN )
    // {
    //   // avoid network congestion
    //   break;
    // }
  }

  Task->set_total_rows(totalRows);
  mDBConn->retrieve_end(rocksItr);

  return (0);
}

int RocksdbProcess::process_direct_query(
    DirectRequestContext *reqCxt,
    DirectResponseContext *respCxt)
{
  log_info("come into process direct query!");

#ifdef PRINT_STAT
  mSTime = GET_TIMESTAMP();
#endif

  int ret;

  std::vector<QueryCond> primaryKeyConds;
  ret = analyse_primary_key_conds(reqCxt, primaryKeyConds);

#if 0
  std::vector<QueryCond>::iterator iter = primaryKeyConds.begin();
  for (; iter != primaryKeyConds.end(); ++iter){
    std::vector<int> fieldTypes;
    fieldTypes.push_back(DField::Signed);
    std::vector<std::string> fieldValues;

    int ipos = iter->sCondValue.find_last_of("#");
    std::string stemp = iter->sCondValue.substr(ipos + 1);
    key_format::Decode(stemp , fieldTypes , fieldValues);
    log_error("field index:%d , condopr:%d , condvalue:%s" ,
        iter->sFieldIndex , 
        iter->sCondOpr , 
        fieldValues[0].c_str());
  }
#endif

  if (ret != 0)
  {
    log_error("query condition incorrect in query context!");
    respCxt->sRowNums = -EC_ERROR_BASE;
    return -1;
  }

  // prefix key
  std::string prefixKey = primaryKeyConds[0].sCondValue;
  if (prefixKey.empty())
  {
    log_error("dtc primary key can not be empty!");
    respCxt->sRowNums = -EC_ERROR_BASE;
    return -1;
  }

  if (mKeyfield_types[0] == DField::String)
    std::transform(prefixKey.begin(), prefixKey.end(), prefixKey.begin(), ::tolower);

  // encode the key to rocksdb format
  std::string fullKey = std::move(key_format::encode_bytes(prefixKey));
  std::string encodePreKey = fullKey;

  int totalRows = 0;
  std::string value;
  RocksDBConn::RocksItr_t rocksItr;

  bool forwardDirection = (primaryKeyConds[0].sCondOpr == (uint8_t)CondOpr::eEQ || 
      primaryKeyConds[0].sCondOpr == (uint8_t)CondOpr::eGT || 
      primaryKeyConds[0].sCondOpr == (uint8_t)CondOpr::eGE);
  bool backwardEqual = primaryKeyConds[0].sCondOpr == (uint8_t)CondOpr::eLE;
  log_debug("forwardDirection:%d , backwardEqual:%d", (int)forwardDirection , (int)backwardEqual);
  if (backwardEqual)
  {
    // if the query condtion is < || <=, use seek_for_prev to seek in the total_order_seek mode
    // will not use the prefix seek features, eg:
    //  1. we have the flowing union key in the rocks (101,xx), (102,xx), (103,xx)
    //  2. we use total_order_seek features for ranging query with primary key '102', and this
    //     lead the rocksdb doesn't use prefix_extractor to match the prefix key, so it use the
    //     entire key for comparing, and seek_for_prev will stop in the last key that <= the
    //     target key, so the iterator point to the key '(101, xx)', that's not what we want,
    //     wo need it point to the '(102,xx)'

    // do primary key equal query first in this section
    ret = mDBConn->retrieve_start(fullKey, value, rocksItr, true);
    if (ret < 0)
    {
      log_error("query rocksdb failed! key:%s, ret:%d", prefixKey.c_str(), ret);
      respCxt->sRowNums = -EC_ERROR_BASE;
      mDBConn->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 0)
    {
      while (true)
      {
        ret = key_matched(encodePreKey, fullKey);
        if (ret != 0)
        {
          // prefix key not matched, reach to the end
          break;
        }

        // save row
        ret = save_direct_row(prefixKey, fullKey, value, reqCxt, respCxt, totalRows);
        if (ret < 0)
        {
          // ignore the incorrect key and keep going
          log_error("save row failed! key:%s, value:%s", fullKey.c_str(), value.c_str());
        }
        else if (ret == 1)
          break;

        // move iterator to the next key
        ret = mDBConn->next_entry(rocksItr, fullKey, value);
        if (ret < 0)
        {
          log_error("iterate rocksdb failed! key:%s", prefixKey.c_str());
          respCxt->sRowNums = -EC_ERROR_BASE;
          mDBConn->retrieve_end(rocksItr);
          return -1;
        }
        else if (ret == 1)
        {
          // reach to the storage end
          break;
        }

        // has remaining value in rocksdb
      }
    }
  }

  // range query in the following section
  ret = mDBConn->retrieve_start(fullKey, value, rocksItr, false, forwardDirection);
  if (ret < 0)
  {
    log_error("query rocksdb failed! key:%s, ret:%d", prefixKey.c_str(), ret);
    respCxt->sRowNums = -EC_ERROR_BASE;
    mDBConn->retrieve_end(rocksItr);
    return -1;
  }
  else if (ret == 1)
  {
    // not found the key
    log_info("no matched key:%s", prefixKey.c_str());
    respCxt->sRowNums = 0;
    mDBConn->retrieve_end(rocksItr);
    return 0;
  }

  // iterate the matched prefix key and find out the real one from start to end
  while (true)
  {
    ret = range_key_matched(fullKey, primaryKeyConds);

#if 0
    std::vector<std::string> rocksValues;
    std::vector<int> fieldTypes;
    fieldTypes.push_back(DField::String);
    fieldTypes.push_back(DField::String);
    fieldTypes.push_back(DField::Signed);
    fieldTypes.push_back(DField::Signed);
    fieldTypes.push_back(DField::Signed);

    key_format::Decode(fullKey, fieldTypes, rocksValues);

    for (int i = 0; i < rocksValues[0].length(); i++){
      log_error("No:%d， is %d \n" , i , (int)rocksValues[0][i]);
    }
    
    int ipos = rocksValues[0].find_last_of("#");
    std::string stemp = rocksValues[0].substr(ipos + 1);
    std::vector<std::string> rocksValues001;

    std::vector<int> fieldTypes001;
    fieldTypes001.push_back(DField::Signed);

    key_format::Decode(stemp , fieldTypes001 , rocksValues001);
    log_error("primary value:%s",  rocksValues001[0].c_str());

    for (size_t i = 0; i < rocksValues.size(); i++)
    {
      
      log_error("value:%s", rocksValues[i].c_str() );
    }
#endif

    if (ret == -1)
    {
      // prefix key not matched, reach to the end
      break;
    }

    if (ret == 0)
    {
      // save row
      ret = save_direct_row(prefixKey, fullKey, value, reqCxt, respCxt, totalRows);
      if (ret < 0)
      {
        // ignore the incorrect key and keep going
        log_error("save row failed! key:%s, value:%s", fullKey.c_str(), value.c_str());
      }
      else if (ret == 1)
        break;
    }

    // move iterator to the next key
    if (forwardDirection)
    {
      ret = mDBConn->next_entry(rocksItr, fullKey, value);
    }
    else
    {
      ret = mDBConn->prev_entry(rocksItr, fullKey, value);
    }
    if (ret < 0)
    {
      log_error("iterate rocksdb failed! key:%s", prefixKey.c_str());
      respCxt->sRowNums = 0;
      mDBConn->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 1)
    {
      // reach to the storage end
      break;
    }

    // has remaining value in rocksdb
  }

  // generate response rows in order by container
  if (mOrderByUnit)
  {
    OrderByUnitElement element;
    int start = ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart >= 0 && ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStep > 0 ? 
          ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sLimitCond.sLimitStart : 0;
    while (true)
    {
      ret = mOrderByUnit->get_row(element);
      if (0 == ret)
      {
        delete mOrderByUnit;
        mOrderByUnit = NULL;
        break;
      }

      if (start != 0)
      {
        start--;
        continue;
      }

      build_direct_row(element.mRocksKeys, element.mRocksValues, respCxt);
    }
  }

  respCxt->sRowNums = ((RangeQueryRows_t*)respCxt->sDirectRespValue.uRangeQueryRows)->sRowValues.size();
  mDBConn->retrieve_end(rocksItr);

#ifdef PRINT_STAT
  mETime = GET_TIMESTAMP();
  insert_stat(OprType::eDirectQuery, mETime - mSTime);
#endif

  return (0);
}

int RocksdbProcess::TriggerReplication(
    const std::string& masterIp,
    int masterPort)
{
  return mReplUnit->startSlaveReplication(masterIp, masterPort);
}

int RocksdbProcess::QueryReplicationState()
{
  return mReplUnit->getReplicationState();
}

int RocksdbProcess::encode_dtc_key(
    const std::string &oKey,
    std::string &codedKey)
{
  int keyLen = oKey.length();
  static const int maxLen = 10240;
  assert(sizeof(int) + keyLen <= maxLen);
  static thread_local char keyBuff[maxLen];

  char *pos = keyBuff;
  *(int *)pos = keyLen;
  pos += sizeof(int);
  memcpy((void *)pos, (void *)oKey.data(), keyLen);

  codedKey.assign(keyBuff, keyLen + sizeof(int));

  return 0;
}

int RocksdbProcess::decode_keys(
    const std::string &compoundKey,
    std::vector<std::string> &keys)
{
  int ret;
  std::string keyField;
  char *head = const_cast<char *>(compoundKey.data());

  // decode dtckey first
  int keyLen = *(int *)head;
  head += sizeof(int);
  keyField.assign(head, keyLen);
  head += keyLen;
  keys.push_back(std::move(keyField));

  // decode other key fields
  for (int idx = 1; idx < mCompoundKeyFieldNums; idx++)
  {
    ret = get_value_by_id(head, mFieldIndexMapping[idx], keyField);
    assert(ret == 0);
    keys.push_back(std::move(keyField));
  }

  return 0;
}

int RocksdbProcess::encode_rocks_key(
    const std::vector<std::string> &keys,
    std::string &rocksKey)
{
  assert(keys.size() == mCompoundKeyFieldNums);

  // evaluate space
  static int align = 1 << 12;
  int valueLen = 0, fid, fsize;
  int totalLen = align;
  char *valueBuff = (char *)malloc(totalLen);

  // encode key first
  int keyLen = keys[0].length();
  *(int *)valueBuff = keyLen;
  valueLen += sizeof(int);
  memcpy((void *)valueBuff, (void *)keys[0].data(), keyLen);
  valueLen += keyLen;

  for (size_t idx = 1; idx < mCompoundKeyFieldNums; idx++)
  {
    fid = mFieldIndexMapping[idx];
    switch (mTableDef->field_type(fid))
    {
    case DField::Signed:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(int32_t))
      {
        if (valueLen + sizeof(int64_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(int64_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int64_t *)(valueBuff + valueLen) = strtoll(keys[idx].c_str(), NULL, 10);
        valueLen += sizeof(int64_t);
      }
      else
      {
        if (valueLen + sizeof(int32_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(int32_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int32_t *)(valueBuff + valueLen) = strtol(keys[idx].c_str(), NULL, 10);
        valueLen += sizeof(int32_t);
      }
      break;
    }

    case DField::Unsigned:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(uint32_t))
      {
        if (valueLen + sizeof(uint64_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(uint64_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(uint64_t *)(valueBuff + valueLen) = strtoull(keys[idx].c_str(), NULL, 10);
        valueLen += sizeof(uint64_t);
      }
      else
      {
        if (valueLen + sizeof(uint32_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(uint32_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(uint32_t *)(valueBuff + valueLen) = strtoul(keys[idx].c_str(), NULL, 10);
        valueLen += sizeof(uint32_t);
      }
      break;
    }

    case DField::Float:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(float))
      {
        if (valueLen + sizeof(double) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(double) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(double *)(valueBuff + valueLen) = strtod(keys[idx].c_str(), NULL);
        valueLen += sizeof(double);
      }
      else
      {
        if (valueLen + sizeof(float) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(float) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(float *)(valueBuff + valueLen) = strtof(keys[idx].c_str(), NULL);
        valueLen += sizeof(float);
      }
      break;
    }

    case DField::String:
    case DField::Binary:
    {
      int len = keys[idx].length();
      fsize = len + sizeof(int);
      {
        if (valueLen + fsize > totalLen)
        {
          // expand buff
          totalLen = (valueLen + fsize + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int *)(valueBuff + valueLen) = len;
        valueLen += sizeof(int);
        if (len > 0)
          memcpy((void *)(valueBuff + valueLen), (void *)keys[idx].data(), len);
        valueLen += len;
      }
      break;
    }

    default:
      log_error("unexpected field type! type:%d", mTableDef->field_type(fid));
      return -1;
    };
  }

  rocksKey.assign(valueBuff, valueLen);
  free(valueBuff);

  return 0;
}

// 1. convert string type key into lower case
// 2. create bit map for those been converted keys
void RocksdbProcess::encode_bitmap_keys(
    std::vector<std::string> &keys,
    std::string &keyBitmaps)
{
  if (mNoBitmapKey)
    return;

  std::vector<char> keyLocationBitmap, keyCaseBitmap;
  int8_t localBits = 0;
  bool hasBeenConverted = false;

  for (size_t idx = 0; idx < keys.size(); idx++)
  {
    switch (mKeyfield_types[idx])
    {
    default:
      hasBeenConverted = false;
      break;
    case DField::String:
    {
      // maybe need convert
      std::vector<char> currentKeyBitmap;
      hasBeenConverted = convert_to_lower(keys[idx], currentKeyBitmap);
      if (hasBeenConverted)
      {
        keyCaseBitmap.insert(keyCaseBitmap.end(),
                             std::make_move_iterator(currentKeyBitmap.begin()),
                             std::make_move_iterator(currentKeyBitmap.end()));
      }
    }
    }

    // record key location bitmap
    if (hasBeenConverted)
    {
      int shift = BITS_OF_BYTE - 1 - 1 - idx % (BITS_OF_BYTE - 1);
      localBits = (localBits >> shift | 1U) << shift;
    }

    // the last boundary bit in this section and has remaining keys, need to set the
    // head bit for indicading
    if ((idx + 1) % (BITS_OF_BYTE - 1) == 0 || idx == keys.size() - 1)
    {
      if (idx != keys.size() - 1)
        localBits |= 128U;
      keyLocationBitmap.push_back((char)localBits);
      localBits = 0;
    }
  }

  // shrink bits to buffer
  keyBitmaps.append(
                std::string(keyLocationBitmap.begin(), keyLocationBitmap.end()))
      .append(
          std::string(keyCaseBitmap.begin(), keyCaseBitmap.end()));
}

void RocksdbProcess::decodeBitmapKeys(
    const std::string &rocksValue,
    std::vector<std::string> &keys,
    int &bitmapLen)
{
  bitmapLen = 0;

  if (mNoBitmapKey)
    return;

  int8_t sectionBits;
  std::vector<char> keyLocationBitmap;

  // decode key location bitmap
  while (true)
  {
    sectionBits = rocksValue[bitmapLen];
    keyLocationBitmap.push_back(sectionBits);
    bitmapLen++;

    if ((sectionBits & 0x80) == 0)
      break;
  }

  int shift = 0;
  for (size_t idx = 0; idx < keys.size(); idx++)
  {
    sectionBits = keyLocationBitmap[idx / (BITS_OF_BYTE - 1)];
    shift = BITS_OF_BYTE - 1 - 1 - idx % (BITS_OF_BYTE - 1);

    switch (mKeyfield_types[idx])
    {
    default:
      assert((sectionBits >> shift & 1U) == 0);
      break;
    case DField::String:
    {
      if ((sectionBits >> shift & 1U) == 0)
      {
        // no need to do convert
      }
      else
      {
        // recovery the origin key
        recover_to_upper(rocksValue, bitmapLen, keys[idx]);
      }
    }
    }
  }
}

int RocksdbProcess::get_key_bitmap_len(const std::string &rocksValue)
{
  int bitmapLen = 0;

  if (mNoBitmapKey)
    return bitmapLen;

  int8_t sectionBits;
  std::deque<char> keyLocationBitmap;

  // decode key location bitmap
  while (true)
  {
    sectionBits = rocksValue[bitmapLen];
    keyLocationBitmap.push_back(sectionBits);
    bitmapLen++;

    if ((sectionBits & 0x80) == 0)
      break;
  }

  int shift = 0;
  while (keyLocationBitmap.size() != 0)
  {
    sectionBits = keyLocationBitmap.front();
    for (int8_t idx = 1; idx < BITS_OF_BYTE; idx++)
    {
      shift = BITS_OF_BYTE - 1 - idx;

      if ((sectionBits >> shift & 1U) == 1)
      {
        // collect the key bitmap len
        int8_t keyBits;
        while (true)
        {
          keyBits = (int8_t)rocksValue[bitmapLen++];
          if ((keyBits & 0x80) == 0)
            break;
        }
      }
    }

    keyLocationBitmap.pop_front();
  }

  return bitmapLen;
}

bool RocksdbProcess::convert_to_lower(
    std::string &key,
    std::vector<char> &keyCaseBitmap)
{
  bool hasConverted = false;
  int8_t caseBits = 0;
  char lowerBase = 'a' - 'A';
  for (size_t idx = 0; idx < key.length(); idx++)
  {
    char &cv = key.at(idx);
    if (cv >= 'A' && cv <= 'Z')
    {
      cv += lowerBase;

      int shift = BITS_OF_BYTE - 1 - 1 - idx % (BITS_OF_BYTE - 1);
      caseBits = (caseBits >> shift | 1U) << shift;

      hasConverted = true;
    }

    if ((idx + 1) % (BITS_OF_BYTE - 1) == 0 || idx == key.length() - 1)
    {
      if (idx != key.length() - 1)
        caseBits |= 128U;
      keyCaseBitmap.push_back((char)caseBits);
      caseBits = 0;
    }
  }

  return hasConverted;
}

void RocksdbProcess::recover_to_upper(
    const std::string &rocksValue,
    int &bitmapLen,
    std::string &key)
{
  int shift;
  int kIdx = 0;
  bool hasRemaining = true;
  char upperBase = 'a' - 'A';
  int8_t sectionBits;

  do
  {
    sectionBits = rocksValue[bitmapLen];

    shift = BITS_OF_BYTE - 1 - 1 - kIdx % (BITS_OF_BYTE - 1);
    if (sectionBits >> shift & 1U)
    {
      // convert to upper mode
      char &cc = key[kIdx];
      assert(cc >= 'a' && cc <= 'z');
      cc -= upperBase;
    }

    kIdx++;
    if (kIdx % (BITS_OF_BYTE - 1) == 0)
    {
      bitmapLen++;
      hasRemaining = (sectionBits & 0x80) != 0;
    }
  } while (hasRemaining);
}

int RocksdbProcess::shrink_value(
    const std::vector<std::string> &values,
    std::string &rocksValue)
{
  assert(values.size() == mExtraValueFieldNums);

  // evaluate space
  static int align = 1 << 12;
  int valueLen = 0, fid, fsize;
  int totalLen = align;
  char *valueBuff = (char *)malloc(totalLen);
  for (size_t idx = 0; idx < mExtraValueFieldNums; idx++)
  {
    fid = mFieldIndexMapping[mCompoundKeyFieldNums + idx];
    switch (mTableDef->field_type(fid))
    {
    case DField::Signed:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(int32_t))
      {
        if (valueLen + sizeof(int64_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(int64_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int64_t *)(valueBuff + valueLen) = strtoll(values[idx].c_str(), NULL, 10);
        valueLen += sizeof(int64_t);
      }
      else
      {
        if (valueLen + sizeof(int32_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(int32_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int32_t *)(valueBuff + valueLen) = strtol(values[idx].c_str(), NULL, 10);
        valueLen += sizeof(int32_t);
      }
      break;
    }

    case DField::Unsigned:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(uint32_t))
      {
        if (valueLen + sizeof(uint64_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(uint64_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(uint64_t *)(valueBuff + valueLen) = strtoull(values[idx].c_str(), NULL, 10);
        valueLen += sizeof(uint64_t);
      }
      else
      {
        if (valueLen + sizeof(uint32_t) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(uint32_t) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(uint32_t *)(valueBuff + valueLen) = strtoul(values[idx].c_str(), NULL, 10);
        valueLen += sizeof(uint32_t);
      }
      break;
    }

    case DField::Float:
    {
      fsize = mTableDef->field_size(fid);
      if (fsize > sizeof(float))
      {
        if (valueLen + sizeof(double) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(double) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(double *)(valueBuff + valueLen) = strtod(values[idx].c_str(), NULL);
        valueLen += sizeof(double);
      }
      else
      {
        if (valueLen + sizeof(float) > totalLen)
        {
          // expand buff
          totalLen = (valueLen + sizeof(float) + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(float *)(valueBuff + valueLen) = strtof(values[idx].c_str(), NULL);
        valueLen += sizeof(float);
      }
      break;
    }

    case DField::String:
    case DField::Binary:
    {
      int len = values[idx].length();
      fsize = len + sizeof(int);
      {
        if (valueLen + fsize > totalLen)
        {
          // expand buff
          totalLen = (valueLen + fsize + align - 1) & -align;
          valueBuff = expand_buff(totalLen, valueBuff);
          if (!valueBuff)
            return -1;
        }

        *(int *)(valueBuff + valueLen) = len;
        valueLen += sizeof(int);
        if (len > 0)
          memcpy((void *)(valueBuff + valueLen), (void *)values[idx].data(), len);
        valueLen += len;
      }
      break;
    }

    default:
      log_error("unexpected field type! type:%d", mTableDef->field_type(fid));
      return -1;
    };
  }

  rocksValue.assign(valueBuff, valueLen);
  free(valueBuff);

  return 0;
}

int RocksdbProcess::split_values(
    const std::string &compoundValue,
    std::vector<std::string> &values)
{
  int ret;
  std::string value;
  char *head = const_cast<char *>(compoundValue.data());
  for (int idx = 0; idx < mExtraValueFieldNums; idx++)
  {
    ret = get_value_by_id(head, mFieldIndexMapping[mCompoundKeyFieldNums + idx], value);
    assert(ret == 0);
    values.push_back(std::move(value));
  }

  return 0;
}

// translate dtcfid to rocksfid
int RocksdbProcess::translate_field_idx(int dtcfid)
{
  for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
  {
    if (mFieldIndexMapping[idx] == dtcfid)
      return idx;
  }

  return -1;
}

int RocksdbProcess::get_value_by_id(
    char *&valueHead,
    int fieldId,
    std::string &fieldValue)
{
  assert(valueHead);

  // evaluate space
  int fsize;
  int fieldType = mTableDef->field_type(fieldId);
  switch (fieldType)
  {
  case DField::Signed:
  {
    fsize = mTableDef->field_size(fieldId);
    if (fsize > sizeof(int32_t))
    {
      int64_t value = *(int64_t *)(valueHead);
      valueHead += sizeof(int64_t);
      fieldValue = std::move(std::to_string(value));
    }
    else
    {
      int32_t value = *(int32_t *)(valueHead);
      valueHead += sizeof(int32_t);
      fieldValue = std::move(std::to_string(value));
    }
    break;
  }

  case DField::Unsigned:
  {
    fsize = mTableDef->field_size(fieldId);
    if (fsize > sizeof(uint32_t))
    {
      uint64_t value = *(uint64_t *)(valueHead);
      valueHead += sizeof(uint64_t);
      fieldValue = std::move(std::to_string(value));
    }
    else
    {
      uint32_t value = *(uint32_t *)(valueHead);
      valueHead += sizeof(uint32_t);
      fieldValue = std::move(std::to_string(value));
    }
    break;
  }

  case DField::Float:
  {
    fsize = mTableDef->field_size(fieldId);
    if (fsize <= sizeof(float))
    {
      float value = *(float *)(valueHead);
      valueHead += sizeof(float);
      fieldValue = std::move(std::to_string(value));
    }
    else
    {
      double value = *(double *)(valueHead);
      valueHead += sizeof(double);
      fieldValue = std::move(std::to_string(value));
    }
    break;
  }

  case DField::String:
  case DField::Binary:
  {
    int len;
    {
      len = *(int *)(valueHead);
      valueHead += sizeof(int);

      fieldValue = std::move(std::string(valueHead, len));
      valueHead += len;
    }
    break;
  }

  default:
    log_error("unexpected field type! type:%d", fieldType);
    return -1;
  };

  return 0;
}

char *RocksdbProcess::expand_buff(int len, char *oldPtr)
{
  char *newPtr = (char *)realloc((void *)oldPtr, len);
  if (!newPtr)
  {
    log_error("realloc memory failed!");
    free(oldPtr);
  }

  return newPtr;
}

// check two rocksdb key whether equal or not
int RocksdbProcess::rocks_key_matched(const std::string &rocksKey1, const std::string &rocksKey2)
{
  return rocksKey1.compare(rocksKey2);
}

// check whether the key in the query conditon range matched or not
// 1 : in the range but not matched
// 0: key matched
// -1: in the out of the range
int RocksdbProcess::range_key_matched(
    const std::string &rocksKey,
    const std::vector<QueryCond> &keyConds)
{
  std::string primaryKey;
  int fieldType = mKeyfield_types[0];
  key_format::decode_primary_key(rocksKey, fieldType, primaryKey);

  int ret;
  for (size_t idx = 0; idx < keyConds.size(); idx++)
  {
    ret = condition_filter(primaryKey, keyConds[idx].sCondValue, fieldType, keyConds[idx].sCondOpr);
    if (ret != 0)
    {
      // check boundary value
      switch (keyConds[idx].sCondOpr)
      {
      /* enum {
           EQ = 0,
           NE = 1,
           LT = 2,
           LE = 3,
           GT = 4,
           GE = 5,
           }; */
      case 0:
      case 1: // not support now
      case 3:
      case 5:
        return -1;
      case 2:
      case 4:
        return primaryKey.compare(keyConds[idx].sCondValue) == 0 ? 1 : -1;
      default:
        log_error("unsupport condition:%d", keyConds[idx].sCondOpr);
      }
    }
  }

  return 0;
}

int RocksdbProcess::analyse_primary_key_conds(
    DirectRequestContext *reqCxt,
    std::vector<QueryCond> &primaryKeyConds)
{
  std::vector<QueryCond>& queryConds = ((RangeQuery_t*)reqCxt->sPacketValue.uRangeQuery)->sFieldConds;
  auto itr = queryConds.begin();
  while (itr != queryConds.end())
  {
    if (itr->sFieldIndex == 0)
    {
      switch ((CondOpr)itr->sCondOpr)
      {
      case CondOpr::eEQ:
      case CondOpr::eLT:
      case CondOpr::eLE:
      case CondOpr::eGT:
      case CondOpr::eGE:
        break;
      case CondOpr::eNE:
      default:
        log_error("unsupport query expression now! condExpr:%d", itr->sCondOpr);
        return -1;
      }
      primaryKeyConds.push_back(*itr);
      itr = queryConds.erase(itr);
    }
    else
    {
      itr++;
    }
  }

  if (primaryKeyConds.size() <= 0)
  {
    log_error("no explicit primary key in query context!");
    return -1;
  }

  return 0;
}

void RocksdbProcess::init_title(int group, int role)
{
  titlePrefixSize = snprintf(name, sizeof(name), "helper%d%c", group, MACHINEROLESTRING[role]);
  memcpy(title, name, titlePrefixSize);
  title[titlePrefixSize++] = ':';
  title[titlePrefixSize++] = ' ';
  title[titlePrefixSize] = '\0';
  title[sizeof(title) - 1] = '\0';
}

void RocksdbProcess::set_title(const char *status)
{
  strncpy(title + titlePrefixSize, status, sizeof(title) - 1 - titlePrefixSize);
  set_proc_title(title);
}

int RocksdbProcess::process_reload_config(DTCTask *Task)
{
  const char *keyStr = gConfig->get_str_val("cache", "CacheShmKey");
  int cacheKey = 0;
  if (keyStr == NULL)
  {
    cacheKey = 0;
    log_notice("CacheShmKey not set!");
    return -1;
  }
  else if (!strcasecmp(keyStr, "none"))
  {
    log_crit("CacheShmKey set to NONE, Cache disabled");
    return -1;
  }
  else if (isdigit(keyStr[0]))
  {
    cacheKey = strtol(keyStr, NULL, 0);
  }
  else
  {
    log_crit("Invalid CacheShmKey value \"%s\"", keyStr);
    return -1;
  }
  CacheInfo stInfo;
  DTCBufferPool bufPool;
  memset(&stInfo, 0, sizeof(stInfo));
  stInfo.ipcMemKey = cacheKey;
  stInfo.keySize = TableDefinitionManager::Instance()->get_cur_table_def()->key_format();
  stInfo.readOnly = 1;

  if (bufPool.cache_open(&stInfo))
  {
    log_error("%s", bufPool.Error());
    Task->set_error(-EC_RELOAD_CONFIG_FAILED, __FUNCTION__, "open cache error!");
    return -1;
  }

  bufPool.reload_table();
  log_error("cmd notify work helper reload table, tableIdx : [%d], pid : [%d]", bufPool.shm_table_idx(), getpid());
  return 0;
}

void RocksdbProcess::insert_stat(
    RocksdbProcess::OprType oprType,
    int64_t timeElapse)
{
  assert(oprType >= OprType::eInsert && oprType < OprType::eDelete);
  int opr = (int)oprType;

  if (timeElapse < 1000)
    mOprTimeCost[opr][(int)TimeZone::eStatLevel0]++;
  else if (timeElapse < 2000)
    mOprTimeCost[opr][(int)TimeZone::eStatLevel1]++;
  else if (timeElapse < 3000)
    mOprTimeCost[opr][(int)TimeZone::eStatLevel2]++;
  else if (timeElapse < 4000)
    mOprTimeCost[opr][(int)TimeZone::eStatLevel3]++;
  else if (timeElapse < 5000)
    mOprTimeCost[opr][(int)TimeZone::eStatLevel4]++;
  else
    mOprTimeCost[opr][(int)TimeZone::eStatLevel5]++;

  mTotalOpr++;

  if (mTotalOpr % 10000 == 0)
    print_stat_info();

  return;
}

void RocksdbProcess::print_stat_info()
{
  int totalNum;

  std::stringstream ss;
  ss << "time cost per opr:\n";
  ss << "totalOpr:" << mTotalOpr << "\n";
  for (unsigned char idx0 = 0; idx0 <= (unsigned char)OprType::eQuery; idx0++)
  {
    switch ((OprType)idx0)
    {
    case OprType::eInsert:
    {
      totalNum = 0;

      ss << "Insert:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    case OprType::eUpdate:
    {
      totalNum = 0;

      ss << "Update:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    case OprType::eDirectQuery:
    {
      totalNum = 0;

      ss << "DirectQuery:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    case OprType::eQuery:
    {
      totalNum = 0;

      ss << "Query:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    case OprType::eReplace:
    {
      totalNum = 0;

      ss << "Replace:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    case OprType::eDelete:
    {
      totalNum = 0;

      ss << "Delete:[";
      for (unsigned char idx1 = 0; idx1 < (unsigned char)TimeZone::eStatMax; idx1++)
      {
        ss << mOprTimeCost[idx0][idx1];
        if (idx1 != (unsigned char)TimeZone::eStatMax - 1)
          ss << ", ";
        totalNum += mOprTimeCost[idx0][idx1];
      }
      ss << "] total:" << totalNum << "\n";
      break;
    }
    }
  }

  log_error("%s", ss.str().c_str());

  return;
}
// use for debuging
int RocksdbProcess::decodeInternalKV(
    const std::string encodeKey,
    const std::string encodeVal,
    std::string& decodeKeys,
    std::string& decodeVals)
{
  int ret;
  // decode the compoundKey and check whether it is matched
  std::vector<std::string> keys;
  key_format::Decode(encodeKey, mKeyfield_types, keys);
  if ( keys.size() != mCompoundKeyFieldNums )
  {
    log_error("unmatched row, fullKey:%s, keyNum:%lu, definitionFieldNum:%d", 
        encodeKey.c_str(), keys.size(), mCompoundKeyFieldNums);
    return -1;
  }
  
  // decode key bitmap
  int bitmapLen = 0;
  decodeBitmapKeys(encodeVal, keys, bitmapLen);

  std::string fieldValue;
  char *valueHead = const_cast<char*>(encodeVal.data()) + bitmapLen;
	for (size_t idx = 0; idx < mFieldIndexMapping.size(); idx++)
  {
    int fieldId = mFieldIndexMapping[idx];
    if ( idx < mCompoundKeyFieldNums )
    {
      fieldValue = keys[idx];
      decodeKeys.append(fieldValue);
      if (idx != mCompoundKeyFieldNums - 1) decodeKeys.append(",");
    }
    else
    {
      ret = get_value_by_id(valueHead, fieldId, fieldValue);
      if ( ret != 0 )
      {
        log_error("parse field value failed! compoundValue:%s", encodeVal.c_str());
        return -1;
      }
      decodeVals.append(fieldValue);
      if (idx != mFieldIndexMapping.size() - 1) decodeVals.append(",");
    }
  }

	return(0);	
}