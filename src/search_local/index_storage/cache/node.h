/*
 * =====================================================================================
 *
 *       Filename:  node.h
 *
 *    Description:  node operation.
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

#ifndef __NODE_DTC_H
#define __NODE_DTC_H

#include <stdint.h>
#include "namespace.h"
#include "global.h"
#include "node_set.h"
#include "node_index.h"

DTC_BEGIN_NAMESPACE

class NGInfo;
class NodeIndex;

class Node
{
public:
	Node(NODE_SET *ns = NULL, int idx = 0) : _owner(ns), _index(idx) {}
	Node(const Node &n) : _owner(n._owner), _index(n._index) {}
	~Node() {}

public:
	int Index(void) { return _index; }
	NODE_SET *Owner() { return _owner; }

	/* attribute op*/
	NODE_ID_T &lru_prev()
	{
		NODE_ID_T *p = node_lru();
		return p[LRU_PREV];
	}

	NODE_ID_T &lru_next()
	{
		NODE_ID_T *p = node_lru();
		return p[LRU_NEXT];
	}

	NODE_ID_T &next_node_id() { return _owner->next_node_id(_index); }
	NODE_ID_T node_id() { return _owner->node_id(_index); }

	MEM_HANDLE_T &vd_handle() { return _owner->vd_handle(_index); }

	/* return time-marker time */
	unsigned int Time() { return (unsigned int)vd_handle(); }

	/* dirty flag*/
	bool is_dirty() const { return _owner->is_dirty(_index); }
	void set_dirty() { return _owner->set_dirty(_index); }
	void clr_dirty() { return _owner->clr_dirty(_index); }

public:
	/* used for timelist */
	Node Next() { return from_id(lru_next()); }
	Node Prev() { return from_id(lru_prev()); }

	/* used for hash */
	Node next_node(void) { return from_id(next_node_id()); }

	/* for copyable */
	Node &operator=(const Node &n)
	{
		_owner = n._owner;
		_index = n._index;
		return *this;
	}
	int operator!() const { return _owner == NULL || _index >= NODE_GROUP_INCLUDE_NODES; }
	int operator!=(Node &node) { return _owner != node.Owner() || _index != node.Index(); }
	int operator==(Node &node) { return _owner == node.Owner() && _index == node.Index(); }

	int not_in_lru_list() { return lru_prev() == node_id() || lru_next() == node_id(); }
	static Node Empty(void)
	{
		Node node;
		return node;
	}

private:
	/* init or delete this */
	int Reset()
	{
		next_node_id() = INVALID_NODE_ID;
		lru_prev() = node_id();
		lru_next() = node_id();

		clr_dirty();
		return 0;
	}

	int Release()
	{
		_owner->release_node(*this);
		Reset();
		_owner = NULL;
		_index = 0;
		return 0;
	}

	static inline Node from_id(NODE_ID_T id) { return I_SEARCH(id); }

private:
	// [0] = prev, [1] = next
	NODE_ID_T *node_lru() { return _owner->node_lru(_index); }

private:
	NODE_SET *_owner;
	int _index;

public:
	/* friend class */
	friend class NGInfo;
	friend class NodeIndex;
	friend struct node_set;
};

DTC_END_NAMESPACE

#endif