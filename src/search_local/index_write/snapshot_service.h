/*
 * =====================================================================================
 *
 *       Filename:  snapshot_service.h
 *
 *    Description:  CTaskSnapShot class definition.
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

#ifndef SNAPSHOT_SERVICE_H_
#define SNAPSHOT_SERVICE_H_
#include "request_base.h"
#include "index_conf.h"
#include "dtcapi.h"
#include "split_manager.h"
using namespace std;

class CTaskSnapShot : public CTaskDispatcher<CTaskRequest>
{
    private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;
	DTC::DTCServers index_servers;

    private:
	int get_snapshot_dtc(UserTableContent &fields,Json::Value &res);
	int update_sanpshot_dtc(const UserTableContent &fields,Json::Value &res);

    public:
	CTaskSnapShot(CPollThread * o);
	virtual ~CTaskSnapShot();
	int pre_process(void);
	int snapshot_process(Json::Value &req,Json::Value &res);

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p)
	{
	    output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};



#endif /* SNAPSHOT_SERVICE_H_ */
