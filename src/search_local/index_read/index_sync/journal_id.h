/*
 * =====================================================================================
 *
 *       Filename:  journal_id.h
 *
 *    Description:  journal id class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __TTC_JOURNAL_ID
#define __TTC_JOURNAL_ID

#include <stdint.h>

struct CJournalID
{
        uint32_t serial;
        uint32_t offset;

        CJournalID()
        {
            serial = 0;
            offset = 0;
        }

        CJournalID(const CJournalID & v)
        {
            serial = v.serial;
            offset = v.offset;
        }

        CJournalID(uint32_t s, uint32_t o) 
        {
            serial = s;
            offset = o;
        }

        CJournalID& operator =(const CJournalID& v)
        {
            serial = v.serial;
            offset = v.offset;
            return *this;
        }

	uint32_t Serial() const  {
		return serial;
	}

	uint32_t Offset() const {
		return offset;
	}

        /*
         * 对外接口全部打包为uint64_t, 方便操作。
         */
        CJournalID& operator =(const uint64_t v)
        {
            serial = v >> 32;
            offset = v & 0xFFFFFFFFULL;
            return *this;
        }

        CJournalID(uint64_t v)
        {
            serial = v >> 32;
            offset = v & 0xFFFFFFFFULL;
        }

        operator uint64_t() const
        {
            uint64_t v= serial;
            v <<= 32;
            v += offset;
            return v;
        }

        int Zero() const
        {
            return serial == 0 && offset == 0;
        }

        int GE(const CJournalID& v)
        {
            return serial > v.serial || (serial == v.serial && offset >= v.offset);
        }

}__attribute__((packed));

#endif
