/*
 * =====================================================================================
 *
 *       Filename:  dbconfig.cc
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dlfcn.h>

#include "value.h"
#include "protocol.h"
#include "log.h"
#include "dbconfig.h"
#include "table_def.h"
#include "config.h"
#include <libgen.h>

#define HELPERPATHFORMAT "@dtcd[%d]helper%d%c"
#define DTC_EXPIRE_TIME_FIELD "_dtc_sys_expiretime"

static int
ParseDbLine(const char *buf, uint16_t *dbIdx)
{
	char *p = (char *)buf; // discard const
	if (*p++ != '[')
		return -1;

	int n = 0;
	while (*p)
	{
		if (!isdigit(p[0]))
			break;
		int begin, end;
		begin = strtol(p, &p, 0);
		if (*p != '-')
			end = begin;
		else
		{
			p++;
			if (!isdigit(p[0]))
				break;
			end = strtol(p, &p, 0);
		}
		while (begin <= end)
			dbIdx[n++] = begin++;

		if (p[0] != ',')
			break;
		else
			p++;
	}
	if (p[0] != ']')
		return -1;
	return n;
}

/* 前置空格已经过滤了 */
static char *skip_blank(char *p)
{
	char *iter = p;
	while (!isspace(*iter) && *iter != '\0')
		++iter;

	*iter = '\0';
	return p;
}

bool MachineConfig::is_same(MachineConfig *mach)
{
	// no need check NULL pointer as NULL is '\0'
	if (strcmp(this->role[0].addr, mach->role[0].addr))
		return false;
	if (strcmp(this->role[0].user, mach->role[0].user))
		return false;
	if (strcmp(this->role[0].pass, mach->role[0].pass))
		return false;
	if (mode != mach->mode)
		return false;
	if (mode > 0)
	{
		if (strcmp(this->role[1].addr, mach->role[1].addr))
			return false;
		if (strcmp(this->role[1].user, mach->role[1].user))
			return false;
		if (strcmp(this->role[1].pass, mach->role[1].pass))
			return false;
	}
	return true;
}

bool FieldConfig::is_same(FieldConfig *field)
{
	if (type != field->type &&
		size != field->size &&
		flags != field->flags &&
		strcmp(name, field->name))
		return false;
	switch (type)
	{
	case DField::Signed:
	case DField::Unsigned:
	case DField::Float:
		if (dval.u64 != field->dval.u64)
			return false;
		break;
	case DField::String:
	case DField::Binary:
		if (dval.bin.len != field->dval.bin.len)
			return false;
		if (dval.bin.len == 0)
			return true;
		if (memcmp(dval.bin.ptr, field->dval.bin.ptr, dval.bin.len) != 0)
			return false;
		break;
	default:
		return false;
	}
	return true;
}

int DbConfig::load_key_hash(DTCConfig *raw)
{
	log_debug("tryto load key-hash plugin");

	/* init. */
	keyHashConfig.keyHashEnable = 0;
	keyHashConfig.keyHashFunction = 0;
	keyHashConfig.keyHashLeftBegin = 0;
	keyHashConfig.keyHashRightBegin = 0;

	/* not enable key-hash */
	if (raw->get_int_val("DB_DEFINE", "EnableKeyHash", 0) == 0)
	{
		log_debug("key-hash plugin disable");
		return 0;
	}

	/* read KeyHashSo */
	const char *so = raw->get_str_val("DB_DEFINE", "KeyHashSo");
	if (NULL == so || 0 == so[0])
	{
		log_notice("not set key-hash plugin name, use default value");
		so = (char *)DEFAULT_KEY_HASH_SO_NAME;
	}

	/* read KeyHashFunction */
	const char *var = raw->get_str_val("DB_DEFINE", "KeyHashFunction");
	if (NULL == var || 0 == var[0])
	{
		log_error("not set key-hash plugin function name");
		return -1;
	}

	char *fun = 0;
	int isfunalloc = 0;
	const char *iter = strchr(var, '(');
	if (NULL == iter)
	{
		/* 
		 * 按照整个buffer来处理
		 */
		keyHashConfig.keyHashLeftBegin = 1;
		keyHashConfig.keyHashRightBegin = -1;
		fun = (char *)var;
	}
	else
	{

		fun = strndup(var, (size_t)(iter - var));
		isfunalloc = 1;
		if (sscanf(iter, "(%d, %d)",
				   &keyHashConfig.keyHashLeftBegin,
				   &keyHashConfig.keyHashRightBegin) != 2)
		{
			free(fun);
			log_error("key-hash plugin function format error, %s:%s", so, var);
			return -1;
		}

		if (keyHashConfig.keyHashLeftBegin == 0)
			keyHashConfig.keyHashLeftBegin = 1;
		if (keyHashConfig.keyHashRightBegin == 0)
			keyHashConfig.keyHashRightBegin = -1;
	}

	/* 过滤fun中的空格*/
	fun = skip_blank(fun);

	void *dll = dlopen(so, RTLD_NOW | RTLD_GLOBAL);
	if (dll == (void *)NULL)
	{
		if (isfunalloc)
			free(fun);
		log_error("dlopen(%s) error: %s", so, dlerror());
		return -1;
	}

	/* find key-hash function in key-hash.so */
	keyHashConfig.keyHashFunction = (key_hash_interface)dlsym(dll, fun);
	if (keyHashConfig.keyHashFunction == NULL)
	{
		log_error("key-hash plugin function[%s] not found in [%s]", fun, so);
		if (isfunalloc)
			free(fun);
		return -1;
	}

	/* check passed， enable key-hash */
	keyHashConfig.keyHashEnable = 1;

	log_notice("key-hash plugin %s->%s(%d, %d) %s", basename((char *)so), fun,
			   keyHashConfig.keyHashLeftBegin,
			   keyHashConfig.keyHashRightBegin,
			   keyHashConfig.keyHashEnable ? "enable" : "disable");

	if (isfunalloc)
		free(fun);

	return 0;
}

