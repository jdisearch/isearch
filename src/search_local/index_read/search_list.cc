/*
 * =====================================================================================
 *
 *       Filename:  search_list.h
 *
 *    Description:  search list class definition.
 *
 *        Version:  1.0
 *        Created:  15/08/2019
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include <malloc.h>
#include <string.h>
#include <string>

#include "log.h"
#include "search_list.h"

MemPool listNodePool;

bool SearchList::InsertNode(ListNode *&list, const char *str)
{
	if (str == NULL || strlen(str) == 0) {
		log_error("str is empty!");
		return false;
	}

	if (strlen(str) >= MAX_DOCID_LENGTH) {
		log_error("the length of str : %s is too long", str);
		return false;
	}

	if (list == NULL) {
		list = (ListNode*)listNodePool.PoolAlloc();
		if (list == NULL) {
			log_error("allocate listnode error,no more memory!");
			return false;
		}
		memset(list->key, 0, MAX_DOCID_LENGTH);
		list->prev = list->next = list->forward = list->backward = NULL;
		list->forward = (ListNode*)listNodePool.PoolAlloc();
		if (list->forward == NULL) {
			log_error("allocate listnode error,no more memory!");
			listNodePool.PoolFree(list);
			list = NULL;
			return false;
		}
		memset(list->forward->key, 0, MAX_DOCID_LENGTH);
		list->forward->prev = list->forward->next = list->forward->forward = list->forward->backward = NULL;
		list->forward->backward = list;
		strncpy(list->forward->key, str, strlen(str));
		return true;
	}

	ListNode *node = NULL;
	ListNode *insertNode = list;
	while (insertNode->forward != NULL && strcmp(str, insertNode->forward->key) > 0)
		insertNode = insertNode->forward;

	if (insertNode->forward != NULL && strcmp(str, insertNode->forward->key) == 0) {
		return true;
	}

	if (insertNode->forward == NULL) {
		node = (ListNode*)listNodePool.PoolAlloc();
		if (node == NULL){
			log_error("allocate listnode error,no more memory!");
			return false;
		}

		memset(node->key, 0, MAX_DOCID_LENGTH);
		node->prev = node->next = node->forward = node->backward = NULL;
		strncpy(node->key, str, strlen(str));
		insertNode->forward = node;
		node->backward = insertNode;
		return true;
	}

	node = (ListNode*)listNodePool.PoolAlloc();
	if (node == NULL){
		log_error("allocate listnode error,no more memory!");
		return false;
	}
	memset(node->key, 0, MAX_DOCID_LENGTH);
	node->prev = node->next = node->forward = node->backward = NULL;
	node->forward = insertNode->forward;
	node->backward = insertNode;
	insertNode->forward->backward = node;
	insertNode->forward = node;
	strncpy(node->key, str, strlen(str));

	return true;
}

void SearchList::InsertListNode(ListNode *list)
{
	if (list == NULL || list->forward == NULL) {
		return ;
	}
	if (size == 0)
	{
		list->prev = list;
		list->next = list;
		head = tail = list;
	} else if (strcmp(list->forward->key, head->forward->key) <= 0) {
		list->next = head;
		list->prev = tail;
		head->prev = list;
		tail->next = list;
		head = list;
	} else if (strcmp(list->forward->key, tail->forward->key) >= 0) {
		list->prev = tail;
		list->next = head;
		tail->next = list;
		head->prev = list;
		tail = list;
	} else {
		ListNode *tmp = head;
		while (strcmp(list->forward->key, tmp->forward->key) > 0) 
			tmp = tmp->next;
		list->prev = tmp->prev;
		list->next = tmp;
		tmp->prev->next = list;
		tmp->prev = list;
	}
	size = size + 1;

	return ;
}

void SearchList::GetIntersectionSets()
{
	if (size == 0 || size == 1) 
		return ;
	bool res = false;

	ListNode *list = Intersection(res); //res为false代表内存不够，不能新增链表节点
	if (!res) {
		DestroyNode(list);
		DestroyList();
		list = NULL;
		return ;
	}
	DestroyList();
	InsertListNode(list);

	return ;
}

ListNode* SearchList::Intersection(bool& res)
{
	ListNode *newList = NULL; //把得到的交集放入newList中，进而销毁原链表，把newList插入链表，此时链表只有newList一个链
	char str[MAX_DOCID_LENGTH];

	int flag = 0;
	int count = 0;
	while(1) {
		if (count == 0){
			memset(str, 0, MAX_DOCID_LENGTH);
			strcpy(str, tail->forward->key);
		} else if(tail->forward && tail->forward->forward) {
			memset(str, 0, MAX_DOCID_LENGTH);
			strcpy(str, tail->forward->forward->key);
		} else {
			return newList;
		}

		ListNode* list = head;
		while (list) {
			if (list->forward == NULL){
				return newList;
			}
			flag = ListKeyCompare(list, str);
			if (flag == -1) {
				log_debug("can't find value str : %s", str);
				return newList;
			} else if (flag == 1){
				res = InsertNode(newList, str);
				if (!res)
					return newList;
				break;
			}

			list = list->next;
		}

		count = count + 1;
	}

	return newList;
}

/*
  返回1代表遍历结束，说明所有list均包含str字符串
  返回-1代表该list中没有与str匹配的值，找不到交集，遍历结束
  返回0代表该list中发现了与str匹配的值
*/
int SearchList::ListKeyCompare(ListNode* list, char *str)
{
	if (!(strcmp(list->forward->key, str) < 0)) {
		return 1;
	}
	ListNode* tmp = list->forward;
	while (tmp != NULL && strcmp (tmp->key, str) < 0) {
		ListNode *del = tmp;
		tmp = tmp->forward;
		ListNodeDelete(del);
		del = NULL;
	}
	if (tmp == NULL) {
		return -1;
	}
	memset(str, 0, MAX_DOCID_LENGTH);
	strcpy(str, tmp->key); // 把tmp中的key更新str，由遍历结果得知该tmp中的key是大于或者等于str的
	return 0;
}

