/*
 * =====================================================================================
 *
 *       Filename:  table_def.h
 *
 *    Description:  
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
#ifndef __CH_TABLEDEF_H_
#define __CH_TABLEDEF_H_

#include "protocol.h"
#include "section.h"
#include "value.h"
#include "atomic.hpp"

#include <vector>

struct FieldDefinition
{
public:
	enum
	{
		FF_READONLY = 1,
		FF_VOLATILE = 2,
		FF_DISCARD = 4,
		FF_DESC = 0x10,
		FF_TIMESTAMP = 0x20,
	};

	typedef uint8_t fieldflag_t;

public:
	char *fieldName;
	int fieldSize; //bytes
	uint8_t fieldType;
	uint8_t nameLen;
	uint8_t offset;
	fieldflag_t flags;

	int boffset; //field的bits起始偏移
	int bsize;	 //bits
	uint16_t next;
};

extern const SectionDefinition tableAttributeDefinition;

class TableAttribute : public SimpleSection
{
public:
	enum
	{
		SingleRow = 0x1,
		admin_table = 0x2,
	};

	TableAttribute() : SimpleSection(tableAttributeDefinition) {}
	~TableAttribute() {}

	int is_single_row(void) const { return (int)(get_tag(1) ? (get_tag(1)->u64 & SingleRow) : 0); }
	void set_single_row(void) { set_tag(1, get_tag(1) ? (get_tag(1)->u64 | SingleRow) : SingleRow); }
	int is_admin_tab(void) const { return (int)(get_tag(1) ? (get_tag(1)->u64 & admin_table) : 0); }
	void set_admin_tab(void) { set_tag(1, get_tag(1) ? (get_tag(1)->u64 | admin_table) : admin_table); }
	unsigned int key_field_count(void) const { return get_tag(3) ? get_tag(3)->u64 : 0; }
	void set_key_field_count(unsigned int n) { set_tag(3, n); }

	//压缩字段用来表示设置compressflag的字段id，传给client端使用。
	//该标识占用tag1的高八位，共计一个字节
	int compress_field_id(void) const { return get_tag(1) ? (((get_tag(1)->u64) >> 56) & 0xFF) : -1; }
	void set_compress_flag(int n)
	{
		uint64_t tmp = (((uint64_t)n & 0xFF) << 56);
		set_tag(1, get_tag(1) ? (get_tag(1)->u64 & tmp) : tmp);
	}
};

class DTCTableDefinition
{
private:
	FieldDefinition *fieldList;
	uint16_t nameHash[256];
	TableAttribute attr;
	int maxFields;
	int usedFields;
	int numFields;
	int keyFields;
	DTCBinary tableName;
	char hash[16];

	// only used by client side
	AtomicS32 count;

	// client side use this determine local/internal object
	DTCValue *defaultValue;

	// only used by server side
	DTCBinary packedTableDefinition;
	DTCBinary packedFullFieldSet;
	DTCBinary packedFieldSet; // field[0] not include
	uint8_t *uniqFields;
	int rowSize;		 //bits
	uint16_t maxKeySize; // max real key size, maybe 0-256. (256 is valid)
	int16_t hasAutoInc;	 // the autoinc field id, -1:none 0-254, fieldid
	int16_t hasLastacc;
	int16_t hasLastmod;
	int16_t hasLastcmod;
	int16_t hasCompress; //flag for compress
	int16_t hasExpireTime;
	uint8_t keyFormat; // 0:varsize, 1-255:fixed, large than 255 is invalid
	uint8_t hasDiscard;
	int8_t indexFields;		 // TREE_DATA, disabled in this release
	uint8_t uniqFieldCnt;	 //the size of uniqFields member
	uint8_t keysAsUniqField; /* 0 == NO, 1 == EXACT, 2 == SUBSET */

	// no copy
	DTCTableDefinition(const DTCTableDefinition &);

