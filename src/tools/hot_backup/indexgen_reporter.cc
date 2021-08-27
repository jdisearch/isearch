#include "indexgen_reporter.h"
#include "sockaddr.h"
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sstream>
#include <unistd.h>
#include <fstream>
#include "json/json.h"

IndexgenReporter::IndexgenReporter()
    : m_oPacHeader()
    , m_oNetContext()
    , m_oNetfd(-1)
{
    memset(&m_oPacHeader , 0 , sizeof(m_oPacHeader));
    m_oPacHeader.magic = 0xfcfd;
    m_oPacHeader.cmd = htons(106);
    m_oPacHeader.seq_num = 0x3131;

    memset(m_oRevBuf , 0 , MAXDATASIZE);
}

IndexgenReporter::~IndexgenReporter()
{}

bool IndexgenReporter::Connect2LocalIndexGen(const IndexGenNetContext& oNetContext)
{
    bool bRet = false;
    if (m_oNetContext != oNetContext)
    {
        if (m_oNetfd > 0)
        {
            close(m_oNetfd);
            m_oNetfd = -1;
            log_info("local indexGenNet Config is changed");
        }
        m_oNetContext = oNetContext;
        bRet = ConnectToServer();
    }else{
        bRet = true;
    }
    
    return bRet;
}

bool IndexgenReporter::ConnectToServer()
{
    if (m_oNetfd > 0){
        return true;
    }
    
    log_info("connect to server. ip:%s, port:%d", m_oNetContext.m_sIp.c_str(), m_oNetContext.m_iPort);
    m_oNetfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_oNetfd < 0)
    {
        log_error("create socket failed. ip:%s, port:%d", m_oNetContext.m_sIp.c_str(), m_oNetContext.m_iPort);
        return false;
    }
  
    struct sockaddr_in net_addr;
    net_addr.sin_addr.s_addr = inet_addr(m_oNetContext.m_sIp.c_str());
    net_addr.sin_family = AF_INET;
    net_addr.sin_port = htons( m_oNetContext.m_iPort);
    // block to connect
    timeval timeout = {10,0};
    setsockopt(m_oNetfd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));
    int ret = connect(m_oNetfd, (struct sockaddr *)&net_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        log_error("connect to server failed, fd:%d, errno:%d, ip:%s, port:%d", m_oNetfd, errno
         , m_oNetContext.m_sIp.c_str(), m_oNetContext.m_iPort);
        close(m_oNetfd);
        m_oNetfd = -1;
        return false;
    }
  
    // set socket to non-block
    // fcntl(m_oNetfd, F_SETFL, O_RDWR|O_NONBLOCK);
    log_info("connect to server success.");
    return true;
}

int IndexgenReporter::HandleAndReporte2IG(std::string sValue)
{
    if (!SendToPeerIG(sValue))
	{
        log_error("how can we do here!! ");
	}

	log_debug("hb respond raw data to indexGen");
	return 0;
}

bool IndexgenReporter::SendToPeerIG(const std::string& sRequest)
{
    if (CheckPeerIsOffline())
    {
        if (m_oNetfd > 0)
        {
            close(m_oNetfd);
            m_oNetfd = -1;
        }
        
        static int iCount = 0;
        bool bRet = false;
        while ((!bRet) && ((++iCount)%6 != 0))
        {
            bRet = ConnectToServer();
			log_warning("reconnect to peer indexgen retFlag: %d, timeCount:%d.", (int)bRet ,iCount);
			sleep(1);
        }
        iCount = 0;
        if (!bRet) {
            return false;
        }
    }
    
    m_oPacHeader.len = htonl(sRequest.length());
    log_debug("pacHeaderLen:%d.", m_oPacHeader.len);

    int iNum = 0;
    if(-1 == (iNum = send(m_oNetfd,&m_oPacHeader,sizeof(m_oPacHeader),0)))
    {
        log_error("send pacHeader failed.");
        return false;
	}

    if(-1 == (iNum = send(m_oNetfd, sRequest.c_str(), sRequest.length(),0)))
    {
		log_error("send body error.");
        return false;
	}
    log_debug("send pacBody len: %d." , iNum);

	if(-1 == (iNum = recv(m_oNetfd , m_oRevBuf , MAXDATASIZE , 0)))
	{
        log_error("responds error.");
        return false;
	}

    m_oRevBuf[iNum]='\0';
    log_debug("recv: %d bytes." , iNum);

    Json::Reader reader;
	Json::Value value;
    std::string sResponse(&m_oRevBuf[sizeof(m_oPacHeader)] , iNum - sizeof(m_oPacHeader));
    log_debug("server message: %s.", sResponse.c_str());

    if (!reader.parse(sResponse, value, false)) {
		log_error("parse json error!\ndata:%s errors:%s\n", sResponse.c_str(), reader.getFormattedErrorMessages().c_str());
		return false;
	}
    if (!value.isObject()) {
		log_error("parse json error!\ndata:%s errors:%s\n", sResponse.c_str(), reader.getFormattedErrorMessages().c_str());
		return false;		
	}

    int iRet = -1;
    if (value.isMember("code") 
        && value["code"].isInt())
    {
        iRet = value["code"].asInt();
    }

    log_debug("parse response ret: %d.", iRet);
    return (0 == iRet);
}

bool IndexgenReporter::CheckPeerIsOffline()
{
    tcp_info oInfo;
    int iLength = sizeof(oInfo);
	if(0 == getsockopt(m_oNetfd , IPPROTO_TCP , TCP_INFO , &oInfo , (socklen_t*)(&iLength)))
	{
        return (TCP_ESTABLISHED == oInfo.tcpi_state) ? false : true;
	}

	return false;
}