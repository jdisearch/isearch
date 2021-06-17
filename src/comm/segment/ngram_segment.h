/*
 * =====================================================================================
 *
 *       Filename:  ngram_segment.h
 *
 *    Description:  ngram_segment class definition.
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

#ifndef __NGRAM_SEGMENT_H__
#define __NGRAM_SEGMENT_H__
#include "segment.h"

class NgramSegment: public Segment
{
public:
    NgramSegment();
    ~NgramSegment();
    virtual void ConcreteSplit(iutf8string& phrase, uint32_t appid, vector<string>& vec);
private:
    void fmm(iutf8string& phrase, uint32_t appid, vector<string>& vec);
    void bmm(iutf8string& phrase, uint32_t appid, vector<string>& vec);
    double calSegProbability(const vector<string>& vec);
    bool getWordInfo(string word, uint32_t appid, WordInfo& word_info);
};



#endif