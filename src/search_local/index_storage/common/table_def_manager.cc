/*
 * =====================================================================================
 *
 *       Filename:  table_def_manager.cc
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
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "table_def_manager.h"
#include "mem_check.h"
#include "admin_tdef.h"

TableDefinitionManager::TableDefinitionManager()
{
	_cur = 0;
	_new = 0;
	_def[0] = NULL;
	_def[1] = NULL;
}

TableDefinitionManager::~TableDefinitionManager()
{
	DEC_DELETE(_def[0]);
	DEC_DELETE(_def[1]);
}

bool TableDefinitionManager::set_new_table_def(DTCTableDefinition *t, int idx)
{
	_new = idx;
	DEC_DELETE(_def[_new % 2]);
	_def[_new % 2] = t;
	_def[_new % 2]->INC();
	return true;
}

bool TableDefinitionManager::set_cur_table_def(DTCTableDefinition *t, int idx)
{
	_cur = idx;
	DEC_DELETE(_def[_cur % 2]);
	_def[_cur % 2] = t;
	_def[_cur % 2]->INC();
	return true;
}

bool TableDefinitionManager::renew_cur_table_def()
{
	_cur = _new;
	return true;
}

bool TableDefinitionManager::save_new_table_conf()
{
	_save_dbconfig->cfgObj->Dump("../conf/table.conf", false);
	_save_dbconfig->Destroy();
	_save_dbconfig = NULL;
	return true;
}

DTCTableDefinition *TableDefinitionManager::get_cur_table_def()
{
	return _def[_cur % 2];
}

DTCTableDefinition *TableDefinitionManager::get_new_table_def()
{
	return _def[_new % 2];
}

DTCTableDefinition *TableDefinitionManager::get_old_table_def()
{
	return _def[(_cur + 1) % 2];
}

int TableDefinitionManager::get_cur_table_idx()
{
	return _cur;
}

DTCTableDefinition *TableDefinitionManager::get_table_def_by_idx(int idx)
{
	return _def[idx % 2];
}

DTCTableDefinition *TableDefinitionManager::get_hot_backup_table_def()
{
	return _hotbackup;
}

bool TableDefinitionManager::build_hot_backup_table_def()
{
	_hotbackup = build_hot_backup_table();
	if (_hotbackup)
		return true;
	return false;
}

bool TableDefinitionManager::save_db_config()
{
	if (_save_dbconfig)
	{
		log_error("_save_dbconfig not empty, maybe error");
		_save_dbconfig->Destroy();
	}
	_save_dbconfig = _dbconfig;
	_dbconfig = NULL;
	return true;
}

DTCTableDefinition *TableDefinitionManager::load_buffered_table(const char *buf)
{
	DTCTableDefinition *table = NULL;
	char *bufLocal = (char *)MALLOC(strlen(buf) + 1);
	memset(bufLocal, 0, strlen(buf) + 1);
	strcpy(bufLocal, buf);
	if (_dbconfig)
	{
		_dbconfig->Destroy();
		_dbconfig = NULL;
	}
	_dbconfig = DbConfig::load_buffered(bufLocal);
	FREE(bufLocal);
	if (!_dbconfig)
	{
		log_error("new dbconfig error");
		return false;
	}
	table = _dbconfig->build_table_definition();
	if (!table)
		log_error("build table def error");
	return table;
}

DTCTableDefinition *TableDefinitionManager::load_table(const char *file)
{
	char *buf;
	int fd, len, unused;
	DTCTableDefinition *ret;
	if (!file || file[0] == '\0' || (fd = open(file, O_RDONLY)) < 0)
	{
		log_error("open config file error");
		return NULL;
	}
	lseek(fd, 0L, SEEK_SET);
	len = lseek(fd, 0L, SEEK_END);
	lseek(fd, 0L, SEEK_SET);
	_buf = (char *)MALLOC(len + 1);
	buf = (char *)MALLOC(len + 1);
	unused = read(fd, _buf, len);
	_buf[len] = '\0';
	close(fd);
	// should copy one, as load_buffered_table will modify buf
	strncpy(buf, _buf, len);
	buf[len] = '\0';
	log_debug("read file to buf, len: %d, content: %s", len, _buf);
	ret = load_buffered_table(buf);
	if (!ret)
		FREE_CLEAR(_buf);
	FREE_CLEAR(buf);
	_table = ret;
	return ret;
}

DTCTableDefinition *TableDefinitionManager::table_file_table_def()
{
	return _table;
}

const char *TableDefinitionManager::table_file_buffer()
{
	return _buf;
}

bool TableDefinitionManager::release_table_file_def_and_buffer()
{
	FREE_CLEAR(_buf);
	DEC_DELETE(_table);
	return true;
}

bool TableDefinitionManager::renew_table_file_def(const char *buf, int len)
{
	FREE_CLEAR(_buf);
	_buf = (char *)malloc(len + 1);
	strncpy(_buf, buf, len);
	_buf[len] = '\0';
	return true;
}
