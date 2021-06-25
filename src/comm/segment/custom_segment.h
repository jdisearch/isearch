/*
 * =====================================================================================
 *
 *       Filename:  custom_segment.h
 *
 *    Description:  custom_segment class definition.
 *
 *        Version:  1.0
 *        Created:  15/06/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __CUSTOM_SEGMENT_H__
#define __CUSTOM_SEGMENT_H__
#include "segment.h"
#include "../config.h"

typedef void (*split_interface)(const char* str, char* res, int len);

class CustomSegment: public Segment
{
public:
    CustomSegment();
    ~CustomSegment();
    virtual bool Init(string word_path, string train_path);
    virtual void ConcreteSplit(const string& str, uint32_t appid, vector<string>& vec);
private:
    CConfig* cache_config_;
    split_interface word_split_func_;
};



#endif