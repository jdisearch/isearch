/*
 * =====================================================================================
 *
 *       Filename:  comm_enum.h
 *
 *    Description:  common enum definition.
 *
 *        Version:  1.0
 *        Created:  26/08/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __COMM_ENUM_H__
#define __COMM_ENUM_H__

enum SORTTYPE {
    SORT_RELEVANCE = 1, // 默认，按相关性排序
    DONT_SORT = 3, //不排序
    SORT_FIELD_ASC = 4, // 按字段升序
    SORT_FIELD_DESC = 5, // 按字段降序
    SORT_GEO_DISTANCE = 6 // 按距离升序
};

enum FieldType{
    FIELD_INT = 1,
    FIELD_STRING = 2,
    FIELD_TEXT = 3,
    FIELD_IP = 4,
    FIELD_GEO_POINT = 5,
    FIELD_LAT = 6,
    FIELD_GIS = 7,
    FIELD_DISTANCE = 8,
    FIELD_DOUBLE = 9,
    FIELD_LONG = 10,
    FIELD_INDEX = 11,
    FIELD_LNG_ARRAY = 12,
    FIELD_LAT_ARRAY = 13,
    FIELD_GEO_SHAPE = 14
};

enum SEGMENTTAG {
    SEGMENT_NONE = 0,
    SEGMENT_DEFAULT = 1,
    SEGMENT_NGRAM = 2,
    SEGMENT_CHINESE = 3,
    SEGMENT_ENGLISH = 4,
    SEGMENT_RANGE = 5,
};

#endif