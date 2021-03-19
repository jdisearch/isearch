/*
 * =====================================================================================
 *
 *       Filename:  node_set.cc
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
#include "node_set.h"
#include "node_index.h"
#include "node_list.h"
#include "global.h"
#include "node.h"

DTC_USING_NAMESPACE

//定义每种属性的内存大小, 至少有以下四种，可以再增加
const uint32_t NODE_SET::NG_ATTR_SIZE[] =
    {
        NODE_GROUP_INCLUDE_NODES * sizeof(NODE_ID_T),     //NEXT_NODE
        NODE_GROUP_INCLUDE_NODES * sizeof(NODE_ID_T) * 2, //TIME_LIST
        NODE_GROUP_INCLUDE_NODES * sizeof(MEM_HANDLE_T),  //VD_HANDLE
        NODE_GROUP_INCLUDE_NODES / 8,                     //DIRTY_BMP
};

int NODE_SET::Init(NODE_ID_T id)
{
    ng_list.prev = ng_list.next = INVALID_HANDLE;
    ng_dele.top = 0;
    ng_dele.count = 0;
    ng_free = 0;
    ng_nid = id;

    //属性
    ng_attr.count = attr_count();
    ng_attr.offset[0] = base_header_size();
    for (unsigned int i = 1; i < ng_attr.count; i++)
    {
        ng_attr.offset[i] = ng_attr.offset[i - 1] + NG_ATTR_SIZE[i - 1];
    }

    /* 初始化每个Node */
    for (unsigned i = 0; i < NODE_GROUP_INCLUDE_NODES; ++i)
    {
        next_node_id(i) = INVALID_NODE_ID;
        NODE_ID_T *lru = node_lru(i);
        lru[LRU_PREV] = node_id(i);
        lru[LRU_NEXT] = node_id(i);
        vd_handle(i) = INVALID_HANDLE;
        clr_dirty(i);
    }

    return 0;
}

/* init system reserved zone */
int NODE_SET::system_reserved_init()
{
    Node dirtyNode = allocate_node();
    if (!dirtyNode)
    {
        return -2;
    }

    Node cleanNode = allocate_node();
    if (!cleanNode)
    {
        return -3;
    }

    Node emptyNode = allocate_node();
    if (!emptyNode)
    {
        return -3;
    }

    /* init node list head */
    INIT_NODE_LIST_HEAD(dirtyNode, dirtyNode.node_id());
    INIT_NODE_LIST_HEAD(cleanNode, cleanNode.node_id());
    INIT_NODE_LIST_HEAD(emptyNode, emptyNode.node_id());

    /* insert node head's node-id to node-index*/
    I_INSERT(dirtyNode);
    I_INSERT(cleanNode);
    I_INSERT(emptyNode);

    return 0;
}

/* check system reserved zone integrity
 * the main purpose is upgrade/add the missing empty lru list 
 */
int NODE_SET::system_reserved_check()
{
    if (ng_free < 2)
        return -10;
    // ng_free==2 old format, index 2 is free & reserved
    // ng_free==3 new format, index 2 allocated to emptyNodeLru
    int hasEmptyLru1 = ng_free >= 3;

    // if new format, index 2 is allocated, lru pointer should be non-zero

    // sanity check passed
    if (hasEmptyLru1 == 0)
    {
        // no empty lru, allocate one
        Node emptyNode = allocate_node();
        if (!emptyNode)
        {
            return -3;
        }

        /* init node list head */
        INIT_NODE_LIST_HEAD(emptyNode, emptyNode.node_id());

        /* insert node head's node-id to node-index*/
        I_INSERT(emptyNode);
        return 1;
    }

    return 0;
}

Node NODE_SET::allocate_node(void)
{
    if (is_full())
    {
        return Node(NULL, 0);
    }

    //优先分配release掉的Node空间
    if (ng_dele.count > 0)
    {
        Node N(this, ng_dele.top);
        N.Reset();

        ng_dele.count--;
        ng_dele.top = (uint8_t)N.vd_handle();

        return N;
    }
    //在空闲Node中分配
    else
    {
        Node N(this, ng_free);
        N.Reset();

        ng_free++;
        return N;
    }
}

int NODE_SET::release_node(Node N)
{
    //复用node的handle attribute空间来把释放掉的node组织为单链表
    N.vd_handle() = ng_dele.top;
    ng_dele.top = N.Index();
    ng_dele.count++;

    return 0;
}

bool NODE_SET::is_full(void)
{
    return (ng_dele.count == 0 && ng_free >= NODE_GROUP_INCLUDE_NODES);
}

uint32_t NODE_SET::attr_count(void)
{
    return sizeof(NG_ATTR_SIZE) / sizeof(uint32_t);
}

uint32_t NODE_SET::base_header_size(void)
{
    return OFFSETOF(NODE_SET, ng_attr) + OFFSETOF(NG_ATTR_T, offset) + sizeof(uint32_t) * attr_count();
}

uint32_t NODE_SET::attr_size(void)
{
    uint32_t size = 0;

    for (uint32_t i = 0; i < attr_count(); i++)
    {
        size += NG_ATTR_SIZE[i];
    }

    return size;
}

uint32_t NODE_SET::Size(void)
{
    return base_header_size() + attr_size();
}

NODE_ID_T NODE_SET::node_id(int idx) const
{
    return (ng_nid + idx);
}

NODE_ID_T &NODE_SET::next_node_id(int idx)
{
    return __CAST__<NODE_ID_T>(NEXT_NODE)[idx];
}

NODE_ID_T *NODE_SET::node_lru(int idx)
{
    return &(__CAST__<NODE_ID_T>(TIME_LIST)[idx * 2]);
}

MEM_HANDLE_T &NODE_SET::vd_handle(int idx)
{
    return __CAST__<MEM_HANDLE_T>(VD_HANDLE)[idx];
}

bool NODE_SET::is_dirty(int idx)
{
    return FD_ISSET(idx, __CAST__<fd_set>(DIRTY_BMP));
}

void NODE_SET::set_dirty(int idx)
{
    FD_SET(idx, __CAST__<fd_set>(DIRTY_BMP));
}

void NODE_SET::clr_dirty(int idx)
{
    FD_CLR(idx, __CAST__<fd_set>(DIRTY_BMP));
}
