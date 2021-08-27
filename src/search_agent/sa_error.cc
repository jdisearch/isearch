#include "sa_error.h"
#include "json/json.h"

static std::map<SEARCH_AGENT_ERR_CODE, std::string> g_mapErrMessageMapper;

void InitErrorMessageMapper()
{
    g_mapErrMessageMapper.insert(make_pair(ERR_NO_VALID_SERVER, std::string("no valid server")));
    g_mapErrMessageMapper.insert(make_pair(ERR_SERVER_ERROR, std::string("server error")));
    g_mapErrMessageMapper.insert(make_pair(ERR_URI_PATH, std::string("error uri path")));
    g_mapErrMessageMapper.insert(make_pair(ERR_PARSE_MSG, std::string("index msg has no doc_id in header")));


}



std::string GetErrorMessage(SEARCH_AGENT_ERR_CODE iErrCode)
{
    Json::FastWriter wr;
    Json::Value errMessageValue;
    errMessageValue["err_code"] = iErrCode;
     
    std::map<SEARCH_AGENT_ERR_CODE, std::string>::iterator iter = g_mapErrMessageMapper.find(iErrCode);

    if(iter != g_mapErrMessageMapper.end()){
       errMessageValue["err_msg"] =  iter->second; 
    }
    else{
        errMessageValue["err_msg"] =  std::string("unknown error");
    }
    return wr.write(errMessageValue);

}
