/*
 * =====================================================================================
 *
 *       Filename:  dag_segment.h
 *
 *    Description:  dag_segment class definition.
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

#ifndef __DAG_SEGMENT_H__
#define __DAG_SEGMENT_H__
#include "segment.h"

struct RouteValue {
    double max_route;
    uint32_t idx;
    RouteValue() {
        max_route = 0;
        idx = 0;
    }
};

class DagSegment: public Segment
{
public:
    DagSegment();
    ~DagSegment();
    virtual void ConcreteSplit(const string& str, uint32_t appid, vector<string>& vec);
private:
    void getDag(iutf8string& sentence, uint32_t appid, map<uint32_t, vector<uint32_t> >& dag_map);
    void calc(iutf8string& sentence, const map<uint32_t, vector<uint32_t> >& dag_map, map<uint32_t, RouteValue>& route, uint32_t appid);
};



#endif