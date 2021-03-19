/*
 * =====================================================================================
 *
 *       Filename:  node_list.h
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
#ifndef __DTC_NODE_LIST_H
#define __DTC_NODE_LIST_H

#include "namespace.h"
#include "global.h"
#include "node.h"

DTC_BEGIN_NAMESPACE

#define INIT_NODE_LIST_HEAD(node, id) \
    do                                \
    {                                 \
        node.lru_prev() = id;          \
        node.lru_next() = id;          \
    } while (0)

inline void __NODE_LIST_ADD(Node p,
                            Node prev,
                            Node next)
{
    next.lru_prev() = p.node_id();
    p.lru_next() = next.node_id();
    p.lru_prev() = prev.node_id();
    prev.lru_next() = p.node_id();
}

inline void NODE_LIST_ADD(Node p, Node head)
{
    __NODE_LIST_ADD(p, head, head.Next());
}

inline void NODE_LIST_ADD_TAIL(Node p, Node head)
{
    __NODE_LIST_ADD(p, head.Prev(), head);
}

inline void __NODE_LIST_DEL(Node prev, Node next)
{
    next.lru_prev() = prev.node_id();
    prev.lru_next() = next.node_id();
}

inline void NODE_LIST_DEL(Node p)
{
    __NODE_LIST_DEL(p.Prev(), p.Next());
    p.lru_prev() = p.node_id();
    p.lru_next() = p.node_id();
}

inline void NODE_LIST_MOVE(Node p, Node head)
{
    __NODE_LIST_DEL(p.Prev(), p.Next());
    NODE_LIST_ADD(p, head);
}

inline void NODE_LIST_MOVE_TAIL(Node p, Node head)
{
    __NODE_LIST_DEL(p.Prev(), p.Next());
    NODE_LIST_ADD_TAIL(p, head);
}

inline int NODE_LIST_EMPTY(Node head)
{
    return head.lru_next() == head.node_id();
}

/*正向遍历*/
#define NODE_LIST_FOR_EACH(pos, head) \
    for (pos = head.Next(); pos != head; pos = pos.Next())

/*反向遍历*/
#define NODE_LIST_FOR_EACH_RVS(pos, head) \
    for (pos = head.Prev(); pos != head; pos = pos.Prev())

DTC_END_NAMESPACE

#endif
