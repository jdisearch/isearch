/*
 * =====================================================================================
 *
 *       Filename:  key_list.h
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
#ifndef __CHC_KEYLIST_H
#define __CHC_KEYLIST_H

#include <string.h>
#include <map>

#include <value.h>

class NCKeyInfo
{
private:
	struct nocase
	{
		bool operator()(const char *const &a, const char *const &b) const
		{
			return strcasecmp(a, b) < 0;
		}
	};
	typedef std::map<const char *, int, nocase> namemap_t;

private:
	namemap_t keyMap;
	const char *keyName[8];
	uint8_t keyType[8];
	int keyCount;

public:
	// zero is KeyField::None
	void Clear(void)
	{
		keyCount = 0;
		memset(keyType, 0, sizeof(keyType));
		memset(keyName, 0, sizeof(keyName));
		keyMap.clear();
	}
	NCKeyInfo()
	{
		keyCount = 0;
		memset(keyType, 0, sizeof(keyType));
		memset(keyName, 0, sizeof(keyName));
	}
	NCKeyInfo(const NCKeyInfo &that)
	{
		keyCount = that.keyCount;
		memcpy(keyType, that.keyType, sizeof(keyType));
		memcpy(keyName, that.keyName, sizeof(keyName));
		for (int i = 0; i < keyCount; i++)
			keyMap[keyName[i]] = i;
	}
	~NCKeyInfo() {}

	int add_key(const char *name, int type);
	int Equal(const NCKeyInfo &other) const;
	int KeyIndex(const char *n) const;
	const char *key_name(int n) const { return keyName[n]; }
	int key_type(int id) const { return keyType[id]; }
	int key_fields(void) const { return keyCount; }
};

class NCKeyValueList
{
public:
	static int KeyValueMax;

public:
	NCKeyInfo *keyinfo;
	DTCValue *val;
	int fcount[8];
	int kcount;

public:
	NCKeyValueList(void) : keyinfo(NULL), val(NULL), kcount(0)
	{
		memset(fcount, 0, sizeof(fcount));
	}
	~NCKeyValueList()
	{
		FREE_CLEAR(val);
	}
	int key_fields(void) const { return keyinfo->key_fields(); }
	int key_type(int id) const { return keyinfo->key_type(id); }
	int key_count(void) const { return kcount; }
	const char *key_name(int id) const { return keyinfo->key_name(id); }

	void Unset(void);
	int add_value(const char *, const DTCValue &, int);
	DTCValue &Value(int row, int col) { return val[row * keyinfo->key_fields() + col]; }
	const DTCValue &Value(int row, int col) const { return val[row * keyinfo->key_fields() + col]; }
	DTCValue &operator()(int row, int col) { return Value(row, col); }
	const DTCValue &operator()(int row, int col) const { return Value(row, col); }
	int IsFlat(void) const
	{
		for (int i = 1; i < keyinfo->key_fields(); i++)
			if (fcount[0] != fcount[i])
				return 0;
		return 1;
	}
};

#endif
