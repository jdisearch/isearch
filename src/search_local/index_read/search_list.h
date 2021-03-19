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

#ifndef SEARCH_LIST_H_H_
#define SEARCH_LIST_H_H_
#include "mem_pool.h"
#include "comm.h"
#include <vector>
#include <string>
using namespace std;

struct ListNode
{
	char key[MAX_DOCID_LENGTH];
	ListNode *prev;
	ListNode *next;
	ListNode *forward;
	ListNode *backward;
};

class SearchList
{
public:
	SearchList():head(NULL),tail(NULL),size(0){}
	~SearchList(){
		DestroyList();
	}

	bool InsertNode(ListNode *&list, const char *str);
	void InsertListNode(ListNode *list);
	void GetIntersectionSets();
	ListNode* Intersection(bool& res);
	int ListKeyCompare(ListNode* list, char *str);
	void ListNodeDelete(ListNode *node);
	int GetSize() {return size;}
	ListNode* GetHeadList() {return head;}
	bool IsContainKey(ListNode* keyList, const char* str);
	void GetDifferenceList(ListNode* keyList);
	void GetListElement(vector<string> &eles);

	void DestroyList();
	void DestroyNode(ListNode *list);
private:
	ListNode *head;
	ListNode *tail;
	int size;
};

#endif