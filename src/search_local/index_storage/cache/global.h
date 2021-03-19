/*
 * =====================================================================================
 *
 *       Filename:  global.h
 *
 *    Description:  macro definition and common function.
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

#ifndef __DTC_GLOBAL_H
#define __DTC_GLOBAL_H

#include <stdint.h>
#include <stdarg.h>
#include "namespace.h"
#include "pt_malloc.h"

DTC_BEGIN_NAMESPACE

/* 共享内存操作定义 */
#define M_HANDLE(ptr) DTCBinMalloc::Instance()->Handle(ptr)
#define M_POINTER(type, v) DTCBinMalloc::Instance()->Pointer<type>(v)
#define M_MALLOC(size) DTCBinMalloc::Instance()->Malloc(size)
#define M_CALLOC(size) DTCBinMalloc::Instance()->Calloc(size)
#define M_REALLOC(v, size) DTCBinMalloc::Instance()->ReAlloc(v, size)
#define M_FREE(v) DTCBinMalloc::Instance()->Free(v)
#define M_ERROR() DTCBinMalloc::Instance()->get_err_msg()

/* Node查找函数 */
#define I_SEARCH(id) NodeIndex::Instance()->Search(id)
#define I_INSERT(node) NodeIndex::Instance()->Insert(node)
/*#define I_DELETE(node)		NodeIndex::Instance()->Delete(node) */

/* memory handle*/
#define MEM_HANDLE_T ALLOC_HANDLE_T

/*Node ID*/
#define NODE_ID_T uint32_t
#define INVALID_NODE_ID ((NODE_ID_T)(-1))
#define SYS_MIN_NODE_ID ((NODE_ID_T)(0))
#define SYS_DIRTY_NODE_INDEX 0
#define SYS_CLEAN_NODE_INDEX 1
#define SYS_EMPTY_NODE_INDEX 2
#define SYS_DIRTY_HEAD_ID (SYS_MIN_NODE_ID + SYS_DIRTY_NODE_INDEX)
#define SYS_CLEAN_HEAD_ID (SYS_MIN_NODE_ID + SYS_CLEAN_NODE_INDEX)
#define SYS_EMPTY_HEAD_ID (SYS_MIN_NODE_ID + SYS_EMPTY_NODE_INDEX)

/* Node time list */
#define LRU_PREV (0)
#define LRU_NEXT (1)

/* features */
#define MIN_FEATURES 32

/*Hash ID*/
#define HASH_ID_T uint32_t

/* Node Group */
#define NODE_GROUP_INCLUDE_NODES 256

/* output u64 format */
#if __WORDSIZE == 64
#define UINT64FMT "%lu"
#else
#define UINT64FMT "%llu"
#endif

#if __WORDSIZE == 64
#define INT64FMT "%ld"
#else
#define INT64FMT "%lld"
#endif

DTC_END_NAMESPACE

#endif
