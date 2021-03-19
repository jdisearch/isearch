/*
 * =====================================================================================
 *
 *       Filename:  dbconfig.h
 *
 *    Description:  database config operation.
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
#ifndef _H_DTC_DB_CONFIG_H_
#define _H_DTC_DB_CONFIG_H_

#include "table_def.h"
#include "config.h"
#include <stdint.h>
#include <vector>
#include <map>

#define MAXDB_ON_MACHINE 1000
#define GROUPS_PER_MACHINE 4
#define GROUPS_PER_ROLE 3
#define ROLES_PER_MACHINE 2
#define MACHINEROLESTRING "ms?????"

#define DB_FIELD_FLAGS_READONLY 1
#define DB_FIELD_FLAGS_UNIQ 2
#define DB_FIELD_FLAGS_DESC_ORDER 4
#define DB_FIELD_FLAGS_VOLATILE 8
#define DB_FIELD_FLAGS_DISCARD 0x10

/* 默认key-hash so文件名及路径 */
#define DEFAULT_KEY_HASH_SO_NAME "../lib/key-hash.so"
/* key-hash接口函数 */
typedef uint64_t (*key_hash_interface)(const char *key, int len, int left, int right);

enum
{
	INSERT_ORDER_LAST = 0,
	INSERT_ORDER_FIRST = 1,
	INSERT_ORDER_PURGE = 2
};

enum
{
	BY_MASTER,
	BY_SLAVE,
	BY_DB,
	BY_TABLE,
	BY_KEY,
	BY_FIFO
};

typedef enum
{
	DUMMY_HELPER = 0,
	DTC_HELPER,
	MYSQL_HELPER,
	TDB_HELPER,
	CUSTOM_HELPER,
} HELPERTYPE;

struct MachineConfig
{
	struct
	{
		const char *addr;
		const char *user;
		const char *pass;
		const char *optfile;
		//DataMerge Addr
		const char *dm;
		char path[32];
	} role[GROUPS_PER_MACHINE]; // GROUPS_PER_ROLE should be ROLES_PER_MACHINE

	HELPERTYPE helperType;
	int mode;
	uint16_t dbCnt;
	uint16_t procs;
	uint16_t dbIdx[MAXDB_ON_MACHINE];
	uint16_t gprocs[GROUPS_PER_MACHINE];
	uint32_t gqueues[GROUPS_PER_MACHINE];
	bool is_same(MachineConfig *mach);
};

struct FieldConfig
{
	const char *name;
	char type;
	int size;
	DTCValue dval;
	int flags;
	bool is_same(FieldConfig *field);
};

struct KeyHash
{
	int keyHashEnable;
	int keyHashLeftBegin;  /* buff 的左起始位置 */
	int keyHashRightBegin; /* buff 的右起始位置 */
	key_hash_interface keyHashFunction;
};

struct DbConfig
{
	DTCConfig *cfgObj;
	char *dbName;
	char *dbFormat;
	char *tblName;
	char *tblFormat;

	int dstype; /* data-source type: default is mysql   0: mysql  1: gaussdb  2: rocksdb */
	int checkTable;
	unsigned int dbDiv;
	unsigned int dbMod;
	unsigned int tblMod;
	unsigned int tblDiv;
	int fieldCnt;
	int keyFieldCnt;
	int idxFieldCnt;
	int machineCnt;
	int procs;	 //all machine procs total
	int dbMax;	 //max db index
	char depoly; //0:none 1:multiple db 2:multiple table 3:both

	struct KeyHash keyHashConfig;

	int slaveGuard;
	int autoinc;
	int lastacc;
	int lastmod;
	int lastcmod;
	int expireTime;
	int compressflag;
	int ordIns;
	char *ordSql;
	struct FieldConfig *field;
	struct MachineConfig *mach;

	static struct DbConfig *load_buffered(const char *buf);
	static struct DbConfig *Load(const char *file);
	static struct DbConfig *Load(DTCConfig *);
	static bool build_path(char *path, int n, int pid, int group, int role, int type);
	void Destroy(void);
	void dump_db_config(FILE *);
	void set_helper_path(int);
	DTCTableDefinition *build_table_definition(void);
	int load_key_hash(DTCConfig *);
	bool compare_mach(DbConfig *config);
	bool Compare(DbConfig *config, bool compareMach = false);
	int find_new_mach(DbConfig *config, std::vector<int> &newMach, std::map<int, int> &machMap);

private:
	int parse_db_config(DTCConfig *);
};

#endif
