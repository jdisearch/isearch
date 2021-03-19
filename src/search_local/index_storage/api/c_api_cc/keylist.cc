#include <errno.h>

#include "memcheck.h"
#include "protocol.h"
#include "keylist.h"
#include "cache_error.h"

int NCKeyValueList::KeyValueMax = 32;

NCKeyInfo::~NCKeyInfo()
{
	for(int i = 0; i < 8; ++i)
	{
		if(keyName[i])
		{
			free(keyName[i]);
			keyName[i] = NULL;
		}
	}
}

int NCKeyInfo::KeyIndex(const char *n) const
{
	namemap_t::const_iterator i = keyMap.find(n);
	return i == keyMap.end() ? -1 : i->second;
}

int NCKeyInfo::AddKey(const char* name, int type)
{
	switch(type){
		case DField::Signed:
		case DField::String:
			break;
		default:
			return -EC_BAD_KEY_TYPE;
	}
	if(KeyIndex(name) >= 0) {
		//return -EC_DUPLICATE_FIELD;

		// ignore duplicate key field name
		// because NCKeyInfo may be initialized by CheckInternalService()
		// add again must be allowed for code compatibility
		return 0;
	}
	int cnt = KeyFields();
	if(cnt >= (int)(sizeof(keyType)/sizeof(keyType[0]))) return -EC_BAD_MULTIKEY;
	char* localName = (char*)malloc(strlen(name)+1);
	strcpy(localName, name);
	keyName[cnt] = localName;
	keyMap[localName] = cnt;
	keyType[cnt] = type;
	keyCount++;
	return 0;
}

int NCKeyInfo::Equal(const NCKeyInfo &other) const {
	int n = KeyFields();
	// key field count mis-match
	if(other.KeyFields() != n)
		return 0;
	// key type mis-match
	if(memcmp(keyType, other.keyType, n)!=0)
		return 0;
	for(int i=0; i<n; i++) {
		// key name mis-match
		if(strcasecmp(keyName[i], other.keyName[i]) != 0)
			return 0;
	}
	return 1;
}

void NCKeyValueList::Unset(void) {
	if(kcount) {
		const int kn = KeyFields();
		for(int i=0; i<kcount; i++){
			for(int j=0; j<kn; j++)
				if(KeyType(j) == DField::String || KeyType(j) == DField::Binary)
					FREE(Value(i, j).bin.ptr);
		}

		FREE_CLEAR(val);
		memset(fcount, 0, sizeof(fcount));
		kcount = 0;
	}
}

int NCKeyValueList::AddValue(const char* name, const DTCValue &v, int type)
{
	const int kn = KeyFields();

	int col = keyinfo->KeyIndex(name);
	if(col < 0 || col >= kn)
		return -EC_BAD_KEY_NAME;

	switch(KeyType(col)){
		case DField::Signed:
		case DField::Unsigned:
			if(type != DField::Signed && type != DField::Unsigned)
				return -EC_BAD_VALUE_TYPE;
			break;
		case DField::String:
		case DField::Binary:
			if(type != DField::String && type != DField::Binary)
				return -EC_BAD_VALUE_TYPE;
			if(v.bin.len > 255)
				return -EC_KEY_OVERFLOW;
			break;
		default:
			return -EC_BAD_KEY_TYPE;
	}

	int row = fcount[col];
	if(row >= KeyValueMax)
		return -EC_TOO_MANY_KEY_VALUE; // key值太多
	if(row >= kcount){
		if(REALLOC(val, (kcount+1)*kn*sizeof(DTCValue)) == NULL)
			throw std::bad_alloc();
		memset(&Value(row, 0), 0, kn*sizeof(DTCValue));
		kcount++;
	}
	DTCValue &slot = Value(row, col);
	switch(KeyType(col)){
		case DField::Signed:
		case DField::Unsigned:
			slot = v;
			break;
		
		case DField::String:
		case DField::Binary:
			slot.bin.len = v.bin.len;
			slot.bin.ptr =  (char *)MALLOC(v.bin.len+1);
			if(slot.bin.ptr==NULL)
				throw std::bad_alloc();
			memcpy(slot.bin.ptr, v.bin.ptr, v.bin.len);
			slot.bin.ptr[v.bin.len] = '\0';
			break;
	}
	fcount[col]++;
	return 0;
}

