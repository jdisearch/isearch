/*
 * =====================================================================================
 *
 *       Filename:  db_manager.h
 *
 *    Description:  db manager class definition.
 *
 *        Version:  1.0
 *        Created:  13/12/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __DB_MANAGER_H__
#define __DB_MANAGER_H__

#include "log.h"
#include "search_conf.h"
#include "data_manager.h"
#include "comm.h"
#include <string>
using namespace std;

struct AppFieldTable{
	map<uint32_t, map<string, AppFieldInfo> > appFieldInfo;
};

class DBManager
{
public:

	DBManager();
	~DBManager();
	static DBManager *Instance()
	{
		return CSingleton<DBManager>::Instance();
	}

	static void Destroy()
	{
		CSingleton<DBManager>::Destroy();
	}

	bool Init(const SGlobalConfig &global_cfg);
	uint32_t GetWordField(uint32_t& segment_tag, uint32_t appid, string field_name, FieldInfo &fieldInfo);
	uint32_t ToInt(const char* str);
	string ToString(const char* str);
	bool IsFieldSupportRange(uint32_t appid, uint32_t field_value);
	bool GetFieldValue(uint32_t appid, string field, uint32_t &field_value);
	bool GetUnionKeyField(uint32_t appid, vector<string> & field_vec);
	void ReplaceAppFieldInfo(string appFieldFile);
	bool GetFieldType(uint32_t appid, string field, uint32_t &field_type);

private:
	AppFieldTable *aTable;
};

#endif
