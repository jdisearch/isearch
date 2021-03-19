/*
 * =====================================================================================
 *
 *       Filename:  adaptive_prefix_extractor.h 
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
#ifndef __ADAPTIVE_PREFIX_EXTRACTOR_H__
#define __ADAPTIVE_PREFIX_EXTRACTOR_H__

#include "rocksdb/slice_transform.h"
#include "key_format.h"
#include "log.h"

#include <algorithm>

// prefix extractor for prefix search
class AdaptivePrefixExtractor : public SliceTransform
{
private:
  // static int mFixedKeyLen; // always encode to be 9 byte length
  // static bool mFixedKey;
  int mkey_type; // dtc key key type
  // bool mKeyCaseSensitive;

public:
  AdaptivePrefixExtractor(int keyType)
  {
    // mFixedKey = (keyType != DField::String && keyType != DField::Binary);
    mkey_type = keyType;
    // mKeyCaseSensitive = (keyType != DField::String ? true : false);
  }

  virtual const char *Name() const { return "AdaptivePrefixExtractor"; }

  // extract the prefix key base on customer define, and this function is used
  // when create the bloom filter
  virtual Slice Transform(const Slice &key) const
  {
    static bool printLog = true;
    if (printLog)
    {
      log_error("use prefix extractor!!!!!!");
      printLog = false;
    }

    // extract the key field as the prefix slice
    int keyLen = key_format::get_field_len(key.data_, mkey_type);
    return Slice(key.data_, (size_t)keyLen);
#if 0
    int prefixLen;
    if ( mFixedKey )
    {
      prefixLen = mFixedKeyLen;
    }
    else
    {
      // need to decode the key and extract the prefix
      // key_format::decode_primary_key(key.ToString(), mkey_type, prefixPrimaryKey);
      prefixLen = key_format::get_field_len(key.data(), mkey_type);
    }
    
    std::string prefix(key.data(), prefixLen);
    if ( !mKeyCaseSensitive )
    {
      // translate the prefix string to lower
      // std::transform(prefixPrimaryKey.begin(), prefixPrimaryKey.end(), prefixPrimaryKey.begin(), ::tolower);
      std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    }
    
    // whether need to append the 4 bytes placeholder ???
    // prefix.append();
    // if ( mHasVirtualField ) fullKey.append(key_format::encode_bytes((uint64_t)0));

    return prefix;
    // return prefixPrimaryKey;
#endif
  }

  // 1.before call 'Transform' function to do logic extract, rocksdb firstly call
  //   'InDomain' to determine whether the key compatible with the logic specified
  //   in the Transform method.
  // 2.inserted key always contains the prefix, so just return true
  virtual bool InDomain(const Slice &key) const { return key.size_ > 0; }

  // This is currently not used and remains here for backward compatibility.
  virtual bool InRange(const Slice & /*dst*/) const { return false; }

  virtual bool FullLengthEnabled(size_t *len) const
  {
    log_error("unexpected call !!!!!");

    // unfixed dynamic key always return false to disable this function
    return false;
  }

  // this function is only used by customer user to check whether the 'prefixKey'
  // can use the current extractor, always turn true
  virtual bool SameResultWhenAppended(const Slice &prefixKey) const
  {
    return true;
  }
};

// '1' for append delimiter '|', in future, remove the redundant '|' in key_format encode style
// int AdaptivePrefixExtractor::mFixedKeyLen = sizeof(uint64_t);
// bool AdaptivePrefixExtractor::mFixedKey = false;

#endif // __ADAPTIVE_PREFIX_EXTRACTOR_H__
