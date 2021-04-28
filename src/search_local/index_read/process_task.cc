/*
 * =====================================================================================
 *
 *       Filename:  process_task.h
 *
 *    Description:  process task class definition.
 *
 *        Version:  1.0
 *        Created:  16/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "process_task.h"
#include "json/writer.h"
#include "poller.h"
#include <netinet/in.h>
#include <string>
#include <sstream>
using namespace std;


ProcessTask::ProcessTask()
{
}

int ProcessTask::Process(CTaskRequest *request) {
	log_debug("process");
	return 0;
}

ProcessTask::~ProcessTask()
{

}

string ProcessTask::GenReplyStr(RSPCODE code, string err_msg) {
	Json::Value m_ReplyPacket;
	m_ReplyPacket["code"] = code;
	if(err_msg != ""){
		m_ReplyPacket["message"] = err_msg;
	}
	Json::StyledWriter _sw;
	string resultStr = _sw.write(m_ReplyPacket);
	return resultStr;
}

