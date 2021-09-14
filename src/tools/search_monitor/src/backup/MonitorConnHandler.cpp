////////////////////////////////////////////////////////
//
// Handle detector cluster vote request
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#include "MonitorConnHandler.h"

MonitorConnHandler::MonitorConnHandler(CPollThread* poll)
:
CPollerObject(poll, 0),
mThreadPoll(poll)
{
}

MonitorConnHandler::~MonitorConnHandler()
{
}

int MonitorConnHandler::AttachThread()
{
  EnableInput();
  int ret = CPollerObject::AttachPoller();
  if (ret < 0)
  {
    monitor_log_error("add event to poll failed.");
    return -1;
  }

  return 0;
}

// handle vote request and send response
void MonitorConnHandler::InputNotify()
{
  int recv_len = readSocket((void*)m_RecvBuf,4);
  if (recv_len == 0)
  {
    monitor_log_error("client close the fd.");
    CPollerObject::DetachPoller();
    delete this;
    return;
  }
  if (recv_len != 4)
  {
    monitor_log_error("revieve package header[cmdtype] error revcd:%d",recv_len);
    SendResponse(RCV_HEADER_ERR);
    return;
  }

  m_CmdType = (MIGRATE_CMD_TYPE) *(int32_t *)m_RecvBuf;

  recv_len = readSocket((void*)m_RecvBuf,4);
  if (recv_len != 4)
  {
    monitor_log_error("revieve package header[xml length] error revcd:%d",recv_len);
    SendResponse(RCV_HEADER_ERR);
    return;
  }
  return;
}

int CMigrateTask::readSocket(void* szBuf, int nLen)
{
  int nRead = 0;
  int nRet = 0;
  do {
    nRet = read(netfd, (char*)szBuf + nRead, nLen - nRead);
    if (nRet > 0)
    {
      nRead += nRet;
      if(nRead == nLen)
        return nRead;
      continue;
    }
    else if (nRet == 0)
    {
      return nRead;
    }
    else
    {
      if (errno == EAGAIN || errno == EINTR)
      {
        continue;
      }
      else
      {
        return nRead;
      }
    }
  }while(nRead < nLen);
  return nLen;
}

// for broadcast to get votes
int CMigrateTask::writeSocket(void* szBuf, int nLen)
{
  int nWrite = 0;
  int nRet = 0;
  do {
    nRet = write(netfd, (char*)szBuf + nWrite, nLen - nWrite);
    if (nRet > 0)
    {
      nWrite += nRet;
      if(nLen == nWrite)
      {
        return nWrite;
      }
      continue;
    }
    else
    {
      if (errno == EINTR || errno == EAGAIN)
      {
        continue;
      }
      else
      {
        return nWrite;
      }
    }
  }
  while(nWrite < nLen);
  return nLen;
}

void CMigrateTask::SendResponse(E_MIGRATERSP e_Result)
{
    writeSocket((void*)(&e_Result), sizeof(e_Result) );
}
