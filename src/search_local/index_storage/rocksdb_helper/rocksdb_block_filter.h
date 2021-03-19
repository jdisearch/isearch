
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_block_filter.h 
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
#pragma once
#if 0
// user define bloom filter in rocksdb datablock SST file for performance improving
class HashBloomFilterPolicy : public FilterPolicy
{
public:
  const char* Name() const override { return "HashBloomFilterPolicy"; }

  // create bloom filter base on the hash code of the prefix key
  void CreateFilter(const Slice* keys, int n, std::string* dst) const override 
  {
    for (int i = 0; i < n; i++) {
      uint32_t h = Hash(keys[i].data(), keys[i].size(), 1);
      PutFixed32(dst, h);
    }
  }

  bool KeyMayMatch(const Slice& key, const Slice& filter) const override 
  {
    uint32_t h = Hash(key.data(), key.size(), 1);
    for (unsigned int i = 0; i + 4 <= filter.size(); i += 4) {
      if (h == DecodeFixed32(filter.data() + i)) {
        return true;
      }
    }
    return false;
  }
};
#endif
