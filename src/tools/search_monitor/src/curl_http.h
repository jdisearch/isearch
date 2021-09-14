// Copyright (c) 2013, Tencent Inc. All Rights Reserved.
// Author: Lei Wang (pecywang@tencent.com)
// Date: 2013-05-23

#ifndef _HTTP_SERVICE_H_
#define _HTTP_SERVICE_H_
#pragma once

#include <assert.h>
#include <string.h>
#include <string>
#include <map>
#include "curl/curl.h"

typedef std::map<std::string, std::string> HttpParams;

struct BuffV
{
    BuffV() : m_used_len(0), m_total_len(0)
    {
        // 默认分配1K
        m_total_len = 2 * 1024;
        m_buf = new char[m_total_len];
        memset(m_buf, 0, sizeof(m_buf));
        assert(NULL != m_buf);
    }

    ~BuffV()
    {
        if(m_buf) {
            delete m_buf;
            m_buf = NULL;
        }
    }

    // 检测buf的长度是否大于need_len
    // 如果小于则自动扩展
    bool CheckBuffer(size_t need_len)
    {
        // 最大支持1M
        if(need_len > 1024 * 1024) {
            return false;
        }

        if(m_total_len > need_len) {
            return true;
        }

        m_total_len = need_len + 256; // 多申请一些
        char* new_buf = new char[m_total_len];
        assert(NULL != new_buf);
        memset(new_buf, 0, m_total_len);

        if(m_used_len) {
            memcpy(new_buf, m_buf, m_used_len);
        }

        delete m_buf;
        m_buf = new_buf;
        return true;
    }

    void SetBuffer(const void* ptr, size_t len)
    {
        memcpy(m_buf + m_used_len, ptr, len);
        m_used_len += len;
    }

    const char* Ptr() const
    {
        return m_buf;
    }

    size_t Size() const
    {
        return m_used_len;
    }

    size_t Capacity() const
    {
        return m_total_len;
    }

private:
    char* m_buf;

    // 当前使用长度
    size_t m_used_len;

    // 当前总长度
    size_t m_total_len;

};

class CurlHttp
{
public:
	CurlHttp();
    virtual ~CurlHttp();
	
	CurlHttp(const CurlHttp&);
    CurlHttp& operator = (const CurlHttp&);
    
	/*
    static CurlHttp* GetInstance()
    {
        static CurlHttp curl_http;
        return &curl_http;
    }*/

    // 设置请求的附加参数
    // 格式:key=value&key=value
    // POST请求传参需调用此函数
    void SetHttpParams(const char* format, ...);

    // 设置超时
    void SetTimeout(long timeout);
	void SetTimeout_MS(long timeout_ms);

    // 发起GET或POST请求
    // @param: buf 保存HTTP请求到的body内容
    int HttpRequest(const std::string& url, BuffV* buf, bool is_get = true, struct curl_slist *headers = NULL);
	int HttpRequest(const std::string& url, BuffV* buf, bool is_get, std::string contentType);

	int SetLocalPortRange(long BeginPort, long LocalPortRange);
	
private:
    
    void AppendGetParam(std::string* addr);

private:
    CURL* m_curl;

    char m_params[50 * 1024];
};

#endif // SODEPEND_HTTP_SERVICE_H_
