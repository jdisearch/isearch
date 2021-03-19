/*
 * =====================================================================================
 *
 *       Filename:  admin_tdef.cc
 *
 *    Description:  table definition.
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
#include "log.h"
#include "admin_tdef.h"
#include "table_def.h"
#include "table_def_manager.h"
#include "protocol.h"
#include "dbconfig.h"

static struct FieldConfig HBTabField[] = {
	{"type", DField::Unsigned, 4, DTCValue::Make(0), 0},
	{"flag", DField::Unsigned, 1, DTCValue::Make(0), 0},
	{"key", DField::Binary, 255, DTCValue::Make(0), 0},
	{"value", DField::Binary, MAXPACKETSIZE, DTCValue::Make(0), 0},
};

DTCTableDefinition *build_hot_backup_table(void)
{
	DTCTableDefinition *tdef = new DTCTableDefinition(4);
	tdef->set_table_name("@HOT_BACKUP");
	struct FieldConfig *field = HBTabField;
	int field_cnt = sizeof(HBTabField) / sizeof(HBTabField[0]);

	// build hotback table key info base on the actual user definite table
	field[0].type = TableDefinitionManager::Instance()->get_cur_table_def()->key_type();
	field[0].size = TableDefinitionManager::Instance()->get_cur_table_def()->field_size(0);

	tdef->set_admin_table();
	for (int i = 0; i < field_cnt; i++)
	{
		if (tdef->add_field(i, field[i].name, field[i].type, field[i].size) != 0)
		{
			log_error("add_field failed, name=%s, size=%d, type=%d",
					  field[i].name, field[i].size, field[i].type);
			delete tdef;
			return NULL;
		}
		tdef->set_default_value(i, &field[i].dval);
	}

	if (tdef->set_key_fields(1) < 0)
	{
		log_error("Table key size %d too large, must <= 255",
				  tdef->key_format() > 0 ? tdef->key_format() : -tdef->key_format());
		delete tdef;
		return NULL;
	}
	tdef->build_info_cache();
	return tdef;
}
