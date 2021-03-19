/*
 * =====================================================================================
 *
 *       Filename:  compress.cc
 *
 *    Description:  add field compression.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "compress.h"
#include "zlib.h"
#include "mem_check.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

DTCCompress::DTCCompress()
{
    _level = 1;
    _buflen = 0;
    _len = 0;
    _buf = NULL;
}

DTCCompress::~DTCCompress()
{
    if (_buf)
        FREE(_buf);
}

void DTCCompress::set_compress_level(int level)
{
    _level = level < 1 ? 1 : (level > 9 ? 9 : level);
}
int DTCCompress::set_buffer_len(unsigned long len)
{
    if (_buf == NULL)
    {
        _buflen = len;
        _buf = (unsigned char *)MALLOC(len);
    }
    else if (_buflen < len)
    {
        _buflen = len;
        FREE(_buf);
        _buf = (unsigned char *)MALLOC(len);
    }
    if (_buf == NULL)
        return -ENOMEM;
    return 0;
}

//source 被压缩的缓冲区 sourcelen 被压缩缓冲区的原始长度
//dest   压缩后的缓冲区 destlen   被压缩后的缓冲区长度
//注意调用该函数时， destlen 首先要设置为dest缓冲区最大可以容纳的长度
int DTCCompress::compress(const char *source, unsigned long sourceLen)
{
    if (_buf == NULL || source == NULL)
    {
        return -111111;
    }
    _len = _buflen;
    return compress2(_buf, &_len, (Bytef *)source, sourceLen, _level);
}

//source 待解压的缓冲区 sourcelen 待解压缓冲区的原始长度
//dest   解压后的缓冲区 destlen   解缩后的缓冲区长度
//注意调用该函数时， destlen 首先要设置为dest缓冲区最大可以容纳的长度
int DTCCompress::UnCompress(char **buf, int *lenp, const char *source, unsigned long sourceLen)
{
    if (_buf == NULL || source == NULL)
    {
        snprintf(_errmsg, sizeof(_errmsg), "input buffer or uncompress buffer is null");
        return -111111;
    }
    _len = _buflen;
    int iret = uncompress(_buf, &_len, (Bytef *)source, sourceLen);
    if (iret)
    {
        snprintf(_errmsg, sizeof(_errmsg), "uncompress error,error code is:%d.check it in /usr/include/zlib.h", iret);
        return -111111;
    }
    *buf = (char *)MALLOC(_len);
    if (*buf == NULL)
        return -ENOMEM;
    memcpy(*buf, _buf, _len);
    *lenp = _len;
    return 0;
}
