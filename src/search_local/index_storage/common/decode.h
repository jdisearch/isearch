/*
 * =====================================================================================
 *
 *       Filename:  decode.h
 *
 *    Description:  decode/encode data.
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
#ifndef __CH_DECODE_H__
#define __CH_DECODE_H__

/*
 * Base Decode/Encode routines:
 *	p = Encode...(p, ...);	// encode object and advance pointer
 *	EncodedBytes...(...);	// calculate encoded object size
 *	Decode...(...);		// Decode objects
 */
#include "value.h"
#include "table_def.h"
#include "section.h"
#include "field.h"
#include "field_api.h"

#define __FLTFMT__ "%LA"
static inline char *encode_data_type(char *p, uint8_t type)
{
	*p++ = type == DField::Unsigned ? DField::Signed : type;
	return p;
}

extern int decode_length(DTCBinary &bin, uint32_t &len);
extern char *encode_length(char *p, uint32_t len);
extern int encoded_bytes_length(uint32_t n);

extern int decode_data_value(DTCBinary &bin, DTCValue &val, int type);
extern char *encode_data_value(char *p, const DTCValue *v, int type);
extern int encoded_bytes_data_value(const DTCValue *v, int type);

extern int decode_field_id(DTCBinary &, uint8_t &id, const DTCTableDefinition *tdef, int &needDefinition);
extern int decode_simple_section(DTCBinary &, SimpleSection &, uint8_t);
extern int decode_simple_section(char *, int, SimpleSection &, uint8_t);

extern int encoded_bytes_simple_section(const SimpleSection &, uint8_t);
extern char *encode_simple_section(char *p, const SimpleSection &, uint8_t);
extern int encoded_bytes_field_set(const DTCFieldSet &);
extern char *encode_field_set(char *p, const DTCFieldSet &);
extern int encoded_bytes_field_value(const DTCFieldValue &);
extern char *encode_field_value(char *, const DTCFieldValue &);

extern int encoded_bytes_multi_key(const DTCValue *v, const DTCTableDefinition *tdef);
extern char *encode_multi_key(char *, const DTCValue *v, const DTCTableDefinition *tdef);
class FieldSetByName;
extern int encoded_bytes_field_set(const FieldSetByName &);
extern char *encode_field_set(char *p, const FieldSetByName &);
extern int encoded_bytes_field_value(const FieldValueByName &);
extern char *encode_field_value(char *, const FieldValueByName &);

#endif
