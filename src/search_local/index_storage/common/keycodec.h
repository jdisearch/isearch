/*
 * =====================================================================================
 *
 *       Filename:  keycodec.h
 *
 *    Description:  key decode relevant operation.
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

#ifndef __DTC_KEY_CODEC_H
#define __DTC_KEY_CODEC_H

#include <string.h>
#include <stdint.h>
#include "namespace.h"

DTC_BEGIN_NAMESPACE

class KeyCodec
{
public:
	KeyCodec(const unsigned t)
	{
		set_key_type(t);
	}

	~KeyCodec() {}

	void set_key_type(const unsigned t)
	{
		type = t;
	}
	unsigned key_length(const char *ptr)
	{
		return type > 0 ? type : *(unsigned char *)ptr;
	}
	unsigned total_length(const char *ptr)
	{
		return type > 0 ? key_length(ptr) : key_length(ptr) + 1;
	}
	const char *key_pointer(const char *ptr)
	{
		return type > 0 ? ptr : ptr + 1;
	}
	int key_compare(const char *a, const char *b)
	{
		switch (type)
		{
		case 1:
			return *(uint8_t *)a - *(uint8_t *)b;
		case 2:
			return *(uint16_t *)a - *(uint16_t *)b;
		case 4:
			return *(uint32_t *)a - *(uint32_t *)b;
		}

		return memcmp(a, b, total_length(a));
	}

	unsigned key_hash(const char *ptr)
	{
		unsigned hash = 0x123;
		unsigned len = total_length(ptr);

		do
		{
			unsigned char c = *ptr++;
			//c = icase_hash(c);
			hash = hash * 101 + c;
		} while (--len);

		return hash;
	}

private:
	/*
		 * This removes bit 5 if bit 6 is set.  (from git name-hash.c)
		 *
		 * That will make US-ASCII characters hash to their upper-case
		 * equivalent. We could easily do this one whole word at a time,
		 * but that's for future worries.
		 */
	static inline unsigned char icase_hash(unsigned char c)
	{
		return c & ~((c & 0x40) >> 1);
	}

private:
	unsigned type;
};

DTC_END_NAMESPACE
#endif
