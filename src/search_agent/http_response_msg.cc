#include "http_response_msg.h"
#include <stdio.h>

#ifdef WIN32

#include <time.h>

#ifdef tzset
#undef tzset
#endif
#define tzset _tzset

#endif

static void httptime(struct tm *gmt, char *szDate)
{
    char szWeekDay[][8] = {"SUN", "Mon", "TUE", "WED", "THU", "FRI", "SAT"};
    char szMonths[][8] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    sprintf(szDate, "Date: %s, %02d %s %04d %02d:%02d:%02d GMT",
            szWeekDay[gmt->tm_wday], gmt->tm_mday, szMonths[gmt->tm_mon], (gmt->tm_year) + 1900,
            gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
}

CHttpResponseMessage::CHttpResponseMessage() : m_nHttpVersion(0), m_isKeepAlive(false), m_nStatusCode(0)
{
}

void CHttpResponseMessage::SetCode(int nCode)
{
    m_nStatusCode = nCode;
}

void CHttpResponseMessage::SetKeepAlive(bool isKeepAlive)
{
    m_isKeepAlive = isKeepAlive;
}

void CHttpResponseMessage::SetBody(const string &strResponse)
{
    m_strResponse = strResponse;
}

void CHttpResponseMessage::SetHttpVersion(int nHttpVersion)
{
    m_nHttpVersion = nHttpVersion;
}

const string &CHttpResponseMessage::Encode(int nHttpVersion,
                                           bool isKeepAlive,
                                           const string &strCharSet,
                                           const string &strContentType,
                                           const string &strResponse,
                                           int nHttpCode,
                                           const vector<string> &vecCookie,
                                           const string &strLocationAddr,
                                           const string &strHeaders)
{
    //sprintf(bufLength,"Content-Length: %d\r\n",body.length());
    time_t tNow;
    struct tm *gmt;
    tzset();
    tNow = time(NULL);
    gmt = (struct tm *)gmtime(&tNow);
    char szDate[128] = {0};
    httptime(gmt, szDate);
    string strHeadOption = string("Content-Type: ") + strContentType + ";charset=" + strCharSet + "\r\n";

    if (nHttpVersion == 1)
    {
        char szHttpCode[128] = {0};
        sprintf(szHttpCode, "HTTP/1.1 %d OK\r\n", nHttpCode);
        m_strHttpResponseMsg.append(szHttpCode);
        m_strHttpResponseMsg.append("Server: HPS Server/1.0.1\r\n");

        m_strHttpResponseMsg.append(string(szDate) + "\r\n");

        m_strHttpResponseMsg += strHeadOption;

        m_strHttpResponseMsg.append("Transfer-Encoding: chunked\r\n");

        if (isKeepAlive)
        {
            m_strHttpResponseMsg.append("Connection: keep-alive\r\n");
        }
        else
        {
            m_strHttpResponseMsg.append("Connection: close\r\n");
        }

        m_strHttpResponseMsg.append("Cache-Control:no-cache\r\n");

        if (vecCookie.size() > 0)
        {
            m_strHttpResponseMsg.append("P3P: CP=CAO PSA OUR\r\n");

            for (vector<string>::const_iterator iter = vecCookie.begin();
                 iter != vecCookie.end(); ++iter)
            {
                m_strHttpResponseMsg.append("Set-Cookie: ");
                m_strHttpResponseMsg.append(*iter);
                m_strHttpResponseMsg.append("\r\n");
            }
        }

        if (nHttpCode == 302 && !strLocationAddr.empty())
        {
            m_strHttpResponseMsg.append("Location: ");
            m_strHttpResponseMsg.append(strLocationAddr);
            m_strHttpResponseMsg.append("\r\n");
        }

        m_strHttpResponseMsg.append(strHeaders);

        m_strHttpResponseMsg.append("\r\n");
        char szBodyLen[16] = {0};
        sprintf(szBodyLen, "%zu", strResponse.length());
        m_strHttpResponseMsg.append(string(szBodyLen) + "\r\n");
        m_strHttpResponseMsg.append(strResponse);
        m_strHttpResponseMsg.append("\r\n\r\n\r\n");
    }
    else //http1.0
    {
        char szHttpCode[128] = {0};
        sprintf(szHttpCode, "HTTP/1.0 %d OK\r\n", nHttpCode);
        m_strHttpResponseMsg.append(szHttpCode);
        m_strHttpResponseMsg.append("Server: HPS Server/1.0.1\r\n");

        m_strHttpResponseMsg.append(string(szDate) + "\r\n");

        m_strHttpResponseMsg += strHeadOption;

        char bufLength[64] = {0};
        sprintf(bufLength, "Content-Length: %zu\r\n", strResponse.length());
        m_strHttpResponseMsg.append(bufLength);

        if (isKeepAlive)
        {
            m_strHttpResponseMsg.append("Connection: keep-alive\r\n");
        }

        m_strHttpResponseMsg.append("Cache-Control:no-cache\r\n");

        if (vecCookie.size() > 0)
        {
            m_strHttpResponseMsg.append("P3P: CP=CAO PSA OUR\r\n");

            for (vector<string>::const_iterator iter = vecCookie.begin();
                 iter != vecCookie.end(); ++iter)
            {
                m_strHttpResponseMsg.append("Set-Cookie: ");
                m_strHttpResponseMsg.append(*iter);
                m_strHttpResponseMsg.append("\r\n");
            }
        }

        if (nHttpCode == 302 && !strLocationAddr.empty())
        {
            m_strHttpResponseMsg.append("Location: ");
            m_strHttpResponseMsg.append(strLocationAddr);
            m_strHttpResponseMsg.append("\r\n");
        }

        m_strHttpResponseMsg.append(strHeaders);

        m_strHttpResponseMsg.append("\r\n");
        m_strHttpResponseMsg.append(strResponse);
    }

    return m_strHttpResponseMsg;
}
