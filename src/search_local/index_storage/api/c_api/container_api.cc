/*
 * =====================================================================================
 *
 *       Filename:  container_api.cc
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
#include <stdio.h>
#include <dlfcn.h>

#include "container.h"
#include "version.h"
#include "dtc_int.h"

typedef IInternalService *(*QueryInternalServiceFunctionType)(const char *name, const char *instance);

IInternalService *query_internal_service(const char *name, const char *instance)
{
	QueryInternalServiceFunctionType entry = NULL;
	entry = (QueryInternalServiceFunctionType)dlsym(RTLD_DEFAULT, "_QueryInternalService");
	if (entry == NULL)
		return NULL;
	return entry(name, instance);
}

static inline int field_type_to_key_type(int t)
{
	switch (t)
	{
	case DField::Signed:
	case DField::Unsigned:
		return DField::Signed;

	case DField::String:
	case DField::Binary:
		return DField::String;
	}

	return DField::None;
}

void NCServer::check_internal_service(void)
{
	if (NCResultInternal::verify_class() == 0)
		return;

	IInternalService *inst = query_internal_service("dtcd", this->tablename);

	// not inside dtcd or tablename not found
	if (inst == NULL)
		return;

	// version mismatch, internal service is unusable
	const char *version = inst->query_version_string();
	if (version == NULL || strcasecmp(version_detail, version) != 0)
		return;

	// cast to DTC service
	IDTCService *inst1 = static_cast<IDTCService *>(inst);

	DTCTableDefinition *tdef = inst1->query_table_definition();

	// verify tablename etc
	if (tdef->is_same_table(tablename) == 0)
		return;

	// verify and save key type
	int kt = field_type_to_key_type(tdef->key_type());
	if (kt == DField::None)
		// bad key type
		return;

	if (keytype == DField::None)
	{
		keytype = kt;
	}
	else if (keytype != kt)
	{
		badkey = 1;
		errstr = "Key Type Mismatch";
		return;
	}

	if (keyinfo.key_fields() != 0)
	{
		// FIXME: checking keyinfo

		// ZAP key info, use ordered name from server
		keyinfo.Clear();
	}
	// add NCKeyInfo
	for (int i = 0; i < tdef->key_fields(); i++)
	{
		kt = field_type_to_key_type(tdef->field_type(i));
		if (kt == DField::None)
			// bad key type
			return;
		keyinfo.add_key(tdef->field_name(i), kt);
	}

	// OK, save it.
	// old tdef always NULL, because tablename didn't set, Server didn't complete
	this->tdef = tdef;
	this->admin_tdef = inst1->query_admin_table_definition();
	// useless here, internal DTCTableDefinition don't maintent a usage count
	tdef->INC();
	this->iservice = inst1;
	this->completed = 1;
	if (get_address() && iservice->match_listening_ports(get_address(), NULL))
	{
		executor = iservice->query_task_executor();
	}
}
