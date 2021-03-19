/*
 * =====================================================================================
 *
 *       Filename:  table_def_manager.h
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
#ifndef __DTC_TABLEDEF_MANAGER_H_
#define __DTC_TABLEDEF_MANAGER_H_

#include "singleton.h"
#include "table_def.h"
#include "dbconfig.h"

class TableDefinitionManager
{
public:
	TableDefinitionManager();
	~TableDefinitionManager();

	static TableDefinitionManager *Instance() { return Singleton<TableDefinitionManager>::Instance(); }
	static void Destroy() { Singleton<TableDefinitionManager>::Destroy(); }

	bool set_new_table_def(DTCTableDefinition *t, int idx);
	bool set_cur_table_def(DTCTableDefinition *t, int idx);
	bool renew_cur_table_def();
	bool save_new_table_conf();

	DTCTableDefinition *get_cur_table_def();
	DTCTableDefinition *get_new_table_def();
	DTCTableDefinition *get_old_table_def();
	int get_cur_table_idx();
	DTCTableDefinition *get_table_def_by_idx(int idx);

	bool build_hot_backup_table_def();
	DTCTableDefinition *get_hot_backup_table_def();

	bool save_db_config();
	DTCTableDefinition *load_buffered_table(const char *buff);
	DTCTableDefinition *load_table(const char *file);

	DTCTableDefinition *table_file_table_def();
	const char *table_file_buffer();
	bool release_table_file_def_and_buffer();
	bool renew_table_file_def(const char *buf, int len);

private:
	int _cur;
	int _new;
	DTCTableDefinition *_def[2];
	DTCTableDefinition *_hotbackup;
	// for cold start
	char *_buf;
	DTCTableDefinition *_table;
	DbConfig *_dbconfig;
	DbConfig *_save_dbconfig;
};

#endif