public:
	DTCTableDefinition(int m = 0);
	~DTCTableDefinition(void);

	// client side only
	int Unpack(const char *, int);
	int INC(void)
	{
		if (defaultValue == 0)
			return ++count;
		else
			return 2;
	};
	int DEC(void)
	{
		if (defaultValue == 0)
			return --count;
		else
			return 1;
	};

	// common methods, by both side
	const char *table_name(void) const { return tableName.ptr; }
	const char *table_hash(void) const { return hash; }
	int is_same_table(const DTCBinary &n) const { return string_equal(tableName, n); }
	int is_same_table(const char *n) const { return string_equal(tableName, n); }
	int is_same_table(const DTCTableDefinition &r) const { return !memcmp(hash, r.hash, sizeof(hash)); }
	int is_same_table(const DTCTableDefinition *r) const { return r ? is_same_table(*r) : 0; }
	int hash_equal(const DTCBinary &n) const { return n.len == 16 && !memcmp(hash, n.ptr, 16); }
	int num_fields(void) const { return numFields; }
	int key_fields(void) const { return keyFields; }
	int field_id(const char *, int = 0) const;
	const char *field_name(int id) const { return fieldList[id].fieldName; }
	int field_type(int id) const { return fieldList[id].fieldType; }
	int field_size(int id) const { return fieldList[id].fieldSize; }
	int max_field_size(void) const
	{
		int max = 0;
		for (int i = key_fields(); i <= num_fields(); ++i)
		{
			if (field_size(i) > max)
			{
				max = field_size(i);
			}
		}
		return max;
	}
	unsigned int field_flags(int id) const { return fieldList[id].flags; }
	const char *key_name(void) const { return fieldList[0].fieldName; }
	int key_type(void) const { return fieldList[0].fieldType; }
	int key_size(void) const { return fieldList[0].fieldSize; }
	int is_read_only(int n) const { return fieldList[n].flags & FieldDefinition::FF_READONLY; }
	int is_volatile(int n) const { return fieldList[n].flags & FieldDefinition::FF_VOLATILE; }
	int is_discard(int n) const { return fieldList[n].flags & FieldDefinition::FF_DISCARD; }
	int is_desc_order(int n) const { return fieldList[n].flags & FieldDefinition::FF_DESC; }
	int is_timestamp(int n) const { return fieldList[n].flags & FieldDefinition::FF_TIMESTAMP; }

	int is_single_row(void) const { return attr.is_single_row(); }
	int is_admin_table(void) const { return attr.is_admin_tab(); }
	void set_admin_table(void) { return attr.set_admin_tab(); }
	int compress_field_id(void) const { return attr.compress_field_id(); }

#if !CLIENTAPI
	// this macro test is scope-test only, didn't affected the class implementation

	// server side only
	//to support Bitmap svr
	const int row_size(void) const { return (rowSize + 7) / 8; }; //返回rowsize 单位bytes
	const int b_row_size(void) const { return rowSize; };			 //返回rowsize 单位bits
	void set_row_size(int size) { rowSize = size; };
	int field_b_size(int id) const { return fieldList[id].bsize; }
	int field_b_offset(int id) const { return fieldList[id].boffset; }
	int field_offset(int id) const { return fieldList[id].offset; }

	void Pack(DTCBinary &v) { v = packedTableDefinition; }
	int build_info_cache(void);
	const DTCBinary &packed_field_set(void) const { return packedFullFieldSet; }
	const DTCBinary &packed_field_set(int withKey) const { return withKey ? packedFullFieldSet : packedFieldSet; }
	const DTCBinary &packed_definition(void) const { return packedTableDefinition; }

	int set_key(const char *name, uint8_t type, int size) { return add_field(0, name, type, size); }
	void set_single_row(void) { attr.set_single_row(); };
	int set_table_name(const char *);
	void set_default_value(int id, const DTCValue *val);
	void get_default_row(DTCValue *) const;
	const DTCValue *default_value(int id) const { return &defaultValue[id]; }
	void set_auto_increment(int n)
	{
		if (n >= 0 && n <= numFields)
			hasAutoInc = n;
	}
	void set_lastacc(int n)
	{
		if (n >= 0 && n <= numFields)
			hasLastacc = n;
		fieldList[n].flags |= FieldDefinition::FF_TIMESTAMP;
	}
	void set_lastmod(int n)
	{
		if (n >= 0 && n <= numFields)
			hasLastmod = n;
		fieldList[n].flags |= FieldDefinition::FF_TIMESTAMP;
	}
	void set_lastcmod(int n)
	{
		if (n >= 0 && n <= numFields)
			hasLastcmod = n;
		fieldList[n].flags |= FieldDefinition::FF_TIMESTAMP;
	}
	void set_compress_flag(int n)
	{
		if (n >= 0 && n <= numFields)
			attr.set_compress_flag(n);
	}
	void set_expire_time(int n)
	{
		if (n >= 0 && n <= numFields)
			hasExpireTime = n;
	}
	int has_discard(void) const { return hasDiscard; }
	int has_auto_increment(void) const { return hasAutoInc >= 0; }
	int key_auto_increment(void) const { return hasAutoInc == 0; }
	int auto_increment_field_id(void) const { return hasAutoInc; }
	int lastacc_field_id(void) const { return hasLastacc; }
	int lastmod_field_id(void) const { return hasLastmod; }
	int lastcmod_field_id(void) const { return hasLastcmod; }
	int expire_time_field_id(void) const { return hasExpireTime; }
	void mark_as_read_only(int n) { fieldList[n].flags |= FieldDefinition::FF_READONLY; }
	void mark_as_volatile(int n) { fieldList[n].flags |= FieldDefinition::FF_VOLATILE; }
	void mark_as_discard(int n)
	{
		fieldList[n].flags |= FieldDefinition::FF_DISCARD;
		hasDiscard = 1;
	}
	void mark_order_desc(int n) { fieldList[n].flags |= FieldDefinition::FF_DESC; }
	void mark_order_asc(int n) { fieldList[n].flags &= ~FieldDefinition::FF_DESC; }
	uint8_t uniq_fields() const { return uniqFieldCnt; }
	uint8_t *uniq_fields_list() { return uniqFields; }
	const uint8_t *uniq_fields_list() const { return uniqFields; }
	void mark_uniq_field(int n) { uniqFields[uniqFieldCnt++] = n; }
	int key_as_uniq_field() const { return keysAsUniqField == 1 ? true : false; }
	int key_part_of_uniq_field() const { return keysAsUniqField; } /* 0 -- NOT, 1 -- EXACT, 2 -- SUBSET */
	int index_fields() const { return indexFields; }
	void set_index_fields(int n) { indexFields = n; }

	int set_key_fields(int n = 1);
	int key_format(void) const { return keyFormat; }
	int max_key_size(void) const { return maxKeySize; }

	DTCValue packed_key(const char *pk)
	{
		DTCValue key;
		key.bin.ptr = (char *)pk;
		key.bin.len = key_format() ?: (*(unsigned char *)pk + 1);
		return key;
	}

	int add_field(int id, const char *name, uint8_t type, int size);
	int add_field(int id, const char *name, uint8_t type, int size, int bsize, int boffset);
