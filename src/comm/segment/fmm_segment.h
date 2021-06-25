/*
 * =====================================================================================
 *
 *       Filename:  fmm_segment.h
 *
 *    Description:  fmm_segment class definition.
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

#ifndef __FMM_SEGMENT_H__
#define __FMM_SEGMENT_H__
#include "segment.h"

class FmmSegment: public Segment
{
private:
    /* data */
public:
    FmmSegment();
    ~FmmSegment();
    virtual void ConcreteSplit(const string& str, uint32_t appid, vector<string>& vec);
};



#endif