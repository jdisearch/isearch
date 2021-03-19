/*
 * =====================================================================================
 *
 *       Filename:  col_expand.cc
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
#include <string.h>
#include <unistd.h>

#include "col_expand.h"
#include "table_def_manager.h"

DTC_USING_NAMESPACE

extern DTCConfig *gConfig;

DTCColExpand::DTCColExpand() : _colExpand(NULL)
{
	memset(_errmsg, 0, sizeof(_errmsg));
}

DTCColExpand::~DTCColExpand()
{
}

int DTCColExpand::Init()
{
	// alloc mem
	size_t size = sizeof(COL_EXPAND_T);
	MEM_HANDLE_T v = M_CALLOC(size);
	if (INVALID_HANDLE == v)
	{
		snprintf(_errmsg, sizeof(_errmsg), "init column expand failed, %s", M_ERROR());
		return -1;
	}
	_colExpand = M_POINTER(COL_EXPAND_T, v);
	_colExpand->expanding = false;
	_colExpand->curTable = 0;
	memset(_colExpand->tableBuff, 0, sizeof(_colExpand->tableBuff));
	// copy file's table.conf to shm
	if (strlen(TableDefinitionManager::Instance()->table_file_buffer()) > COL_EXPAND_BUFF_SIZE)
	{
		snprintf(_errmsg, sizeof(_errmsg), "table buf size bigger than %d", COL_EXPAND_BUFF_SIZE);
		return -1;
	}
	strcpy(_colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM],
		   TableDefinitionManager::Instance()->table_file_buffer());
	// use file's tabledef
	DTCTableDefinition *t = TableDefinitionManager::Instance()->table_file_table_def();
	TableDefinitionManager::Instance()->set_cur_table_def(t, _colExpand->curTable % COL_EXPAND_BUFF_NUM);
	log_debug("init col expand with curTable: %d, tableBuff: %s", _colExpand->curTable, _colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM]);
	return 0;
}

int DTCColExpand::reload_table()
{
	if (TableDefinitionManager::Instance()->get_cur_table_idx() == _colExpand->curTable)
		return 0;

	DTCTableDefinition *t = TableDefinitionManager::Instance()->load_buffered_table(_colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM]);
	if (!t)
	{
		log_error("load shm table.conf error, buf: %s", _colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM]);
		return -1;
	}
	TableDefinitionManager::Instance()->set_cur_table_def(t, _colExpand->curTable);
	return 0;
}

int DTCColExpand::Attach(MEM_HANDLE_T handle, int forceFlag)
{
	if (INVALID_HANDLE == handle)
	{
		log_crit("attch col expand error, handle = 0");
		return -1;
	}
	_colExpand = M_POINTER(COL_EXPAND_T, handle);
	// 1) force update shm mem, 2)replace shm mem by dumped mem
	if (forceFlag)
	{
		log_debug("force use table.conf, not use shm conf");
		if (strlen(TableDefinitionManager::Instance()->table_file_buffer()) > COL_EXPAND_BUFF_SIZE)
		{
			log_error("table.conf to long while force update shm");
			return -1;
		}
		if (_colExpand->expanding)
		{
			log_error("col expanding, can't force update table.conf, delete shm and try again");
			return -1;
		}
		strcpy(_colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM],
			   TableDefinitionManager::Instance()->table_file_buffer());
		DTCTableDefinition *t = TableDefinitionManager::Instance()->table_file_table_def();
		TableDefinitionManager::Instance()->set_cur_table_def(t, _colExpand->curTable);
		return 0;
	}
	// parse shm table.conf
	DTCTableDefinition *t, *tt = NULL;
	t = TableDefinitionManager::Instance()->load_buffered_table(_colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM]);
	if (!t)
	{
		log_error("load shm table.conf error, buf: %s", _colExpand->tableBuff[_colExpand->curTable % COL_EXPAND_BUFF_NUM]);
		return -1;
	}
	if (_colExpand->expanding)
	{
		tt = TableDefinitionManager::Instance()->load_buffered_table(_colExpand->tableBuff[(_colExpand->curTable + 1) % COL_EXPAND_BUFF_NUM]);
		if (!tt)
		{
			log_error("load shm col expand new table.conf error, buf: %s", _colExpand->tableBuff[(_colExpand->curTable + 1) % COL_EXPAND_BUFF_NUM]);
			return -1;
		}
	}
	// compare
	// if not same
	// 		log_error
	if (!t->is_same_table(TableDefinitionManager::Instance()->table_file_table_def()))
	{ // same with hash_equal
		DTCTableDefinition *tt = TableDefinitionManager::Instance()->table_file_table_def();
		log_error("table.conf is not same to shm's");
		log_error("shm table, name: %s, hash: %s", t->table_name(), t->table_hash());
		log_error("file table, name: %s, hash: %s", tt->table_name(), tt->table_hash());
	}
	else
	{
		log_debug("table.conf is same to shm's");
	}
	// use shm's
	TableDefinitionManager::Instance()->set_cur_table_def(t, _colExpand->curTable);
	if (_colExpand->expanding)
		TableDefinitionManager::Instance()->set_new_table_def(tt, _colExpand->curTable + 1);
	return 0;
}

bool DTCColExpand::is_expanding()
{
	return _colExpand->expanding;
}

bool DTCColExpand::expand(const char *table, int len)
{
	_colExpand->expanding = true;
	memcpy(_colExpand->tableBuff[(_colExpand->curTable + 1) % COL_EXPAND_BUFF_NUM], table, len);
	return true;
}

int DTCColExpand::try_expand(const char *table, int len)
{
	if (_colExpand->expanding || len > COL_EXPAND_BUFF_SIZE || _colExpand->curTable > 255)
		return -1;
	return 0;
}

bool DTCColExpand::expand_done()
{
	++_colExpand->curTable;
	_colExpand->expanding = false;
	return true;
}

int DTCColExpand::cur_table_idx()
{
	return _colExpand->curTable;
}
