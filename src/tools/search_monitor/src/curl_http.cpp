#include <assert.h>
#include <stdarg.h>
#include "curl_http.h"
// 接收到数据的回调函数
// @param curl接收到的数据所在缓冲区  
// @param [in] size 数据长度  
// @param [in] nmemb 数据片数量  
// @param [in/out] 用户自定义指针
// @return 获取的数据长度  
size_t CurlCallback(void* ptr, size_t size, size_t nmemb, void* userp)
{
    size_t read_bytes = size * nmemb;
    BuffV* buf = static_cast<BuffV*>(userp);

    if(!buf->CheckBuffer(buf->Size() + read_bytes)) {
        printf("Can't get enough memory!\n");
        return read_bytes;
    }

    buf->SetBuffer(ptr, read_bytes);

    //printf("read_bytes:%lu\n", read_bytes);
    return read_bytes;
}

CurlHttp::CurlHttp()
{
    m_curl = curl_easy_init();
    assert(NULL != m_curl);

    SetTimeout(5L);

    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1l);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1l);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, CurlCallback);

    memset(m_params, 0, sizeof(m_params));
}

CurlHttp::~CurlHttp()
{
    curl_easy_cleanup(m_curl);
}

void CurlHttp::SetTimeout(long timeout)
{
    // 超时设置（单位：秒）
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, timeout);
}

void CurlHttp::SetTimeout_MS(long timeout_ms)
{
    // 超时设置（单位：毫秒）
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
}

void CurlHttp::SetHttpParams(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(m_params, sizeof(m_params), format, ap);
    va_end(ap);
}

int CurlHttp::HttpRequest(const std::string& url, BuffV* buf, bool is_get, struct curl_slist *headers)
{
    if(url.empty()) {
        return -100000;
    }
	
    CURLcode iCurlRet;
	
    std::string addr = url;
    if(is_get) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
        AppendGetParam(&addr);
    } else {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_params);
		//struct curl_slist *headers = NULL;
		headers =curl_slist_append(headers,"Content-Type: application/json");
		curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);

    }

    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, buf);

    //printf("addr:%s\n", addr.c_str());
    curl_easy_setopt(m_curl, CURLOPT_URL, addr.c_str());
	
    iCurlRet = curl_easy_perform(m_curl);
	memset(m_params, 0, sizeof(m_params));
	curl_slist_free_all(headers);
	
    return iCurlRet;
}


int CurlHttp::HttpRequest(const std::string& url, BuffV* buf, bool is_get, std::string contentType)
{
	if(url.empty()) {
		return -100000;
	}

	CURLcode iCurlRet;
	struct curl_slist *headers = NULL;
	std::string addr = url;
	if(is_get) {
		curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
		AppendGetParam(&addr);
	} else {
		curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
		curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_params);
		
		//headers =curl_slist_append(headers,"Content-Type: application/json");
		headers =curl_slist_append(headers,contentType.c_str());
		curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);

	}

	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, buf);

	//printf("addr:%s\n", addr.c_str());
	curl_easy_setopt(m_curl, CURLOPT_URL, addr.c_str());

	iCurlRet = curl_easy_perform(m_curl);
	memset(m_params, 0, sizeof(m_params));
	curl_slist_free_all(headers);

	return iCurlRet;
}

// 为get请求追加参数
void CurlHttp::AppendGetParam(std::string* addr)
{
    if(m_params[0] == '\0') {
        return;
    }

    addr->append("?");
    addr->append(m_params);
}

int CurlHttp::SetLocalPortRange(long BeginPort, long LocalPortRange)
{
	
	if(BeginPort) {
		curl_easy_setopt(m_curl, CURLOPT_LOCALPORT, (long)BeginPort);
		curl_easy_setopt(m_curl, CURLOPT_LOCALPORTRANGE, (long)LocalPortRange);
	}
	return 0;
}