bool DbConfig::compare_mach(DbConfig *config)
{
	bool found = true;
	if (this->machineCnt != config->machineCnt)
		return false;
	for (int i = 0; found && i < this->machineCnt; ++i)
	{
		found = false;
		for (int j = 0; j < config->machineCnt; ++j)
		{
			if (this->mach[i].is_same(config->mach + j))
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

bool DbConfig::Compare(DbConfig *config, bool compareMach)
{
	if (compareMach && !compare_mach(config))
		return false;

	if (depoly != config->depoly ||
		keyFieldCnt != config->keyFieldCnt ||
		idxFieldCnt != config->idxFieldCnt ||
		fieldCnt != config->fieldCnt ||
		dbMax != config->dbMax ||
		dbDiv != config->dbDiv ||
		dbMod != config->dbMod ||
		tblMod != config->tblMod ||
		tblDiv != config->tblDiv)
	{
		log_error("before origin after param, depoly:[%d,%d],keyFieldCnt:[%d,%d],idxFieldCnt:[%d,%d],fieldCnt:[%d,%d],dbMax:[%d,%d],dbDiv:[%d,%d],dbMod:[%d,%d],tblMod:[%d,%d],tblDiv:[%d,%d]",
				  depoly, config->depoly,
				  keyFieldCnt, config->keyFieldCnt,
				  idxFieldCnt, config->idxFieldCnt,
				  fieldCnt, config->fieldCnt,
				  dbMax, config->dbMax,
				  dbDiv, config->dbDiv,
				  dbMod, config->dbMod,
				  tblMod, config->tblMod,
				  tblDiv, config->tblDiv);
		return false;
	}

	for (int i = 0; i < fieldCnt; ++i)
	{
		if (!(field + i)->is_same(config->field + i))
		{
			log_error("field%d does not match", i + 1);
			return false;
		}
	}

	return true;
}

int DbConfig::find_new_mach(DbConfig *config, std::vector<int> &newMach, std::map<int, int> &machMap)
{
	// be careful, same machine numbers, but change db distribution with new machine cause bug
	if (this->machineCnt == config->machineCnt)
		return 0;
	if (newMach.size() > 0)
		newMach.clear();
	if (machMap.size() > 0)
		machMap.clear();
	std::map<int, int> old2New;
	for (int i = 0; i < this->machineCnt; ++i)
	{
		bool found = false;
		for (int j = 0; j < config->machineCnt; ++j)
		{
			if (config->mach[j].is_same(this->mach + i))
			{
				if (old2New.find(j) != old2New.end())
				{
					log_error("multi new machines to one old machine");
					return 0;
				}
				found = true;
				machMap[i] = j;
				old2New[j] = i;
				break;
			}
		}
		if (!found)
		{
			newMach.push_back(i);
			log_debug("found new db machine No. %d", i + 1);
		}
	}
	return newMach.size();
}

int DbConfig::parse_db_config(DTCConfig *raw)
{
	char buf[1024];
	char sectionStr[256];
	const char *cp;
	char *p;

	//DB section
	machineCnt = raw->get_int_val("DB_DEFINE", "MachineNum", 1);
	if (machineCnt <= 0)
	{
		log_error("%s", "invalid MachineNum");
		return -1;
	}
	depoly = raw->get_int_val("DB_DEFINE", "Deploy", 0);

	cp = raw->get_str_val("DB_DEFINE", "DbName");
	if (cp == NULL || cp[0] == '\0')
	{
		log_error("[DB_DEFINE].DbName not defined");
		return -1;
	}

	dbName = STRDUP(cp);

	dstype = raw->get_int_val("DB_DEFINE", "DataSourceType", 0);

	checkTable = raw->get_int_val("DB_DEFINE", "CheckTableConfig", 1);

	// key-hash dll
	if (load_key_hash(raw) != 0)
		return -1;

	if ((depoly & 1) == 0)
	{
		if (strchr(dbName, '%') != NULL)
		{
			log_error("Invalid [DB_DEFINE].DbName, cannot contain symbol '%%'");
			return -1;
		}

		dbDiv = 1;
		dbMod = 1;
		dbFormat = dbName;
	}
	else
	{
		cp = raw->get_str_val("DB_DEFINE", "DbNum");
		if (sscanf(cp ?: "", "(%u,%u)", &dbDiv, &dbMod) != 2 || dbDiv == 0 || dbMod == 0)
		{
			log_error("invalid [DB_DEFINE].DbNum = %s", cp);
			return -1;
		}

		if (dbMod > 1000000)
		{
			log_error("invalid [DB_DEFINE].DbMod = %s, mod value too large", cp);
			return -1;
		}

		p = strchr(dbName, '%');
		if (p)
		{
			dbFormat = STRDUP(dbName);
			*p = '\0';
		}
		else
		{
			dbFormat = (char *)MALLOC(strlen(dbName) + 3);
			snprintf(dbFormat, strlen(dbName) + 3, "%s%%d", dbName);
		}
	}

	// super group
	int enableSuperMach = raw->get_int_val("SUPER_MACHINE", "Enable", 0);
	if (enableSuperMach)
	{
		log_error("SUPER_MACHINE don't support anymore, please change to HelperType=DTC");
		return -1;
	}

	//Table section
	cp = raw->get_str_val("TABLE_DEFINE", "TableName");
	if (cp == NULL || cp[0] == '\0')
	{
		log_error("[TABLE_DEFINE].TableName not defined");
		return -1;
	}
	tblName = STRDUP(cp);

	if ((depoly & 2) == 0)
	{
		if (strchr(tblName, '%') != NULL)
		{
			log_error("Invalid TableName, cannot contain symbol '%%'");
			return -1;
		}

		tblDiv = 1;
		tblMod = 1;
		tblFormat = tblName;
	}
	else
	{
		cp = raw->get_str_val("TABLE_DEFINE", "TableNum");
		if (sscanf(cp ?: "", "(%u,%u)", &tblDiv, &tblMod) != 2 || tblDiv == 0 || tblMod == 0)
		{
			log_error("invalid [TABLE_DEFINE].TableNum = %s", cp);
			return -1;
		}

		p = strchr(tblName, '%');
		if (p)
		{
			tblFormat = STRDUP(tblName);
			*p = '\0';
		}
		else
		{
			tblFormat = (char *)MALLOC(strlen(tblName) + 3);
			snprintf(tblFormat, strlen(tblName) + 3, "%s%%d", tblName);
		}
	}

	dbMax = raw->get_int_val("DB_DEFINE", "dbMax", 1);
	if (dbMax < 0 || dbMax > 10000)
	{
		log_error("%s", "invalid [DB_DEFINE].DbMax");
		return -1;
	}
	if (dbMax < (int)dbMod)
	{
		log_warning("invalid [TABLE_DEFINE].DbMax too small, increase to %d", dbMod);
		dbMax = dbMod;
	}

	fieldCnt = raw->get_int_val("TABLE_DEFINE", "FieldCount", 0);
	if (fieldCnt <= 0 || fieldCnt > 240)
	{
		log_error("invalid [TABLE_DEFINE].FieldCount");
		return -1;
	}

	keyFieldCnt = raw->get_int_val("TABLE_DEFINE", "KeyFieldCount", 1);
	if (keyFieldCnt <= 0 || keyFieldCnt > 32 || keyFieldCnt > fieldCnt)
	{
		log_error("invalid [TABLE_DEFINE].KeyFieldCount");
		return -1;
	}

	idxFieldCnt = raw->get_int_val("TABLE_DEFINE", "IndexFieldCount", 0);
	if (keyFieldCnt < 0 || keyFieldCnt + idxFieldCnt > fieldCnt)
	{
		log_error("invalid [TABLE_DEFINE].IndexFieldCount");
		return -1;
	}

	cp = raw->get_str_val("TABLE_DEFINE", "ServerOrderBySQL");
	if (cp && cp[0] != '\0')
	{
		int n = strlen(cp) - 1;
		if (cp[0] != '"' || cp[n] != '"')
		{
			log_error("dbConfig error, [TABLE_DEFINE].ServerOrderBySQL must quoted");
			return -1;
		}
		ordSql = (char *)MALLOC(n);
		memcpy(ordSql, cp + 1, n - 1);
		ordSql[n - 1] = '\0';
	}
	ordIns = raw->get_idx_val("TABLE_DEFINE", "ServerOrderInsert",
							  ((const char *const[]){"last", "first", "purge", NULL}), 0);
	if (ordIns < 0)
	{
		log_error("bad [TABLE_DEFINE].ServerOrderInsert");
		return -1;
	}

	//Machine setction
	mach = (struct MachineConfig *)calloc(machineCnt, sizeof(struct MachineConfig));
	if (!mach)
	{
		log_error("malloc failed, %m");
		return -1;
	}
	for (int i = 0; i < machineCnt; i++)
	{
		struct MachineConfig *m = &mach[i];

		snprintf(sectionStr, sizeof(sectionStr), "MACHINE%d", i + 1);
		m->helperType = (HELPERTYPE)raw->get_idx_val(sectionStr, "HelperType",
													 ((const char *const[]){"DUMMY", "DTC", "MYSQL", "TDB", "CUSTOM", NULL}), 2);

		if (m->helperType == DUMMY_HELPER)
		{
			m->role[0].addr = "DUMMY";
		}
		else
		{
			//master
			m->role[0].addr = raw->get_str_val(sectionStr, "DbAddr");
			m->role[0].user = raw->get_str_val(sectionStr, "DbUser");
			m->role[0].pass = raw->get_str_val(sectionStr, "DbPass");
			m->role[0].optfile = raw->get_str_val(sectionStr, "MyCnf");

			//slave
			m->role[1].addr = raw->get_str_val(sectionStr, "DbAddr1");
			m->role[1].user = raw->get_str_val(sectionStr, "DbUser1");
			m->role[1].pass = raw->get_str_val(sectionStr, "DbPass1");
			m->role[1].optfile = raw->get_str_val(sectionStr, "MyCnf");

			/* master DB settings */
			if (m->role[0].addr == NULL)
			{
				log_error("missing [%s].DbAddr", sectionStr);
				return -1;
			}
			if (m->role[0].user == NULL)
				m->role[0].user = "";
			if (m->role[0].pass == NULL)
				m->role[0].pass = "";
			if (m->role[0].optfile == NULL)
				m->role[0].optfile = "";

			m->mode = raw->get_idx_val(sectionStr, "Workload",
									   ((const char *const[]){"master", "slave", "database", "table", "key", /*"fifo",*/ NULL}), 0);

			if (m->mode < 0)
			{
				log_error("bad [%s].Workload", sectionStr);
				return -1;
			}
			if (m->mode > 0)
			{
				if (m->role[1].addr == NULL)
				{
					log_error("missing [%s].DbAddr1", sectionStr);
					return -1;
				}
				if (m->role[1].user == NULL)
					m->role[1].user = m->role[0].user;
				if (m->role[1].pass == '\0')
					m->role[1].pass = m->role[0].pass;
				if (m->role[1].optfile == NULL)
					m->role[1].optfile = m->role[0].optfile;
			}
		}

		cp = raw->get_str_val(sectionStr, "DbIdx");
		if (!cp || !cp[0])
			cp = "[0-0]";

		m->dbCnt = ParseDbLine(cp, m->dbIdx);

		for (int j = 0; j < m->dbCnt; j++)
		{
			if (m->dbIdx[j] >= dbMax)
			{
				log_error("dbConfig error, dbMax=%d, machine[%d].dbIdx=%d",
						  dbMax, j + 1, m->dbIdx[j]);
				return -1;
			}
		}

		/* Helper number alter */
		m->gprocs[0] = raw->get_int_val(sectionStr, "Procs", 0);
		if (m->gprocs[0] < 1)
			m->gprocs[0] = 0;
		m->gprocs[1] = raw->get_int_val(sectionStr, "WriteProcs", 0);
		if (m->gprocs[1] < 1)
			m->gprocs[1] = 0;
		m->gprocs[2] = raw->get_int_val(sectionStr, "CommitProcs", 0);
		if (m->gprocs[2] < 1)
			m->gprocs[2] = 0;

		/* Helper Queue Size */
		m->gqueues[0] = raw->get_int_val(sectionStr, "QueueSize", 0);
		m->gqueues[1] = raw->get_int_val(sectionStr, "WriteQueueSize", 0);
		m->gqueues[2] = raw->get_int_val(sectionStr, "CommitQueueSize", 0);
		if (m->gqueues[0] <= 0)
			m->gqueues[0] = 1000;
		if (m->gqueues[1] <= 0)
			m->gqueues[1] = 1000;
		if (m->gqueues[2] <= 0)
			m->gqueues[2] = 10000;
		if (m->gqueues[0] <= 2)
			m->gqueues[0] = 2;
		if (m->gqueues[1] <= 2)
			m->gqueues[1] = 2;
		if (m->gqueues[2] <= 1000)
			m->gqueues[2] = 1000;

		if (m->dbCnt == 0) // DbIdx is NULL, no helper needed
		{
			m->gprocs[0] = 0;
			m->gprocs[1] = 0;
			m->gprocs[2] = 0;
			m->mode = 0;
		}

		switch (m->mode)
		{
		case BY_SLAVE:
		case BY_DB:
		case BY_TABLE:
		case BY_KEY:
			m->gprocs[3] = m->gprocs[0];
			m->gqueues[3] = m->gqueues[0];
			if (slaveGuard == 0)
			{
				slaveGuard = raw->get_int_val("DB_DEFINE", "SlaveGuardTime", 15);
				if (slaveGuard < 5)
					slaveGuard = 5;
			}
			break;
		default:
			m->gprocs[3] = 0;
			m->gqueues[3] = 0;
		}

		m->procs = m->gprocs[0] + m->gprocs[1] + m->gprocs[2];
		procs += m->procs;
	}

	//Field section
	field = (struct FieldConfig *)calloc(fieldCnt, sizeof(struct FieldConfig));
	if (!field)
	{
		log_error("malloc failed, %m");
		return -1;
	}
	autoinc = -1;
	lastmod = -1;
	lastcmod = -1;
	lastacc = -1;
	compressflag = -1;
	expireTime = -1;
	for (int i = 0; i < fieldCnt; i++)
	{
		struct FieldConfig *f = &field[i];

		snprintf(sectionStr, sizeof(sectionStr), "FIELD%d", i + 1);
		f->name = raw->get_str_val(sectionStr, "FieldName");
		if (f->name == NULL)
		{
			log_error("field name missing for %s", sectionStr);
			return -1;
		}
		f->type = raw->get_int_val(sectionStr, "FieldType", -1);
		if (f->type < 0)
		{
			f->type = raw->get_idx_val(sectionStr, "FieldType",
									   ((const char *const[]){
										   "int",
										   "signed",
										   "unsigned",
										   "float",
										   "string",
										   "binary",
										   NULL}),
									   -1);
			if (f->type == 0)
				f->type = 1;
		}
		if (f->type <= 0 || f->type > 5)
		{
			log_error("Invalid value [%s].FieldType", sectionStr);
			return -1;
		}
		f->size = raw->get_int_val(sectionStr, "FieldSize", -1);
		if (f->size == -1)
		{
			log_error("field size missing for %s", sectionStr);
			return -1;
		}

		if (i >= keyFieldCnt && i < keyFieldCnt + idxFieldCnt)
		{ // index field
			if (field[i].size > 255)
			{
				log_error("index field[%s] size must less than 256", f->name);
				return -1;
			}
			const char *idx_order = raw->get_str_val(sectionStr, "Order");
			if (idx_order && strcasecmp(idx_order, "DESC") == 0)
				f->flags |= DB_FIELD_FLAGS_DESC_ORDER;
			else
				log_debug("index field[%s] order: ASC", f->name);
		}
		if (field[i].size >= (64 << 20))
		{
			log_error("field[%s] size must less than 64M", f->name);
			return -1;
		}

		if (i >= keyFieldCnt && raw->get_int_val(sectionStr, "ReadOnly", 0) > 0)
			f->flags |= DB_FIELD_FLAGS_READONLY;
		if (raw->get_int_val(sectionStr, "UniqField", 0) > 0)
			f->flags |= DB_FIELD_FLAGS_UNIQ;
		if (raw->get_int_val(sectionStr, "Volatile", 0) > 0)
		{
			if (i < keyFieldCnt)
			{
				log_error("field%d: key field can't be volatile", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: uniq field can't be volatile", i + 1);
				return -1;
			}
			f->flags |= DB_FIELD_FLAGS_VOLATILE;
		}
		if (raw->get_int_val(sectionStr, "Discard", 0) > 0)
		{
			if (i < keyFieldCnt)
			{
				log_error("field%d: key field can't be discard", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: uniq field can't be discard", i + 1);
				return -1;
			}
			f->flags |= DB_FIELD_FLAGS_DISCARD | DB_FIELD_FLAGS_VOLATILE;
		}

		if (!strcmp(f->name, DTC_EXPIRE_TIME_FIELD))
		{
			if (f->type != DField::Unsigned && f->type != DField::Signed)
			{
				log_error("field%d: expire time field byte must be unsigned", i + 1);
				return -1;
			}
			if (expireTime >= 0)
			{
				log_error("field%d already defined as expire time", expireTime + 1);
				return -1;
			}
			expireTime = i;
		}

		/* ATTN: must be last one */
		cp = raw->get_str_val(sectionStr, "DefaultValue");
		if (cp && !strcmp(cp, "auto_increment"))
		{
			if (f->type != DField::Unsigned && f->type != DField::Signed)
			{
				log_error("field%d: auto_increment field byte must be unsigned", i + 1);
				return -1;
			}
			if (autoinc >= 0)
			{
				log_error("field%d already defined as auto_increment", autoinc + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_DISCARD))
			{
				log_error("field%d: auto_increment can't be Discard", autoinc + 1);
				return -1;
			}
			autoinc = i;
			f->flags |= DB_FIELD_FLAGS_READONLY;
		}
		else if (cp && !strcmp(cp, "lastmod"))
		{
			if (i < keyFieldCnt)
			{
				log_error("key field%d can't be lastmod", i + 1);
				return -1;
			}
			if ((f->type != DField::Unsigned && f->type != DField::Signed) || f->size < 4)
			{
				log_error("field%d: lastmod field byte must be unsigned & size>=4", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: lastmod field byte can't be UniqField", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_DISCARD))
			{
				log_error("field%d: lastmod can't be Discard", autoinc + 1);
				return -1;
			}
			if (lastmod >= 0)
			{
				log_error("field%d already defined as lastmod", lastmod + 1);
				return -1;
			}
			lastmod = i;
		}
		else if (cp && !strcmp(cp, "lastcmod"))
		{
			if (i < keyFieldCnt)
			{
				log_error("key field%d can't be lastcmod", i + 1);
				return -1;
			}
			if ((f->type != DField::Unsigned && f->type != DField::Signed) || f->size < 4)
			{
				log_error("field%d: lastcmod field byte must be unsigned & size>=4", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: lastcmod field byte can't be UniqField", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_DISCARD))
			{
				log_error("field%d: lastcmod can't be Discard", autoinc + 1);
				return -1;
			}
			if (lastcmod >= 0)
			{
				log_error("field%d already defined as lastcmod", lastcmod + 1);
				return -1;
			}
			lastcmod = i;
		}
		else if (cp && !strcmp(cp, "lastacc"))
		{
			if (i < keyFieldCnt)
			{
				log_error("key field%d can't be lastacc", i + 1);
				return -1;
			}
			if ((f->type != DField::Unsigned && f->type != DField::Signed) || f->size != 4)
			{
				log_error("field%d: lastacc field byte must be unsigned & size==4", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: lastacc field byte can't be UniqField", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_DISCARD))
			{
				log_error("field%d: lastacc can't be Discard", autoinc + 1);
				return -1;
			}
			if (lastacc >= 0)
			{
				log_error("field%d already defined as lastacc", lastacc + 1);
				return -1;
			}
			lastacc = i;
		}
		else if (cp && !strcmp(cp, "compressflag"))
		{
			if (i < keyFieldCnt)
			{
				log_error("key field%d can't be compressflag", i + 1);
				return -1;
			}
			if ((f->type != DField::Unsigned && f->type != DField::Signed) || f->size != 8)
			{
				log_error("field%d: compressflag field byte must be unsigned & size==8", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_UNIQ))
			{
				log_error("field%d: compressflag field byte can't be UniqField", i + 1);
				return -1;
			}
			if ((f->flags & DB_FIELD_FLAGS_DISCARD))
			{
				log_error("field%d: compressflag can't be Discard", i + 1);
				return -1;
			}
			if (compressflag >= 0)
			{
				log_error("field%d already defined as compressflag", i + 1);
				return -1;
			}
			compressflag = i;
		}
		else
		{
			if (i == 0 && cp)
			{
				log_error("specify DefaultValue for Field1 is invalid");
				return -1;
			}
			switch (f->type)
			{
			case DField::Unsigned:
				f->dval.Set(!cp || !cp[0] ? 0 : (uint64_t)strtoull(cp, NULL, 0));
				break;
			case DField::Signed:
				f->dval.Set(!cp || !cp[0] ? 0 : (int64_t)strtoll(cp, NULL, 0));
				break;
			case DField::Float:
				f->dval.Set(!cp || !cp[0] ? 0.0 : strtod(cp, NULL));
				break;
			case DField::String:
			case DField::Binary:
				int len;
				if (!cp || !cp[0])
				{
					f->dval.Set(NULL, 0);
					break;
				}
				else if (cp[0] == '"')
				{
					/* Decode quoted string */
					cp++;
					len = strlen(cp);
					if (cp[len - 1] != '"')
					{
						log_error("field%d: unmatched quoted default value", i + 1);
						return -1;
					}
					len--;
				}
				else if (cp[0] == '0' && (cp[1] | 0x20) == 'x')
				{
					/* Decode hex value */
					int j = 2;
					len = 0;
					while (cp[j])
					{
						char v;
						if (cp[j] == '-')
							continue;
						if (cp[j] >= '0' && cp[j] <= '9')
							v = cp[j] - '0';
						else if (cp[j] >= 'a' && cp[j] <= 'f')
							v = cp[j] - 'a' + 10;
						else if (cp[j] >= 'A' && cp[j] <= 'F')
							v = cp[j] - 'A' + 10;
						else
						{
							log_error("field%d: invalid hex default value", i + 1);
							return -1;
						}
						j++;
						if (cp[j] >= '0' && cp[j] <= '9')
							v = (v << 4) + cp[j] - '0';
						else if (cp[j] >= 'a' && cp[j] <= 'f')
							v = (v << 4) + cp[j] - 'a' + 10;
						else if (cp[j] >= 'A' && cp[j] <= 'F')
							v = (v << 4) + cp[j] - 'A' + 10;
						else
						{
							log_error("field%d: invalid hex default value", i + 1);
							return -1;
						}
						j++;
						buf[len++] = v;
					}
				}
				else
				{
					log_error("field%d: string default value must quoted or hex value", i + 1);
					return -1;
				}

				if (len > f->size)
				{
					log_error("field%d: default value size %d truncated to %d",
							  i + 1, len, f->size);
					return -1;
				}
				if (len == 0)
					f->dval.Set(NULL, 0);
				else
				{
					char *p = (char *)MALLOC(len + 1);
					memcpy(p, cp, len);
					p[len] = '\0';
					f->dval.Set(p, len);
				}
				break;
			}
		}
	}

	if (field[0].type == DField::Float)
	{
		log_error("%s", "FloatPoint key not supported");
		return -1;
	}

	if (((field[0].type == DField::String || field[0].type == DField::Binary) && keyHashConfig.keyHashEnable == 0) ||
		autoinc == 0)
	{
		if (machineCnt != 1)
		{
			log_error("%s", "String/Binary/AutoInc key require MachineNum==1");
			return -1;
		}
		if (dbMax != 1)
		{
			log_error("%s", "String/Binary/AutoInc key require dbMax==1");
			return -1;
		}
		if (depoly != 0)
		{
			log_error("%s", "String/Binary/AutoInc key require Depoly==0");
			return -1;
		}
	}

	// expire time only support uniq key
	if (expireTime != -1 && !(field[0].flags & DB_FIELD_FLAGS_UNIQ))
	{
		log_error("%s", "expire time only support uniq key");
		return -1;
	}

	if (keyFieldCnt > 1)
	{
		for (int j = 0; j < keyFieldCnt; j++)
		{
			struct FieldConfig *f1 = &field[j];
			if (f1->type != DField::Signed && f1->type != DField::Unsigned)
			{
				log_error("%s", "Only Multi-Integer-Key supported");
				return -1;
			}
		}
	}

	return 0;
}

struct DbConfig *DbConfig::load_buffered(const char *buf)
{
	DTCConfig *raw = new DTCConfig();
	if (raw->parse_buffered_config(buf) < 0)
	{
		delete raw;
		return NULL;
	}

	DbConfig *dbConfig = (struct DbConfig *)calloc(1, sizeof(struct DbConfig));
	dbConfig->cfgObj = raw;

	if (dbConfig->parse_db_config(raw) == -1)
	{
		dbConfig->Destroy();
		dbConfig = NULL;
	}

	return dbConfig;
}

struct DbConfig *DbConfig::Load(const char *file)
{
	DTCConfig *raw = new DTCConfig();
	if (raw->parse_config(file) < 0)
	{
		delete raw;
		return NULL;
	}

	DbConfig *dbConfig = (struct DbConfig *)calloc(1, sizeof(struct DbConfig));
	dbConfig->cfgObj = raw;

	if (dbConfig->parse_db_config(raw) == -1)
	{
		dbConfig->Destroy();
		dbConfig = NULL;
	}

	return dbConfig;
}

struct DbConfig *DbConfig::Load(DTCConfig *raw)
{
	DbConfig *dbConfig = (struct DbConfig *)calloc(1, sizeof(struct DbConfig));
	dbConfig->cfgObj = raw;

	if (dbConfig->parse_db_config(raw) == -1)
	{
		dbConfig->Destroy();
		dbConfig = NULL;
	}

	return dbConfig;
}

bool DbConfig::build_path(char *path, int n, int pid, int group, int role, int type)
{
	if (type == DUMMY_HELPER || type == DTC_HELPER)
		return false;
	memset(path, 0, n);
	snprintf(path, n - 1, HELPERPATHFORMAT, pid, group, MACHINEROLESTRING[role]);
	return true;
}

void DbConfig::set_helper_path(int serverId)
{
	//multi DTC datasource
	for (int i = 0; i < machineCnt; i++)
	{
		MachineConfig *m = &mach[i];
		for (int j = 0; j < ROLES_PER_MACHINE; j++)
		{
			//check dtc datasource
			switch (m->helperType)
			{
			case DUMMY_HELPER:
				break;
			case DTC_HELPER:
				snprintf(m->role[j].path, sizeof(m->role[j].path) - 1, "%s", m->role[j].addr);
				log_info("build helper path, serverId:%d, addr:%s, path:%s, dstype:%d", serverId, m->role[j].addr, m->role[j].path, dstype);
				break;
			default:
				snprintf(m->role[j].path, sizeof(m->role[j].path) - 1, HELPERPATHFORMAT, serverId, i, MACHINEROLESTRING[j]);
				log_info("build helper path, serverId:%d, addr:%s, path:%s, dstype:%d", serverId, m->role[j].addr, m->role[j].path, dstype);
			}

			// rocksdb only start one process
			if (dstype == 2)
				break;
		}
	}
}

void DbConfig::Destroy(void)
{
	if (this == NULL)
		return;

	// machine hasn't dynamic objects
	FREE_IF(mach);

	if (field)
	{
		for (int i = 0; i < fieldCnt; i++)
		{
			switch (field[i].type)
			{
			case DField::String:
			case DField::Binary:
				if (field[i].dval.str.ptr)
					FREE(field[i].dval.str.ptr);
			}
		}
		FREE(field);
	}

	if (dbFormat != dbName)
		FREE_IF(dbFormat);
	FREE_IF(dbName);
	if (tblFormat != tblName)
		FREE(tblFormat);
	FREE_IF(tblName);
	FREE_IF(ordSql);
	DELETE(cfgObj);
	FREE((void *)this);
}

void dump_db_config(const struct DbConfig *cf)
{
	int i, j;

	printf("DbName: %s\n", cf->dbName);
	printf("DbNum: (%d,%d)\n", cf->dbDiv, cf->dbMod);
	printf("MachineNum: %d\n", cf->machineCnt);
	for (i = 0; i < cf->machineCnt; i++)
	{
		struct MachineConfig *mach = &cf->mach[i];
		printf("\n");
		printf("Machine[%d].addr: %s\n", i, mach->role[0].addr);
		printf("Machine[%d].user: %s\n", i, mach->role[0].user);
		printf("Machine[%d].pass: %s\n", i, mach->role[0].pass);
		printf("Machine[%d].procs: %d\n", i, mach->procs);
		printf("Machine[%d].dbCnt: %d\n", i, mach->dbCnt);
		printf("Machine[%d].dbIdx: ", i);
		for (j = 0; j < mach->dbCnt; j++)
			printf("%d ", mach->dbIdx[j]);
		printf("\n");
	}
	printf("\nTableName: %s\n", cf->tblName);
	printf("TableNum: (%d,%d)\n", cf->tblDiv, cf->tblMod);
	printf("FieldCount: %d key %d\n", cf->fieldCnt, cf->keyFieldCnt);
	for (i = 0; i < cf->fieldCnt; i++)
		printf("Field[%d].name: %s, size: %d, type: %d\n", i,
			   cf->field[i].name, cf->field[i].size, cf->field[i].type);
}
