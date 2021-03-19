/*
 * =====================================================================================
 *
 *       Filename:  mallocator.h
 *
 *    Description:  memory operating interface.
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

#ifndef MALLOCATOR_H
#define MALLOCATOR_H

#include <stdint.h>
#include <stdlib.h>
#include "namespace.h"

DTC_BEGIN_NAMESPACE

#define ALLOC_SIZE_T uint32_t
#define ALLOC_HANDLE_T uint64_t
#define INTER_SIZE_T uint64_t
#define INTER_HANDLE_T uint64_t

#define INVALID_HANDLE 0ULL

#define SIZE_SZ (sizeof(ALLOC_SIZE_T))
#define MALLOC_ALIGNMENT (2 * SIZE_SZ)
#define MALLOC_ALIGN_MASK (MALLOC_ALIGNMENT - 1)
#define MAX_ALLOC_SIZE (((ALLOC_SIZE_T)-1) & ~MALLOC_ALIGN_MASK)

class Mallocator
{
public:
	Mallocator() {}
	virtual ~Mallocator() {}

	template <class T>
	T *Pointer(ALLOC_HANDLE_T hHandle) { return reinterpret_cast<T *>(handle_to_ptr(hHandle)); }

	virtual ALLOC_HANDLE_T Handle(void *p) = 0;

	virtual const char *get_err_msg() = 0;

	/*************************************************
	  Description:	分配内存
	  Input:		tSize		分配的内存大小
	  Output:		
	  Return:		内存块句柄，INVALID_HANDLE为失败
	*************************************************/
	virtual ALLOC_HANDLE_T Malloc(ALLOC_SIZE_T tSize) = 0;

	/*************************************************
	  Description:	分配内存，并将内存初始化为0
	  Input:		tSize		分配的内存大小
	  Output:		
	  Return:		内存块句柄，INVALID_HANDLE为失败
	*************************************************/
	virtual ALLOC_HANDLE_T Calloc(ALLOC_SIZE_T tSize) = 0;

	/*************************************************
	  Description:	重新分配内存
	  Input:		hHandle	老内存句柄
				tSize		新分配的内存大小
	  Output:		
	  Return:		内存块句柄，INVALID_HANDLE为失败(失败时不会释放老内存块)
	*************************************************/
	virtual ALLOC_HANDLE_T ReAlloc(ALLOC_HANDLE_T hHandle, ALLOC_SIZE_T tSize) = 0;

	/*************************************************
	  Description:	释放内存
	  Input:		hHandle	内存句柄
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	virtual int Free(ALLOC_HANDLE_T hHandle) = 0;

	/*************************************************
	  Description:	获取内存块大小
	  Input:		hHandle	内存句柄
	  Output:		
	  Return:		内存大小
	*************************************************/
	virtual ALLOC_SIZE_T chunk_size(ALLOC_HANDLE_T hHandle) = 0;

	/*************************************************
	  Description:	将句柄转换成内存地址
	  Input:		内存句柄
	  Output:		
	  Return:		内存地址，如果句柄无效返回NULL
	*************************************************/
	virtual void *handle_to_ptr(ALLOC_HANDLE_T hHandle) = 0;

	/*************************************************
	  Description:	将内存地址转换为句柄
	  Input:		内存地址
	  Output:		
	  Return:		内存句柄，如果地址无效返回INVALID_HANDLE
	*************************************************/
	virtual ALLOC_HANDLE_T ptr_to_handle(void *p) = 0;

	virtual ALLOC_SIZE_T ask_for_destroy_size(ALLOC_HANDLE_T hHandl) = 0;

	/*************************************************
	  Description:	检测handle是否有效
	  Input:		内存句柄
	  Output:		
      Return:	    0: 有效; -1:无效
	*************************************************/
	virtual int handle_is_valid(ALLOC_HANDLE_T mem_handle) = 0;
};

DTC_END_NAMESPACE

#endif
