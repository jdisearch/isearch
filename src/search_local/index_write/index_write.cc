/*
 * =====================================================================================
 *
 *       Filename:  index_write.cc
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

#include "index_write.h"
#include <iostream>
#include <string>
#include <map>
#include "log.h"
#include "poll_thread.h"
#include "task_request.h"
#include "dtc_tools.h"
#include "index_clipping.h"
#include "monitor.h"
#include "chash.h"
#include "index_tbl_op.h"
#include "geohash.h"
#include "type_parser_manager.h"
#include "system_state.h"

CTaskIndexGen::CTaskIndexGen(CPollThread * o) :
    CTaskDispatcher<CTaskRequest>(o),
    ownerThread(o),
    output(o)
{
}

CTaskIndexGen::~CTaskIndexGen()
{
}

int CTaskIndexGen::decode_request(const Json::Value & req, Json::Value & subreq, uint32_t & id)
{
    if (req.isMember("table_content") && req["table_content"].isObject()) {
        subreq = req["table_content"];
    }
    else {
        return RT_NO_TABLE_CONTENT;
    }

    if (req.isMember("appid") && req["appid"].isInt()) {
        id = req["appid"].asInt();
    }
    else {
        return RT_NO_APPID;
    }

    return 0;
}

int CTaskIndexGen::index_gen_process(Json::Value &req){

    if (req.isMember("read_only") && req["read_only"].isInt()) {
        return req["read_only"].asInt();
    }

    uint32_t app_id = 0;
    Json::Value table_content;
    int iRet = decode_request(req, table_content, app_id);
    if(iRet != 0){
        return iRet;
    }
    log_debug("table_content: %s", table_content.toStyledString().c_str());

    if(!SplitManager::Instance()->is_effective_appid(app_id)){
        return RT_NO_APPID;
    }

    iRet = TypeParserManager::Instance()->HandleTableContent(app_id , table_content);
    if (SystemState::Instance()->GetReportIndGenFlag()
        && (0 == iRet)){
        g_originalIndexInstance.Dump_Original_Request(req);
    }
    return iRet;
}

void CTaskIndexGen::TaskNotify(CTaskRequest * curr)
{
    log_debug("CTaskIndexGen::TaskNotify start");
    common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.indexGenTask"));
    //there is a race condition here:
    //curr may be deleted during process (in task->ReplyNotify())
    int ret;
    Json::Reader reader;
    Json::FastWriter writer;
    Json::Value value, res;
    std::string req;
    res["code"] = 0;

    CTaskRequest * task = curr;
    if(NULL == curr){
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
        return;
    }
    if(SERVICE_INDEXGEN != task->GetReqCmd()){
        res["code"] = RT_ERROR_SERVICE_TYPE;
        res["reqcmd"] = task->GetReqCmd();
        res["message"] = "service type wrong! need 106";
        goto end;
    }
    req = task->buildRequsetString();
    log_debug("recv:%s\n",req.c_str());
    if(!reader.parse(req,value,false))
    {
        log_error("parse json error!\ndata:%s errors:%s\n",req.c_str(),reader.getFormattedErrorMessages().c_str());
        res["code"] = RT_PARSE_JSON_ERR;
        res["message"] = reader.getFormattedErrorMessages();
        res["data"] = req;
        goto end;

    }
    if(!value.isObject()){
        log_error("parse json error!\ndata:%s errors:%s\n",req.c_str(),reader.getFormattedErrorMessages().c_str());
        res["code"] = RT_PARSE_JSON_ERR;
        res["message"] = "it's not a json";
        res["data"] = req;
        goto end;
    }
    ret = index_gen_process(value);
    if(0 != ret){
        res["code"] = ret;
    }

end:
    task->setResult(writer.write(res));
    task->ReplyNotify();

    common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return;
}
