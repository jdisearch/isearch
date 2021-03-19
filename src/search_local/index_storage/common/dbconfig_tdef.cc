/*
 * =====================================================================================
 *
 *       Filename:  dbconfig_tdef.cc
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
#include "log.h"
#include "dbconfig.h"
#include "table_def.h"

DTCTableDefinition *DbConfig::build_table_definition(void)
{
	DTCTableDefinition *tdef = new DTCTableDefinition(fieldCnt);
	tdef->set_table_name(tblName);
	for (int i = 0; i < fieldCnt; i++)
	{
		if (tdef->add_field(i, field[i].name, field[i].type, field[i].size) != 0)
		{
			log_error("add_field failed, name=%s, size=%d, type=%d",
					  field[i].name, field[i].size, field[i].type);
			delete tdef;
			return NULL;
		}
		tdef->set_default_value(i, &field[i].dval);
		if ((field[i].flags & DB_FIELD_FLAGS_READONLY))
			tdef->mark_as_read_only(i);
		if ((field[i].flags & DB_FIELD_FLAGS_VOLATILE))
			tdef->mark_as_volatile(i);
		if ((field[i].flags & DB_FIELD_FLAGS_DISCARD))
			tdef->mark_as_discard(i);
		if ((field[i].flags & DB_FIELD_FLAGS_UNIQ))
			tdef->mark_uniq_field(i);
		if ((field[i].flags & DB_FIELD_FLAGS_DESC_ORDER))
		{
			tdef->mark_order_desc(i);
			if (tdef->is_desc_order(i))
				log_debug("success set field[%d] desc order", i);
			else
				log_debug("set field[%d] desc roder fail", i);
		}
	}
	if (autoinc >= 0)
		tdef->set_auto_increment(autoinc);
	if (lastacc >= 0)
		tdef->set_lastacc(lastacc);
	if (lastmod >= 0)
		tdef->set_lastmod(lastmod);
	if (lastcmod >= 0)
		tdef->set_lastcmod(lastcmod);
	if (compressflag >= 0)
		tdef->set_compress_flag(compressflag);
	if (expireTime >= 0)
		tdef->set_expire_time(expireTime);

	if (tdef->set_key_fields(keyFieldCnt) < 0)
	{
		log_error("Table key size %d too large, must <= 255",
				  tdef->key_format() > 0 ? tdef->key_format() : -tdef->key_format());
		delete tdef;
		return NULL;
	}
	tdef->set_index_fields(idxFieldCnt);
	tdef->build_info_cache();
	return tdef;
}