#endif
};

class TableReference
{
private:
	DTCTableDefinition *tableDef;

public:
	DTCTableDefinition *table_definition(void) const { return tableDef; }
	operator DTCTableDefinition *(void) { return tableDef; }
	TableReference(DTCTableDefinition *t = 0) { tableDef = t; }
	TableReference(const TableReference &c) { tableDef = c.table_definition(); }
	virtual ~TableReference(void) {}

	inline void set_table_definition(DTCTableDefinition *t) { tableDef = t; }

	int num_fields(void) const { return tableDef->num_fields(); }
	int key_fields(void) const { return tableDef->key_fields(); }
	int key_type(void) const { return tableDef->key_type(); }
	const char *key_name(void) const { return tableDef->key_name(); }
	int key_size(void) const { return tableDef->key_size(); }
	int field_type(int n) const { return tableDef->field_type(n); }
	int field_size(int n) const { return tableDef->field_size(n); }
	int field_id(const char *n) const { return tableDef->field_id(n); }
	const char *field_name(int id) const { return tableDef->field_name(id); }
	int is_same_table(const DTCBinary &n) const { return tableDef->is_same_table(n); }
	int is_same_table(const char *n) const { return tableDef->is_same_table(n); }
	int is_same_table(const DTCTableDefinition &r) const { return tableDef->is_same_table(r); }
	int is_same_table(const DTCTableDefinition *r) const { return tableDef->is_same_table(r); }
	int hash_equal(const DTCBinary &n) const { return tableDef->hash_equal(n); }
	const char *table_name(void) const { return tableDef->table_name(); }
	const char *table_hash(void) const { return tableDef->table_hash(); }

#if !CLIENTAPI
	// this macro test is scope-test only, didn't affected the class implementation
	int key_format(void) const { return tableDef->key_format(); }
	int key_auto_increment(void) const { return tableDef->key_auto_increment(); }
	int auto_increment_field_id(void) const { return tableDef->auto_increment_field_id(); }
	int field_offset(int n) const { return tableDef->field_offset(n); }
	int field_b_size(int n) const { return tableDef->field_b_size(n); }
	int field_b_offset(int n) const { return tableDef->field_b_offset(n); }
	void get_default_row(DTCValue *value) const { tableDef->get_default_row(value); }
#endif
};

#endif
