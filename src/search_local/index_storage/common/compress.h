/*
 * =====================================================================================
 *
 *       Filename:  compress.h
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
class DTCCompress
{
public:
    DTCCompress();
    virtual ~DTCCompress();

    void set_compress_level(int level);
    int set_buffer_len(unsigned long len);
    const char *error_message(void) const { return _errmsg; }

    //source 被压缩的缓冲区 sourcelen 被压缩缓冲区的原始长度
    //dest   压缩后的缓冲区 destlen   被压缩后的缓冲区长度
    //注意调用该函数时， destlen 首先要设置为dest缓冲区最大可以容纳的长度
    int compress(const char *source, unsigned long sourceLen);

    //source 待解压的缓冲区 sourcelen 待解压缓冲区的原始长度
    //dest   解压后的缓冲区 destlen   解缩后的缓冲区长度
    //注意调用该函数时， destlen 首先要设置为dest缓冲区最大可以容纳的长度
    int UnCompress(char **dest, int *destlen, const char *source, unsigned long sourceLen);

    unsigned long get_len(void) { return _len; }
    char *get_buf(void) { return (char *)_buf; }
    char _errmsg[512];

private:
    unsigned long _buflen;
    unsigned char *_buf;
    unsigned long _len;
    int _level;
};
