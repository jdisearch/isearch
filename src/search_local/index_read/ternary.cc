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

#include <fstream>
#include "utf8_str.h"
#include "log.h"
#include "ternary.h"
using namespace std;

bool TernaryTree::Init(string path)
{
	uint32_t freq;
	string str,word;
	ifstream suggest_infile;
	suggest_infile.open(path.c_str());
	if (suggest_infile.is_open() == false) {
		log_error("open file error: %s.", path.c_str());
		return false;
	}

	while (getline(suggest_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 2) {
			word = str_vec[0];
			freq = atoi(str_vec[0].c_str());
			if (!Insert(word, freq)){
				suggest_infile.close();
				return false;
			}
		}
	}
	suggest_infile.close();
	return true;
}

bool TernaryTree::Insert(string word, uint32_t freq)
{
	if(word.size() == 0)
	{
		log_debug("word id empty");
		return true;
	}
	if (!Insert(word, 0, freq, m_root)){
		log_error("insert word:%s error", word.c_str());
		return false;
	}
	return true;
}

bool TernaryTree::Insert(string word, int index, uint32_t freq, NodeTree* &node)
{
	if(node == NULL){
		node = new NodeTree(word[index], UNCOMPLETED);
		if (node == NULL) {
			log_error("allocate ternary tree node failed");
			return false;
		}
	}
	if(word[index] < node->word)
	{
		return Insert(word, index, freq, node->left);
	}
	else if(word[index] > node->word)
	{
		return Insert(word, index, freq, node->right);
	}
	else
	{
		if(index + 1 == (int)word.length())
		{
			node->type = COMPLETED;
			node->freq = freq;
			return true;
		}
		else
		{
			return Insert(word, index + 1, freq, node->mid); 
		}
	}
}

bool TernaryTree::FindSimliar(string word, PriorityQueue& priorityQueue)
{
	NodeTree* node = Find(word, priorityQueue);
	if(node == NULL)
	{
		log_error("search suffix like %s is error", word.c_str());
		return false;
	}
	DFS(word, node, priorityQueue);
	return true;
}

NodeTree* TernaryTree::Find(string word, PriorityQueue& priorityQueue)
{
	if(word.size() == 0)
	{
		log_error("search suffix is empty");
		return NULL;
	}
	int pos = 0;
	NodeTree* node = m_root;
	if(node == NULL)
	{
		log_error("the ternary tree is empty");
		return NULL;
	}

	while(node != NULL)
	{
		if(word[pos] < node->word)
		{
			node = node->left; 
		}
		else if(word[pos] > node->word)
		{
			node = node->right;
		}
		else
		{
			if(++pos == (int)word.size())
			{
				if (COMPLETED == node->type){
					PAIR tmp(node->freq, word);
					priorityQueue.EnQueue(tmp);
				}
				return node->mid;
			}
			node = node->mid;
		}
	}
	return NULL;
}

void TernaryTree::DFS(string perfix, NodeTree* node, PriorityQueue& priorityQueue)
{
	if(node != NULL)
	{
		if(COMPLETED == node->type)
		{
			PAIR tmp(node->freq, perfix+node->word);
			priorityQueue.EnQueue(tmp);
		}
		DFS(perfix, node->left, priorityQueue);
		DFS(perfix+node->word, node->mid, priorityQueue);
		DFS(perfix, node->right, priorityQueue);
	}
}

void TernaryTree::DestroyTree()
{
	NodeTree* node = m_root;
	DestroyNode(node);
	m_root = NULL;
}

void TernaryTree::DestroyNode(NodeTree* node)
{
	if(node == NULL)
		return ;
	if(node->left)
		DestroyNode(node->left);
	if(node->mid)
		DestroyNode(node->mid);
	if(node->right)
		DestroyNode(node->right);
	delete node;
	node = NULL;
}