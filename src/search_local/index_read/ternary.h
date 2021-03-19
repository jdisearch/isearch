/*
 * =====================================================================================
 *
 *       Filename:  ternary.h
 *
 *    Description:  ternary tree class definition.
 *
 *        Version:  1.0
 *        Created:  02/08/2019
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef TERNARY_H_H
#define TERNARY_H_H

#include <vector>
#include <string>
#include "priority_queue.h"
#include "singleton.h"

enum NodeType
{
	COMPLETED,
	UNCOMPLETED
};

typedef struct Node
{
	char word;
	NodeType type;
	uint32_t freq;
	struct Node *left,*mid,*right;

	Node(char ch, NodeType t)
	{
		word = ch;
		type = t;
		freq = 0;
		left = mid = right = NULL;
	}
}NodeTree;

class TernaryTree
{
public:
	static TernaryTree *Instance()
	{
		return CSingleton<TernaryTree>::Instance();
	}

	static void Destroy()
	{
		CSingleton<TernaryTree>::Destroy();
	}

	TernaryTree():m_root(NULL){}
	~TernaryTree(){
		DestroyTree();
	}

	bool Init(string path);
	bool Insert(string word, int index, uint32_t freq, NodeTree* &node);
	bool Insert(string word, uint32_t freq);
	NodeTree* Find(string word, PriorityQueue& priorityQueue);
	void DFS(string perfix, NodeTree* node, PriorityQueue& priorityQueue);
	bool FindSimliar(string word, PriorityQueue& priorityQueue);
	void DestroyTree();
	void DestroyNode(NodeTree *node);
private:
	NodeTree* m_root;
};

#endif