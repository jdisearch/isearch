#ifndef __SA_ERROR_H__
#define __SA_ERROR_H__

#include <map>
#include <string>


typedef enum{
    ERR_NO_VALID_SERVER = 20001,
    ERR_SERVER_ERROR,
    ERR_URI_PATH,
    ERR_PARSE_MSG,
}SEARCH_AGENT_ERR_CODE;


void InitErrorMessageMapper();


std::string GetErrorMessage(SEARCH_AGENT_ERR_CODE iErrCode);

#endif
