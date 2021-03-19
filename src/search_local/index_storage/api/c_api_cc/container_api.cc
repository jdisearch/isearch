
#include <stdio.h>
#include <dlfcn.h>

#include "container.h"
#include "version.h"
#include "dtcint.h"

typedef IInternalService *(*QueryInternalServiceFunctionType)(const char *name, const char *instance);

IInternalService *QueryInternalService(const char *name, const char *instance)
{
	QueryInternalServiceFunctionType entry = NULL;
	entry = (QueryInternalServiceFunctionType)dlsym(RTLD_DEFAULT, "_QueryInternalService");
	if(entry == NULL)
		return NULL;
	return entry(name, instance);
}

static inline int fieldtype2keytype(int t)
{
	switch(t) {
		case DField::Signed:
		case DField::Unsigned:
			return DField::Signed;

		case DField::String:
		case DField::Binary:
			return DField::String;
	}

	return DField::None;
}

void NCServer::CheckInternalService(void)
{
	if(NCResultInternal::VerifyClass()==0)
		return;

	IInternalService *inst = QueryInternalService("dtcd", this->tablename);

	// not inside dtcd or tablename not found
	if(inst == NULL)
		return;

	// version mismatch, internal service is unusable
	const char *version = inst->query_version_string();
	if(version==NULL || strcasecmp(version_detail, version) != 0)
		return;

	// cast to DTC service
	IDTCService *inst1 = static_cast<IDTCService *>(inst);

	DTCTableDefinition *tdef = inst1->query_table_definition();

	// verify tablename etc
	if(tdef->is_same_table(tablename)==0)
		return;

	// verify and save key type
	int kt = fieldtype2keytype(tdef->key_type());
	if(kt == DField::None)
		// bad key type
		return;

	if(keytype == DField::None) {
		keytype = kt;
	} else if(keytype != kt) {
		badkey = 1;
		errstr = "Key Type Mismatch";
		return;
	}
	
	if(keyinfo.KeyFields()!=0) {
		// FIXME: checking keyinfo

		// ZAP key info, use ordered name from server
		keyinfo.Clear();
	}
	// add NCKeyInfo
	for(int i=0; i<tdef->key_fields(); i++) {
		kt = fieldtype2keytype(tdef->field_type(i));
		if(kt == DField::None)
			// bad key type
			return;
		keyinfo.AddKey(tdef->field_name(i), kt);
	}

	// OK, save it.
	// old tdef always NULL, because tablename didn't set, Server didn't complete
	this->tdef = tdef;
	this->admin_tdef = inst1->query_admin_table_definition();
	// useless here, internal DTCTableDefinition don't maintent a usage count
	tdef->INC();
	this->iservice = inst1;
	this->completed = 1;
	if(GetAddress() && iservice->match_listening_ports(GetAddress(), NULL)) {
		executor = iservice->query_task_executor();
	}
}

