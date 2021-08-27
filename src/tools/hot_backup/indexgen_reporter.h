/*
 * =====================================================================================
 *
 *       Filename:  indexgen_reporter.h
 *
 *    Description:  indexgen_reporter class definition.
 *
 *        Version:  1.0
 *        Created:  11/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef INDEXGEN_REPORTER_H_
#define INDEXGEN_REPORTER_H_

#include <vector>
#include <string>
#include "singleton.h"
#include "log.h"
#include "noncopyable.h"
#include "dtcapi.h"

// connect to the local index_gen
#define MAXDATASIZE 4096

struct CPacketHeader {
	uint16_t magic;
	uint16_t cmd;
	uint32_t len;
	uint32_t seq_num;
};

struct IndexGenNetContext
{
    std::string m_sIp;
    int m_iPort;

    IndexGenNetContext()
        : m_sIp("127.0.0.1")
        , m_iPort(11017)
    { }

    IndexGenNetContext(std::string _sIp , std::string _sPort)
        : m_sIp(_sIp)
        , m_iPort(11017)
    {
        try
        {
            m_iPort = atoi(_sPort.c_str());
        }
        catch(...)
        {
            log_error("string to int has exception.");
        }
    }

    bool operator !=(const IndexGenNetContext& oNetContext)
    {
        return (m_sIp != oNetContext.m_sIp) || (m_iPort != oNetContext.m_iPort);
    }
};

class IndexgenReporter : private noncopyable
{
public:
    IndexgenReporter();
    virtual ~IndexgenReporter();

public:
    static IndexgenReporter* Instance()
    {
        return CSingleton<IndexgenReporter>::Instance();
    };

    static void Destroy()
    {
        CSingleton<IndexgenReporter>::Destroy();
    };

public:
    int HandleAndReporte2IG(std::string sValue);
    bool Connect2LocalIndexGen(const IndexGenNetContext& oNetContext);

private:
    bool ConnectToServer();
    bool SendToPeerIG(const std::string& );
    bool CheckPeerIsOffline(void);

private:
    CPacketHeader m_oPacHeader;
    char m_oRevBuf[MAXDATASIZE];
    IndexGenNetContext m_oNetContext;
    int m_oNetfd;
};

#endif
