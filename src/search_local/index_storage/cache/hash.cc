/*
 * =====================================================================================
 *
 *       Filename:  hash.cc
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
#include <string.h>
#include <stdio.h>
#include "hash.h"
#include "global.h"

DTC_USING_NAMESPACE

DTCHash::DTCHash() : _hash(NULL)
{
    memset(_errmsg, 0, sizeof(_errmsg));
}

DTCHash::~DTCHash()
{
}

NODE_ID_T &DTCHash::hash2_node(const HASH_ID_T v)
{
    return _hash->hh_buckets[v];
}

int DTCHash::Init(const uint32_t hsize, const uint32_t fixedsize)
{
    size_t size = sizeof(NODE_ID_T);
    size *= hsize;
    size += sizeof(HASH_T);

    MEM_HANDLE_T v = M_CALLOC(size);
    if (INVALID_HANDLE == v)
    {
        snprintf(_errmsg, sizeof(_errmsg), "init hash bucket failed, %s", M_ERROR());
        return -1;
    }

    _hash = M_POINTER(HASH_T, v);
    _hash->hh_size = hsize;
    _hash->hh_free = hsize;
    _hash->hh_node = 0;
    _hash->hh_fixedsize = fixedsize;

    /* init each nodeid to invalid */
    for (uint32_t i = 0; i < hsize; i++)
    {
        _hash->hh_buckets[i] = INVALID_NODE_ID;
    }

    return 0;
}

int DTCHash::Attach(MEM_HANDLE_T handle)
{
    if (INVALID_HANDLE == handle)
    {
        snprintf(_errmsg, sizeof(_errmsg), "attach hash bucket failed, memory handle = 0");
        return -1;
    }

    _hash = M_POINTER(HASH_T, handle);
    return 0;
}

int DTCHash::Detach(void)
{
    _hash = (HASH_T *)(0);
    return 0;
}
