/*
 * =====================================================================================
 *
 *       Filename:  journal_id.h
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
#ifndef __DTC_JOURNAL_ID
#define __DTC_JOURNAL_ID

#include <stdint.h>

struct JournalID
{
    uint32_t serial;
    uint32_t offset;

    JournalID()
    {
        serial = 0;
        offset = 0;
    }

    JournalID(const JournalID &v)
    {
        serial = v.serial;
        offset = v.offset;
    }

    JournalID(uint32_t s, uint32_t o)
    {
        serial = s;
        offset = o;
    }

    JournalID &operator=(const JournalID &v)
    {
        serial = v.serial;
        offset = v.offset;
        return *this;
    }

    uint32_t Serial() const
    {
        return serial;
    }

    uint32_t Offset() const
    {
        return offset;
    }

    /*
         * 对外接口全部打包为uint64_t, 方便操作。
         */
    JournalID &operator=(const uint64_t v)
    {
        serial = v >> 32;
        offset = v & 0xFFFFFFFFULL;
        return *this;
    }

    JournalID(uint64_t v)
    {
        serial = v >> 32;
        offset = v & 0xFFFFFFFFULL;
    }

    operator uint64_t() const
    {
        uint64_t v = serial;
        v <<= 32;
        v += offset;
        return v;
    }

    int Zero() const
    {
        return serial == 0 && offset == 0;
    }

    int GE(const JournalID &v)
    {
        return serial > v.serial || (serial == v.serial && offset >= v.offset);
    }

} __attribute__((packed));

#endif
