#ifndef __HTTP_RESPONSE_MSG_H__
#define __HTTP_RESPONSE_MSG_H__
#include <string>
#include <vector>
using namespace std;
class  CHttpResponseMessage
{
    public:
		CHttpResponseMessage();
		~CHttpResponseMessage(){}
		void SetCode(int nCode);
		void SetKeepAlive(bool isKeepAlive);
		void SetBody(const string &strResponse);
		void SetHttpVersion(int nHttpVersion);
		
		
		
		const string & Encode(int nHttpVersion,bool isKeepAlive,const string &strCharSet,
			const string &strContentType, const string &strResponse,int nHttpCode,
			const vector<string>& vecCookie, const string& strLocationAddr,
			const string &strHeaders);
                
		
	private:
		int m_nHttpVersion;
		bool m_isKeepAlive;
		int m_nStatusCode;
		string m_strResponse;
		
		string m_strHttpResponseMsg;
};

#endif
