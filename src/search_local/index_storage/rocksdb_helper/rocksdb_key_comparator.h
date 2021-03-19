
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_key_comparator.h 
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
#ifndef __ROCKSDB_KEY_COMPARATOR_H__
#define __ROCKSDB_KEY_COMPARATOR_H__

#include "log.h"
#include "key_format.h"
#include "rocksdb_conn.h"

#include "rocksdb/comparator.h"
using namespace rocksdb;

// compare the string with length of n, not the end of '\0'
extern int my_strn_case_cmp(const char *s1, const char *s2, size_t n)
{
  for (size_t idx = 0; idx < n; idx++)
  {
    char c1 = ::toupper(*(s1 + idx));
    char c2 = ::toupper(*(s2 + idx));
    if (c1 == c2)
      continue;
    if (c1 < c2)
      return -1;
    return 1;
  }

  return 0;
}

class CommKeyComparator
{
private:
  bool mCaseSensitiveKey; // key need to be compared with case sensitive
  bool mHasVirtualField;

public:
  void set_compare_flag(bool keySensitive, bool hasVirtualField)
  {
    mCaseSensitiveKey = keySensitive;
    mHasVirtualField = hasVirtualField;
  }

  bool operator()(const std::string &lk, const std::string &rk) const
  {
    int ret = internal_compare(lk, rk);
    return ret < 0;
  }

  int internal_compare(const std::string &lk, const std::string &rk) const
  {
    int r;
    size_t lkLen = lk.length();
    size_t rkLen = rk.length();
    if (mHasVirtualField)
    {
      std::string lkNextStr = lk.substr(lkLen - sizeof(uint64_t));
      std::string rkNextStr = rk.substr(rkLen - sizeof(uint64_t));
      uint64_t lkNextLen = 0, rkNextLen = 0;
      key_format::DecodeBytes(lkNextStr, lkNextLen);
      key_format::DecodeBytes(rkNextStr, rkNextLen);
#if 0
std::vector<int> fieldTypes;
fieldTypes.push_back(4);
fieldTypes.push_back(2);
fieldTypes.push_back(1);
std::vector<std::string> keys1;
if (lkNextLen > 0) key_format::Decode(lk, fieldTypes, keys1);
std::vector<std::string> keys2;
if (rkNextLen > 0) key_format::Decode(rk, fieldTypes, keys2);
#endif
      size_t lkPrevLen = lkLen - lkNextLen - sizeof(uint64_t);
      size_t rkPrevLen = rkLen - rkNextLen - sizeof(uint64_t);
      size_t minLen = lkPrevLen < rkPrevLen ? lkPrevLen : rkPrevLen;
      assert(minLen > 0);
      if (mCaseSensitiveKey)
        r = lk.compare(0, minLen, rk);
      else
        r = my_strn_case_cmp(lk.data(), rk.data(), minLen);

      if (r == 0)
      {
        if (lkPrevLen < rkPrevLen)
          r = -1;
        else if (lkPrevLen > rkPrevLen)
          r = 1;
      }

      if (r == 0)
      {
        assert(lkPrevLen == rkPrevLen);

        // need to compare the case sensitive free zone
        minLen = lkNextLen < rkNextLen ? lkNextLen : rkNextLen;
        if (!mCaseSensitiveKey)
          r = memcmp(lk.data() + lkPrevLen, rk.data() + rkPrevLen, minLen);
        else
          r = my_strn_case_cmp(lk.data() + lkPrevLen, rk.data() + rkPrevLen, minLen);
      }
    }
    else
    {
      size_t minLen = lkLen < rkLen ? lkLen : rkLen;
      assert(minLen > 0);
      if (mCaseSensitiveKey)
        r = lk.compare(0, minLen, rk);
      else
        r = my_strn_case_cmp(lk.data(), rk.data(), minLen);
    }

    if (r == 0)
    {
      if (lkLen < rkLen)
        r = -1;
      else if (lkLen > rkLen)
        r = 1;
    }

    return r;
  }

  // check whether the rocksdb key, atten that, it's internal rocksdb key, not the 'key'
  // in DTC user space
  bool case_sensitive_rockskey()
  {
    return mCaseSensitiveKey == true && mHasVirtualField == false;
  }
};

// whether ignoring the case of the characters when compare the rocksdb entire key base
// on the user definition key
extern CommKeyComparator gInternalComparator;
class CaseSensitiveFreeComparator : public rocksdb::Comparator
{
public:
  CaseSensitiveFreeComparator() {}

  const char *Name() const { return "CaseSensitiveFreeComparator"; }

  int Compare(const rocksdb::Slice &lhs, const rocksdb::Slice &rhs) const
  {
    assert(lhs.data_ != nullptr && rhs.data_ != nullptr);

    return gInternalComparator.internal_compare(lhs.ToString(), rhs.ToString());
  }

  void FindShortestSeparator(std::string *start, const Slice &limit) const override
  {
    // not implement now
    return;
  }

  void FindShortSuccessor(std::string *key) const override
  {
    // not implement now
    return;
  }

  // user define interface to using hash index feature
  virtual bool CanKeysWithDifferentByteContentsBeEqual() const
  {
    // 1.rocksdb will use hashtable to index the binary seach key module that reside in the
    //   tail of the datablock, it use case sensitive hash function to create hash code, so
    //   if user want to use this feature, it must ensure that keys should be case sensitive
    // 2.data block hash index only support point lookup, Range lookup request will fall back
    //   to BinarySeek
    if (gInternalComparator.case_sensitive_rockskey())
    {
      log_info("use hash index feature!!!!!!!!!");
      return false;
    }

    log_info("use binary search index feature!!!!!!!!!");
    return true;
  }
};

#endif // __ROCKSDB_KEY_COMPARATOR_H__
