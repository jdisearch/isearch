/*
 * =====================================================================================
 *
 *       Filename:  comm.h
 *
 *    Description:  common enumeration classes definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __COMM_H__
#define __COMM_H__
#include <string>
#include <stdint.h>
#include <vector>
#include "comm_enum.h"
using namespace std;

#define BUILD_BIGINT_KEY(a,b) ((((unsigned long long)(a)) << 32)&0xffffffff00000000ll) | (b);
#define KETTOAPPID(a) (((unsigned long long)(a))>>32)&0xFFFFFFFF
#define MESSAGE "message"

struct table_info{
	int is_primary_key;
	int field_type;
	int index_tag;
	int snapshot_tag;
	int field_id;
	int segment_tag;
	int segment_feature;
	string index_info;
};

struct item {
    string doc_id;
    uint32_t freq;
    vector<uint32_t> indexs;
    string extend;

    void Clear(){
        doc_id.clear();
        freq = 0;
        indexs.clear();
        extend.clear();
    }
};

struct WordProperty{
    std::string s_format_key;
    uint32_t ui_word_freq;
    std::vector<uint32_t> word_indexs_vet;

    WordProperty()
        : s_format_key("")
        , ui_word_freq(0)
        , word_indexs_vet()
    { }

    WordProperty(const std::string& format_key
                , uint32_t word_freq)
        : s_format_key(format_key)
        , ui_word_freq(word_freq)
        , word_indexs_vet()
    { }

    WordProperty(const std::string& format_key
                , uint32_t word_freq
                , const std::vector<uint32_t>& word_indexs)
        : s_format_key(format_key)
        , ui_word_freq(word_freq)
        , word_indexs_vet(word_indexs)
    { }
};

struct InsertParam{
    uint32_t appid;
    uint32_t field_id; // 字段唯一标识
    std::string doc_id;
    uint32_t doc_version;
    uint32_t trans_version;
    std::string sExtend;

    InsertParam()
        : appid(0)
        , field_id(-1)
        , doc_id("")
        , doc_version(0)
        , trans_version(0)
        , sExtend("")
    { }

    InsertParam(uint32_t uiAppid
                , const std::string& sDocId
                , uint32_t uiDocVersion
                , uint32_t uiTransVersion)
    {
        appid = uiAppid;
        field_id = -1;
        doc_id = sDocId;
        doc_version = uiDocVersion;
        trans_version = uiTransVersion;
        sExtend = "";
    }

    void Clear(){
        appid = 0;
        field_id = -1;
        doc_id.clear();
        doc_version = 0;
        trans_version = 0;
        sExtend.clear();
    }
};

enum CHARACTERTYPE {
    CHINESE = 1,  
    INITIAL = 2,  
    WHOLE_SPELL = 3,  
};

enum SegmentFeature
{
    SEGMENT_FEATURE_DEFAULT = 0,   // 默认值，只支持前缀模糊匹配
    SEGMENT_FEATURE_ALLLOCATE = 1, // 支持任意位置的模糊匹配
    SEGMENT_FEATURE_SNAPSHOT = 2,  // 该字段的倒排索引中extend字段需带上快照信息
};

enum CmdType {
    CMD_INDEX_GEN    = 106,
    CMD_TOP_INDEX    = 107,
    CMD_SNAPSHOT     = 108,
    CMD_IMAGE_REPORT = 109,
};

enum RetCode{
    RT_CMD_ADD=10000,
    RT_CMD_UPDATE,
    RT_CMD_GET,
    RT_CMD_DELETE,
    RT_PARSE_JSON_ERR = 20001,
    RT_PARSE_CONF_ERR,
    RT_INIT_ERR,
    RT_NO_TABLE_CONTENT,
    RT_NO_FIELD_COUNT,
    RT_NO_APPID,
    RT_NO_DOCID,
    RT_NO_GIS_DEFINE,
    RT_ERROR_FIELD_COUNT,
    RT_ERROR_FIELD_CMD,
    RT_ERROR_FIELD,
    RT_ERROR_SERVICE_TYPE,
    RT_ERROR_GET_SNAPSHOT,
    RT_ERROR_DELETE_SNAPSHOT,
    RT_ERROR_UPDATE_SNAPSHOT,
    RT_ERROR_INSERT_SNAPSHOT,
    RT_ERROR_INSERT_TOP_INDEX_DTC,
    RT_ERROR_INSERT_INDEX_DTC,
    RT_ERROR_INVALID_SP_WORD,
    RT_ERROR_FIELD_FORMAT,
    RT_ERROR_GET_GISCODE,
    RT_NO_THIS_DOC,
    RT_UPDATE_SNAPSHOT_CONFLICT,
    RT_ERROR_INDEX_READONLY,
    RT_ERROR_CMD_TRANSFORM,
    RT_ERROR_WORD_SEGMENTATION,
    RT_ERROR_ILLEGAL_PARSE_CMD
};

#endif