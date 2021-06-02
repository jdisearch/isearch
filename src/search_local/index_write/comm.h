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
using namespace std;

#define BUILD_BIGINT_KEY(a,b) ((((unsigned long long)(a)) << 32)&0xffffffff00000000ll) | (b);
#define KETTOAPPID(a) (((unsigned long long)(a))>>32)&0xFFFFFFFF
#define MESSAGE "message"


struct item {
	string doc_id;
	uint32_t freq;
	vector<uint32_t> indexs;
	string extend;
};

struct InsertParam{
	uint32_t appid;
	string doc_id;
	uint32_t doc_version;
	uint32_t trans_version;	
};

enum CHARACTERTYPE {
	CHINESE = 1,  
	INITIAL = 2,  
	WHOLE_SPELL = 3,  
};

enum FieldType{
	FIELD_INT = 1,
	FIELD_STRING,
	FIELD_TEXT,
	FIELD_IP,
	FIELD_LNG,
	FIELD_LAT,
	FIELD_GIS,
	FIELD_DISTANCE,
	FIELD_DOUBLE,
	FIELD_LONG,
	FIELD_INDEX = 11,
	FIELD_LNG_ARRAY,
	FIELD_LAT_ARRAY,
	FIELD_WKT,
};

enum SEGMENTTAG {
	SEGMENT_NONE = 0,
	SEGMENT_DEFAULT = 1,
	SEGMENT_NGRAM = 2,
	SEGMENT_CHINESE = 3,
	SEGMENT_ENGLISH = 4,
	SEGMENT_RANGE = 5,
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
	RT_ERROR_INDEX_READONLY
};

#endif