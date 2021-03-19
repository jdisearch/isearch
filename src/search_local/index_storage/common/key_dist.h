/*
 * =====================================================================================
 *
 *       Filename:  key_dist.h
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
#include "value.h"

class KeyDist
{
public:
	KeyDist() {}
	virtual ~KeyDist() = 0;
	virtual unsigned int HashKey(const DTCValue *) = 0;
	virtual int key2_db_id(const DTCValue *) = 0;
	virtual int key2_table(const DTCValue *, char *, int) = 0;
};
