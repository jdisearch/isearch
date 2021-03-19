
/*
 * =====================================================================================
 *
 *       Filename:  key_format.h 
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __KEY_FORMAT_H__
#define __KEY_FORMAT_H__

#include <string>
#include <sstream>
#include <stdint.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <string.h>
#include <map>
#include <vector>

#include "table_def.h"

class key_format
{

public:
    static std::string Encode(const std::map<uint8_t, DTCValue *> &fieldValues, const DTCTableDefinition *table_def, uint64_t &caseSensitiveFreeLen);
    static std::string Encode(const std::vector<std::string> &fieldValues, const DTCTableDefinition *table_def, uint64_t &caseSensitiveFreeLen);
    static void Decode(const std::string &src, std::vector<std::string> &fieldValues, const DTCTableDefinition *table_def);

    static std::string Encode(
        const std::vector<std::string> &fieldValues,
        const std::vector<int> &fieldTypes);

    static void Decode(
        const std::string &src,
        const std::vector<int> &fieldTypes,
        std::vector<std::string> &fieldValues);

    static void decode_primary_key(
        const std::string &src,
        int keyType,
        std::string &pKey);

    static int get_format_key(
        const std::string &src,
        int fieldType,
        std::string &key);

    static int get_field_len(
        const char *src,
        int fieldType);

    static int Compare(
        const std::string &ls,
        const std::string &rs,
        const std::vector<int> &fieldTypes);

    // private:
    static std::string encode_bytes(const std::string &src);
    static std::string encode_bytes(int64_t src);
    static std::string encode_bytes(uint64_t src);
    static std::string encode_bytes(double src);
    static void DecodeBytes(const std::string &src, int64_t &dst);
    static void DecodeBytes(const std::string &src, std::string &dst);
    static void DecodeBytes(const std::string &src, uint64_t &dst);
    static void DecodeBytes(const std::string &src, double &dst);
};

#endif