bool SearchList::IsContainKey(ListNode* keyList, const char* str)
{
	int res;
	if (keyList == NULL || keyList->forward == NULL)
		return false;
	ListNode* tmp = keyList->forward;
	while (tmp != NULL) {
		res = strcmp(tmp->key, str);
		if (res > 0)
			return false;
		else if (res == 0)
			return true;
		tmp = tmp->forward;
	}
	return false;
}

void SearchList::GetDifferenceList(ListNode* keyList)
{
	if (size != 1 || keyList == NULL || head->forward == NULL)
		return ;
	ListNode *node = head->forward;
	ListNode *temp = node->forward;
	while (node != NULL)
	{
		if (IsContainKey(keyList, node->key))
			ListNodeDelete(node);
		node = temp;
		if (node == NULL)
			break;
		temp = node->forward;
	}

	/*防止在删除节点的时候，head链表只剩一个头结点*/
	if (head && head->forward == NULL) {
		listNodePool.PoolFree(head);
		head = tail = NULL;
		size = 0;
	}
	return ;
}

void SearchList::GetListElement(vector<string>& eles)
{
	if (size == 0){
		log_debug("search list is empty");
		return ;
	}

	ListNode *node = head->forward;
	while (node != NULL) {
		eles.push_back(node->key);
		node = node->forward;
	}
	return ;
}

void SearchList::ListNodeDelete(ListNode *node) {
	if (node == NULL) {
		log_debug("node is NULL, can't delete");
		return ;
	}
	if (node->forward == NULL) {
		node->backward->forward = NULL;
		listNodePool.PoolFree(node);
		node = NULL;
		return ;
	}
	node->backward->forward = node->forward;
	node->forward->backward = node->backward;
	listNodePool.PoolFree(node);
	node = NULL;
	return ;
}

void SearchList::DestroyList()
{
	if (head == NULL && tail == NULL)
		return ;
	ListNode *list = head;
	ListNode *tmp;
	while (list != tail) {
		tmp = list->next;
		DestroyNode(list);
		list = tmp;
	}
	DestroyNode(list);
	head = NULL;
	tail = NULL;
	size = 0;
}

void SearchList::DestroyNode(ListNode *list)
{
	if (list == NULL)
		return ;
	ListNode *tmp = list;
	ListNode *li;
	while(tmp != NULL)
	{
		li = tmp->forward;
		listNodePool.PoolFree(tmp);
		tmp = li;
	}
	tmp = NULL;
	li = NULL;
}