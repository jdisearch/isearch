/*
 * =====================================================================================
 *
 *       Filename:  bmm_segment.h
 *
 *    Description:  bmm_segment class definition.
 *
 *        Version:  1.0
 *        Created:  08/06/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __BMM_SEGMENT_H__
#define __BMM_SEGMENT_H__
#include "segment.h"

class BmmSegment: public Segment
{
private:
    /* data */
public:
    BmmSegment();
    ~BmmSegment();
    virtual void ConcreteSplit(const string& str, uint32_t appid, vector<string>& vec);
};



#endif