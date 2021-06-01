/*
 * =====================================================================================
 *
 *       Filename:  query_process.h
 *
 *    Description:  query_process class definition.
 *
 *        Version:  1.0
 *        Created:  14/05/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __MATCH_QUERY_PROCESS_H__
#define __MATCH_QUERY_PROCESS_H__

#include "query_process.h"

class MatchQueryProcess: public QueryProcess{
public:
    MatchQueryProcess(Json::Value& value);
    virtual~ MatchQueryProcess();

public:
    virtual int ParseContent(int logic_type);

private:
    virtual int ParseContent();
    virtual int GetValidDoc();
};

#endif