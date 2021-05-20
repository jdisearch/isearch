#include "query_process.h"

QueryProcess::QueryProcess(uint32_t appid, Json::Value& value, Component* component)
:component_(component),
appid_(appid),
value_(value)
{

}

QueryProcess::~QueryProcess(){
    if(NULL != component_){
        delete component_;
    }
    if(NULL != logical_operate_){
        delete logical_operate_;
    }
    if(NULL != doc_manager_){
        delete doc_manager_;
    }
}

int QueryProcess::DoJob(){
    TaskBegin();
    int ret = ParseContent();
	if(0 != ret){
		return ret;
	}
    ret = GetValidDoc();
	if(0 != ret){
		return ret;
	}
    ret = GetScoreAndSort();
	if(0 != ret){
		return ret;
	}
    TaskEnd();
    return 0;
}

void QueryProcess::SetSkipList(SkipList& skipList){
    skipList_ = skipList;
}

void QueryProcess::SetRequest(CTaskRequest* request){
    request_ = request;
}

void QueryProcess::TaskBegin(){

}

int QueryProcess::ParseContent(){
    return 0;
}

int QueryProcess::GetValidDoc(){
    return 0;
}

int QueryProcess::GetScoreAndSort(){
    return 0;
}

void QueryProcess::TaskEnd(){

}

void QueryProcess::SetErrMsg(string err_msg){
    log_error(err_msg.c_str());
    err_msg_ = err_msg;
}

string QueryProcess::GetErrMsg(){
    return err_msg_;
}
