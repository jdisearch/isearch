
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_orderby_unit.cc 
 *
 *    Description:  
//  
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

#include "rocksdb_orderby_unit.h"

RocksdbOrderByUnit::RocksdbOrderByUnit(
    DTCTableDefinition *tabdef,
    int rowSize,
    const std::vector<uint8_t> &fieldMap,
    std::vector<std::pair<int, bool>> &orderByFields)
    : mMaxRowSize(rowSize),
      mComparator(OrderByComparator(tabdef, fieldMap, orderByFields)),
      mRawRows(mComparator)
{
}

void RocksdbOrderByUnit::add_row(const OrderByUnitElement &ele)
{
  mRawRows.push(ele);

  if (mRawRows.size() > mMaxRowSize)
    mRawRows.pop();

  return;
}

int RocksdbOrderByUnit::get_row(OrderByUnitElement &ele)
{
  if (mRawRows.empty())
    return 0;

  ele = mRawRows.top();
  mRawRows.pop();

  return 1;
}
