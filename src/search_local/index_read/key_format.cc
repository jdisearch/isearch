#include "key_format.h"
#include <iostream>
#include <utility>
#include "comm.h"

#define SEGMENT_SIZE  8

const std::string  SEG_SYMBOL = "|";

const char ENCODER_MARKER = 127;
const uint64_t  signMask = 0x8000000000000000;

uint64_t encode_into_cmp_uint(int64_t src) {

    return  uint64_t(src) ^ signMask;
}

uint64_t htonll(uint64_t val) {
    return (((uint64_t)htonl(val)) << 32) + htonl(val >> 32);
}

uint64_t ntohll(uint64_t val)
{
    return (((uint64_t)ntohl(val)) << 32) + ntohl(val >> 32);
}

std::string KeyFormat::Encode(const UnionKey& oUnionKey)
{
    std::string sUnionKey;
    for (size_t i = 0; i < oUnionKey.size(); ++i)
    {
        switch (oUnionKey[i].first)
        {
            case FIELD_INT:
            case FIELD_LONG:
            case FIELD_IP:
                {
                    sUnionKey.append(EncodeBytes((int64_t)strtoll(oUnionKey[i].second.c_str(), NULL, 10)));
                }
                break;
            case FIELD_DOUBLE:
                sUnionKey.append(EncodeBytes(strtod(oUnionKey[i].second.c_str(), NULL)));
                break;
            case FIELD_STRING:
            case FIELD_TEXT:
            case FIELD_GEO_POINT:
            case FIELD_GEO_SHAPE:
                sUnionKey.append(EncodeBytes(oUnionKey[i].second));
                break;
            default:
                sUnionKey.clear();
                break;
        }
    }
    return sUnionKey;
}

bool KeyFormat::Decode(const std::string& sKey, UnionKey& oUnionKey)
{
    if (oUnionKey.empty()){
        return false;
    }
    
    int iPos = 0;
    for (size_t i = 0; i < oUnionKey.size(); ++i)
    {
        switch (oUnionKey[i].first)
        {
        case FIELD_INT:
        case FIELD_LONG:
        case FIELD_IP:
            {
                int64_t lValue;
                DecodeBytes(sKey.substr(iPos, 8), lValue);
                iPos += 8;
                oUnionKey[i].second = std::to_string((long long)lValue);
            }
            break;
        case FIELD_DOUBLE:
            {
                double dValue;
                DecodeBytes(sKey.substr(iPos, 8), dValue);
                iPos += 8;
                oUnionKey[i].second = std::to_string((long double)dValue);
            }
            break;
        case FIELD_STRING:
        case FIELD_TEXT:
            {
                int begin_pos = iPos;
                iPos += SEGMENT_SIZE ;
                for ( ; ENCODER_MARKER == sKey[ iPos - 1 ] ; iPos += SEGMENT_SIZE) {
                }
                DecodeBytes(sKey.substr(begin_pos, iPos - begin_pos ), oUnionKey[i].second);
            }
            break;
        default:
            return false;
        }
    }
    return true;
}

std::string KeyFormat::EncodeBytes(const std::string & src)
{
    unsigned char padding_bytes;
    size_t left_length = src.length();
    size_t pos = 0;
    std::stringstream oss_dst;
    while (true) {
        unsigned char  copy_len = SEGMENT_SIZE - 1 < left_length ? SEGMENT_SIZE - 1 : left_length;
        padding_bytes = SEGMENT_SIZE - 1 - copy_len;
        oss_dst << src.substr(pos, copy_len);
        pos += copy_len;
        left_length -= copy_len;

        if (padding_bytes) {
            oss_dst << std::string(padding_bytes, '\0');
            oss_dst << (char)(ENCODER_MARKER - padding_bytes);
            break;
        }
        else {
            oss_dst << ENCODER_MARKER;
        }
    }
    return oss_dst.str();
}

std::string KeyFormat::EncodeBytes(int src)
{
    return EncodeBytes((int64_t)src);
}

std::string KeyFormat::EncodeBytes(int64_t src)
{
    uint64_t host_bytes = encode_into_cmp_uint(src);
    uint64_t net_bytes = htonll(host_bytes);
    char dst_bytes[8];
    memcpy(dst_bytes, &net_bytes, sizeof(uint64_t));
    std::string dst = std::string(8, '\0');
    for (size_t i = 0; i < dst.length(); i++) {
        dst[i] = dst_bytes[i];
    }
    return dst;
}

std::string KeyFormat::EncodeBytes(double src)
{
    uint64_t u;
    memcpy(&u, &src, sizeof(double));
    if (src >= 0) {
        u |= signMask;
    }
    else {
        u = ~u;
    }

    return EncodeBytes(u);
}

std::string KeyFormat::EncodeBytes(uint64_t src)
{
    uint64_t net_bytes = htonll(src);
    char dst_bytes[8];
    memcpy(dst_bytes, &net_bytes, sizeof(uint64_t));
    std::string dst = std::string(8, '\0');
    for (size_t i = 0; i < dst.length(); i++) {
        dst[i] = dst_bytes[i];
    }
    return dst;
}

void KeyFormat::DecodeBytes(const std::string & src, int64_t& dst)
{
    uint64_t net_bytes;
    memcpy(&net_bytes, src.c_str(), sizeof(uint64_t));
    uint64_t host_bytes = ntohll(net_bytes);
    dst = int64_t(host_bytes ^ signMask);
}

void KeyFormat::DecodeBytes(const std::string & src, std::string & dst)
{
    if (src.length() == 0) {
        dst = "";
    }
    std::stringstream oss_dst;
    for (size_t i = 0; i < src.length(); i += SEGMENT_SIZE) {
        char padding_bytes = ENCODER_MARKER - src[i + 7];
        oss_dst << src.substr(i, SEGMENT_SIZE - 1 - padding_bytes);
    }
    dst = oss_dst.str();
}

void KeyFormat::DecodeBytes(const std::string & src, uint64_t & dst)
{
    uint64_t net_bytes;
    memcpy(&net_bytes, src.c_str(), sizeof(uint64_t));
    dst = ntohll(net_bytes);
}

void KeyFormat::DecodeBytes(const std::string & src, double & dst)
{
    uint64_t u;
    DecodeBytes(src, u);

    if ((u & signMask) > 0) {
        u &= (~signMask);
    }
    else {
        u = ~u;
    }
    memcpy(&dst, &u, sizeof(dst));
}
