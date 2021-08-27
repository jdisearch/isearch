/*
 * =====================================================================================
 *
 *       Filename:  index_write.h
 *
 *    Description:  IndexConf class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef INDEX_GEN_H_
#define INDEX_GEN_H_

#include <set>
#include <vector>
#include <sstream>
#include "request_base.h"
#include "index_conf.h"
#include "dtcapi.h"
#include "comm.h"
#include "split_manager.h"
using namespace std;

class CPollThread;
class CTaskRequest;
class SplitManager;

class CTaskIndexGen : public CTaskDispatcher<CTaskRequest>
{
private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;

private:
	int decode_request(const Json::Value &req, Json::Value &subreq, uint32_t &id);

public:
	CTaskIndexGen(CPollThread * o);
	virtual ~CTaskIndexGen();
	int index_gen_process(Json::Value &req);
	int pre_process(void);

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p){
	    output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};



#endif /* INDEX_GEN_H_ */
