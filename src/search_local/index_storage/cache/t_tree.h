/*
 * =====================================================================================
 *
 *       Filename:  t_tree.h
 *
 *    Description:  T-tree fundamental operation. only for TreeData invoke.
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

#ifndef T_TREE_H
#define T_TREE_H

#include <stdint.h>
#include "mallocator.h"

int64_t KeyCompare(const char *pchKey, void *pCmpCookie, Mallocator &stMalloc, ALLOC_HANDLE_T hOtherKey);
int Visit(Mallocator &stMalloc, ALLOC_HANDLE_T &hRecord, void *pCookie);

typedef int64_t (*KeyComparator)(const char *pchKey, void *pCmpCookie, Mallocator &stMalloc, ALLOC_HANDLE_T hOtherKey);
typedef int (*ItemVisit)(Mallocator &stMalloc, ALLOC_HANDLE_T &hRecord, void *pCookie);

class Ttree
{
protected:
	ALLOC_HANDLE_T m_hRoot;
	Mallocator &m_stMalloc;
	char m_szErr[100];

public:
	Ttree(Mallocator &stMalloc);
	~Ttree();

	const char *get_err_msg() { return m_szErr; }
	const ALLOC_HANDLE_T Root() const { return m_hRoot; }
	ALLOC_HANDLE_T first_node();

	/*************************************************
	  Description:	attach一块已经格式化好的内存
	  Input:		
	  Output:		
	  Return:		
	*************************************************/
	void Attach(ALLOC_HANDLE_T hRoot) { m_hRoot = hRoot; }

	/*************************************************
	  Description:	将key insert到树里，hRecord为key对应的数据（包含key）
	  Input:		pchKey		插入的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				hRecord		保存着要插入的key以及其他数据的句柄
	  Output:		
	  Return:		0为成功，EC_NO_MEM为内存不足，EC_KEY_EXIST为key已经存在，其他值为错误
	*************************************************/
	int Insert(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T hRecord, bool &isAllocNode);

	/*************************************************
	  Description:	删除key以及对应的数据(但不会自动释放key对应的内存)
	  Input:		pchKey		插入的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int Delete(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, bool &isFreeNode);

	int find_handle(ALLOC_HANDLE_T hRecord);

	/*************************************************
	  Description:	查找key对应的数据
	  Input:		pchKey		插入的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
	  Output:		hRecord		保存查找到的key以及其他数据的句柄
	  Return:		0为查找不到，1为找到数据
	*************************************************/
	int Find(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord);

	/*************************************************
	  Description:	查找key对应的数据
	  Input:		pchKey		插入的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
	  Output:		phRecord		指向树节点的item指针
	  Return:		0为查找不到，1为找到数据
	*************************************************/
	int Find(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T *&phRecord);

	/*************************************************
	  Description:	销毁整棵树，并释放相应的内存
	  Input:		
	  Output:		
	  Return:		0为成功，非0失败
	*************************************************/
	int Destroy();

	/*************************************************
	  Description: 查询销毁整棵树可以释放多少空闲内存	
	  Input:		
	  Output:		
	  Return:	 >0 成功， 0 失败
	*************************************************/
	unsigned ask_for_destroy_size(void);

	/*************************************************
	  Description:	从小到大遍历整棵树
	  Input:		pfVisit	访问数据记录的用户自定义函数
				pCookie	自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_forward(ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从大到小遍历整棵树
	  Input:		pfVisit	访问数据记录的用户自定义函数
				pCookie	自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_backward(ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	后序遍历整棵树
	  Input:		pfVisit	访问数据记录的用户自定义函数
				pCookie	自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int post_order_traverse(ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，从小到大遍历树，遍历的范围为[key, key+iInclusion]
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				iInclusion		key的范围
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_forward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, int64_t iInclusion, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，从小到大遍历树, 遍历的范围为[key, key1]
	  Input:		pchKey		开始的key
				pchKey1		结束的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_forward(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，从小到大遍历树(遍历大于等于key的所有记录)
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_forward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，从大到小遍历树(遍历小于等于key的所有记录)
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_backward(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，从大到小遍历树，遍历的范围为[key, key1]
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int traverse_backward(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，先左右树，后根结点, 遍历的范围为[key, key1]
	  Input:		pchKey		开始的key
				pchKey1		结束的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int post_order_traverse(const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，后序遍历树(遍历大于等于key的所有记录)
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int post_order_traverse_ge(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	/*************************************************
	  Description:	从指定的key开始，后序遍历树(遍历小于等于key的所有记录)
	  Input:		pchKey		开始的key
				pCmpCookie	调用用户自定义的pfComp函数跟树里的节点比较时作为输入参数
				pfComp		用户自定义的key比较函数
				pfVisit		访问数据记录的用户自定义函数
				pCookie		自定义函数的cookie参数
	  Output:		
	  Return:		0为成功，其他值为错误
	*************************************************/
	int post_order_traverse_le(const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
};

/************************************************************
  Description:    封装了T-tree node的各种操作，仅供t-tree内部使用   
  Version:         DTC 3.0
***********************************************************/
struct _TtreeNode
{
	enum
	{
		PAGE_SIZE = 20,			  // 每个节点保存多少条记录
		MIN_ITEMS = PAGE_SIZE - 2 // minimal number of items in internal node
	};

	ALLOC_HANDLE_T m_hLeft;
	ALLOC_HANDLE_T m_hRight;
	int8_t m_chBalance;
	uint16_t m_ushNItems;
	ALLOC_HANDLE_T m_ahItems[PAGE_SIZE];

	int Init();
	static ALLOC_HANDLE_T Alloc(Mallocator &stMalloc, ALLOC_HANDLE_T hRecord);
	static int Insert(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T hRecord, bool &isAllocNode);
	static int Delete(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, bool &isFreeNode);
	static int balance_left_branch(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode);
	static int balance_right_branch(Mallocator &stMalloc, ALLOC_HANDLE_T &hNode);
	static int Destroy(Mallocator &stMalloc, ALLOC_HANDLE_T hNode);
	static unsigned ask_for_destroy_size(Mallocator &, ALLOC_HANDLE_T hNode);

	// 查找指定的key。找到返回1，否则返回0
	int Find(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hRecord);
	int Find(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T *&phRecord);
	int find_handle(Mallocator &stMalloc, ALLOC_HANDLE_T hRecord);
	// 假设node包含key-k1~kn，查找这样的node节点：k1<= key <=kn
	int find_node(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ALLOC_HANDLE_T &hNode);
	int traverse_forward(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie);
	int traverse_backward(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie);
	int post_order_traverse(Mallocator &stMalloc, ItemVisit pfVisit, void *pCookie);

	int traverse_forward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, int iInclusion, ItemVisit pfVisit, void *pCookie);
	int traverse_forward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
	int traverse_forward(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	int traverse_backward(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
	int traverse_backward(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);

	int post_order_traverse(Mallocator &stMalloc, const char *pchKey, const char *pchKey1, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
	int post_order_traverse_ge(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
	int post_order_traverse_le(Mallocator &stMalloc, const char *pchKey, void *pCmpCookie, KeyComparator pfComp, ItemVisit pfVisit, void *pCookie);
} __attribute__((packed));
typedef struct _TtreeNode TtreeNode;

#endif
