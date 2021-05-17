#include "query_process.h"

QueryProcess::QueryProcess(uint32_t appid, Json::Value& value, Component* component)
:component_(component),
appid_(appid),
value_(value)
{

}

QueryProcess::~QueryProcess(){

}

int QueryProcess::DoJob(){
    TaskBegin();
    ParseContent();
    GetValidDoc();
    GetScoreAndSort();
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
