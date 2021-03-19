
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_orderby_unit.h 
 *
 *    Description:  order by using Heap sort to get the Top N elements
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

#ifndef __ROCKSDB_ORDER_BY_UNIT_H_
#define __ROCKSDB_ORDER_BY_UNIT_H_

#include "table_def.h"
#include <log.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <queue>

struct OrderByUnitElement
{
  std::vector<std::string> mRocksKeys;
  std::vector<std::string> mRocksValues;

  const std::string &getValue(int fid) const
  {
    if (fid < mRocksKeys.size())
      return mRocksKeys[fid];
    return mRocksValues[fid - mRocksKeys.size()];
  }
};

class RocksdbOrderByUnit
{
private:
  class OrderByComparator
  {
  private:
    DTCTableDefinition *mTableDef;
    std::vector<uint8_t> mRockFieldIndexes;
    std::vector<std::pair<int, bool /* asc is true */>> mOrderbyFields;

  public:
    OrderByComparator(
        DTCTableDefinition *tab,
        const std::vector<uint8_t> &fieldMap,
        std::vector<std::pair<int, bool>> &orderByFields)
        : mTableDef(tab),
          mRockFieldIndexes(fieldMap),
          mOrderbyFields(orderByFields)
    {
    }

    bool operator()(const OrderByUnitElement &lhs, const OrderByUnitElement &rhs) const
    {
      for (size_t idx = 0; idx < mOrderbyFields.size(); idx++)
      {
        int fieldType = mTableDef->field_type(mOrderbyFields[idx].first);
        int rocksFid = mRockFieldIndexes[mOrderbyFields[idx].first];
        const std::string &ls = lhs.getValue(rocksFid);
        const std::string &rs = rhs.getValue(rocksFid);
        switch (fieldType)
        {
        case DField::Signed:
        {
          int64_t li = strtoll(ls.c_str(), NULL, 10);
          int64_t ri = strtoll(rs.c_str(), NULL, 10);
          if (li == ri)
            break;
          return mOrderbyFields[idx].second /*asc*/ ? li < ri : li > ri;
        }

        case DField::Unsigned:
        {
          uint64_t li = strtoull(ls.c_str(), NULL, 10);
          uint64_t ri = strtoull(rs.c_str(), NULL, 10);
          if (li == ri)
            break;
          return mOrderbyFields[idx].second /*asc*/ ? li < ri : li > ri;
        }

        case DField::Float:
        {
          float li = strtod(ls.c_str(), NULL);
          float ri = strtod(rs.c_str(), NULL);
          if (li == ri)
            break;
          return mOrderbyFields[idx].second /*asc*/ ? li < ri : li > ri;
        }

        case DField::String:
        {
          // case ignore
          int llen = ls.length();
          int rlen = rs.length();
          int ret = strncasecmp(ls.c_str(), rs.c_str(), llen < rlen ? llen : rlen);
          if (0 == ret && llen == rlen)
            break;
          return mOrderbyFields[idx].second /*asc*/ ? (ret < 0 || (ret == 0 && llen < rlen)) : (ret > 0 || (ret == 0 && llen > rlen));
        }
        case DField::Binary:
        {
          int llen = ls.length();
          int rlen = rs.length();
          int ret = strncmp(ls.c_str(), rs.c_str(), llen < rlen ? llen : rlen);
          if (0 == ret && llen == rlen)
            break;
          return mOrderbyFields[idx].second /*asc*/ ? (ret < 0 || (ret == 0 && llen < rlen)) : (ret > 0 || (ret == 0 && llen > rlen));
        }

        default:
          log_error("unrecognized field type", mOrderbyFields[idx].first, fieldType);
          return -1;
        }
      }
    };
  };

private:
  int mMaxRowSize;
  OrderByComparator mComparator;
  std::priority_queue<OrderByUnitElement, std::vector<OrderByUnitElement>, OrderByComparator> mRawRows;

public:
  RocksdbOrderByUnit(
      DTCTableDefinition *tabdef,
      int rowSize,
      const std::vector<uint8_t> &fieldMap,
      std::vector<std::pair<int, bool>> &orderByFields);

  void add_row(const OrderByUnitElement &ele);
  int get_row(OrderByUnitElement &ele);

  /*
   * order in heap is opposite with the 'order by' semantic, wo need to reverse it
   * ig. 
   *   order by 'field' asc
   * element in heap will be: 7 5 3 1 -1, and the semantic of order by is: -1 1 3 5 7
   * */
  void reverse_rows()
  {
    // use deque to store rows can avoid this issuse, no need to implement
    return;
  }
};

#endif // __ROCKSDB_ORDER_BY_UNIT_H_
