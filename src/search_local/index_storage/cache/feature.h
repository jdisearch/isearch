/*
 * =====================================================================================
 *
 *       Filename:  feature.h
 *
 *    Description:  feature description character definition.
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

#ifndef __DTC_FEATURE_H
#define __DTC_FEATURE_H

#include "namespace.h"
#include "global.h"

DTC_BEGIN_NAMESPACE

// feature type
enum feature_id
{
	NODE_GROUP = 10, //DTC begin feature id
	NODE_INDEX,
	HASH_BUCKET,
	TABLE_INFO,
	EMPTY_FILTER,
	HOT_BACKUP,
	COL_EXPAND,
};
typedef enum feature_id FEATURE_ID_T;

struct feature_info
{
	uint32_t fi_id;			// feature id
	uint32_t fi_attr;		// feature attribute
	MEM_HANDLE_T fi_handle; // feature handler
};
typedef struct feature_info FEATURE_INFO_T;

struct base_info
{
	uint32_t bi_total; // total features
	FEATURE_INFO_T bi_features[0];
};
typedef struct base_info BASE_INFO_T;

class Feature
{
public:
	static Feature *Instance();
	static void Destroy();

	MEM_HANDLE_T Handle() const { return M_HANDLE(_baseInfo); }
	const char *Error() const { return _errmsg; }

	int modify_feature(FEATURE_INFO_T *fi);
	int delete_feature(FEATURE_INFO_T *fi);
	int add_feature(const uint32_t id, const MEM_HANDLE_T v, const uint32_t attr = 0);
	FEATURE_INFO_T *get_feature_by_id(const uint32_t id);

	//创建物理内存并格式化
	int Init(const uint32_t num = MIN_FEATURES);
	//绑定到物理内存
	int Attach(MEM_HANDLE_T handle);
	//脱离物理内存
	int Detach(void);

public:
	Feature();
	~Feature();

private:
	BASE_INFO_T *_baseInfo;
	char _errmsg[256];
};

DTC_END_NAMESPACE

#endif
