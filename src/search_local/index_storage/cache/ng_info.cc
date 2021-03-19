/*
 * =====================================================================================
 *
 *       Filename:  ng_info.cc
 *
 *    Description:  NodeGroup operation.
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
#include <string.h>
#include <stdio.h>
#include "node_set.h"
#include "node_list.h"
#include "node_index.h"
#include "ng_info.h"
#include "node.h"
#include "dtc_global.h"

DTC_USING_NAMESPACE

NGInfo::NGInfo() : _ngInfo(NULL)
{
    memset(_errmsg, 0, sizeof(_errmsg));
    emptyCnt = 0;
    emptyStartupMode = CREATED;

    statUsedNG = statmgr.get_item_u32(DTC_USED_NGS);
    statUsedNode = statmgr.get_item_u32(DTC_USED_NODES);
    statDirtyNode = statmgr.get_item_u32(DTC_DIRTY_NODES);
    statEmptyNode = statmgr.get_item_u32(DTC_EMPTY_NODES);
    statEmptyNode = 0;
    statUsedRow = statmgr.get_item_u32(DTC_USED_ROWS);
    statDirtyRow = statmgr.get_item_u32(DTC_DIRTY_ROWS);
}

NGInfo::~NGInfo()
{
}

Node NGInfo::allocate_node(void)
{
    //优先在空闲链表分配
    NODE_SET *NS = find_free_ng();
    if (!NS)
    {
        /* 防止NodeGroup把内存碎片化，采用预分配 */
        static int step = DTCGlobal::_pre_alloc_NG_num;
        static int fail = 0;
        for (int i = 0; i < step; i++)
        {
            NS = allocate_ng();
            if (!NS)
            {
                if (i == 0)
                    return Node();
                else
                {
                    fail = 1;
                    step = 1;
                    break;
                }
            }

            free_list_add(NS);
        }

        /* find again */
        NS = find_free_ng();

        if (step < 256 && !fail)
            step *= 2;
    }

    Node node = NS->allocate_node();
    //NG中没有任何可分配的Node
    if (NS->is_full())
    {
        list_del(NS);
        full_list_add(NS);
    }

    if (!node)
    {
        snprintf(_errmsg, sizeof(_errmsg), "PANIC: allocate node failed");
        return Node();
    }

    //statistic
    _ngInfo->ni_used_node++;
    statUsedNode = _ngInfo->ni_used_node;

    //insert to node_index
    I_INSERT(node);
    return node;
}

int NGInfo::release_node(Node &node)
{
    NODE_SET *NS = node.Owner();
    if (NS->is_full())
    {
        //NG挂入空闲链表
        list_del(NS);
        free_list_add(NS);
    }

    _ngInfo->ni_used_node--;
    statUsedNode = _ngInfo->ni_used_node;
    return node.Release();
}

Node NGInfo::dirty_node_head()
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    if (!sysNG)
        return Node();
    return Node(sysNG, SYS_DIRTY_NODE_INDEX);
}

Node NGInfo::clean_node_head()
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    if (!sysNG)
        return Node();
    return Node(sysNG, SYS_CLEAN_NODE_INDEX);
}

Node NGInfo::empty_node_head()
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    if (!sysNG)
        return Node();
    return Node(sysNG, SYS_EMPTY_NODE_INDEX);
}

int NGInfo::insert2_dirty_lru(Node node)
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    Node dirtyNode(sysNG, SYS_DIRTY_NODE_INDEX);

    NODE_LIST_ADD(node, dirtyNode);

    return 0;
}

int NGInfo::insert2_clean_lru(Node node)
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    Node cleanNode(sysNG, SYS_CLEAN_NODE_INDEX);

    NODE_LIST_ADD(node, cleanNode);

    return 0;
}

int NGInfo::insert2_empty_lru(Node node)
{
    NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
    Node emptyNode(sysNG, SYS_EMPTY_NODE_INDEX);

    NODE_LIST_ADD(node, emptyNode);

    return 0;
}

int NGInfo::remove_from_lru(Node node)
{
    NODE_LIST_DEL(node);
    return 0;
}

NODE_SET *NGInfo::allocate_ng(void)
{
    MEM_HANDLE_T v = M_CALLOC(NODE_SET::Size());
    if (INVALID_HANDLE == v)
    {
        snprintf(_errmsg, sizeof(_errmsg), "allocate nodegroup failed, %s", M_ERROR());
        return (NODE_SET *)0;
    }

    NODE_SET *NS = M_POINTER(NODE_SET, v);
    NS->Init(_ngInfo->ni_min_id);
    _ngInfo->ni_min_id += NODE_GROUP_INCLUDE_NODES;
    _ngInfo->ni_used_ng++;
    statUsedNG = _ngInfo->ni_used_ng;

    return NS;
}

NODE_SET *NGInfo::find_free_ng(void)
{
    //链表为空
    if (NG_LIST_EMPTY(&(_ngInfo->ni_free_head)))
    {
        return (NODE_SET *)0;
    }

    return NG_LIST_ENTRY(_ngInfo->ni_free_head.Next(), NODE_SET, ng_list);
}

void NGInfo::list_del(NODE_SET *NS)
{
    NG_LIST_T *p = &(NS->ng_list);
    return NG_LIST_DEL(p);
}

#define EXPORT_NG_LIST_FUNCTION(name, member, function) \
    void NGInfo::name(NODE_SET *NS)                 \
    {                                                   \
        NG_LIST_T *p = &(NS->ng_list);                  \
        NG_LIST_T *head = &(_ngInfo->member);           \
        return function(p, head);                       \
    }

EXPORT_NG_LIST_FUNCTION(free_list_add, ni_free_head, NG_LIST_ADD)
EXPORT_NG_LIST_FUNCTION(full_list_add, ni_full_head, NG_LIST_ADD)
EXPORT_NG_LIST_FUNCTION(free_list_add_tail, ni_free_head, NG_LIST_ADD_TAIL)
EXPORT_NG_LIST_FUNCTION(full_list_add_tail, ni_full_head, NG_LIST_ADD_TAIL)

int NGInfo::InitHeader(NG_INFO_T *ni)
{
    INIT_NG_LIST_HEAD(&(ni->ni_free_head));
    INIT_NG_LIST_HEAD(&(ni->ni_full_head));

    ni->ni_min_id = SYS_MIN_NODE_ID;

    /* init system reserved zone*/
    {
        NODE_SET *sysNG = allocate_ng();
        if (!sysNG)
            return -1;

        sysNG->system_reserved_init();
        ni->ni_sys_zone = M_HANDLE(sysNG);
    }

    ni->ni_used_ng = 1;
    ni->ni_used_node = 0;
    ni->ni_dirty_node = 0;
    ni->ni_used_row = 0;
    ni->ni_dirty_row = 0;

    statUsedNG = ni->ni_used_ng;
    statUsedNode = ni->ni_used_node;
    statDirtyNode = ni->ni_dirty_node;
    statDirtyRow = ni->ni_dirty_row;
    statUsedRow = ni->ni_used_row;
    statEmptyNode = 0;

    return 0;
}

int NGInfo::Init(void)
{
    //1. malloc ng_info mem.
    MEM_HANDLE_T v = M_CALLOC(sizeof(NG_INFO_T));
    if (INVALID_HANDLE == v)
    {
        snprintf(_errmsg, sizeof(_errmsg), "init nginfo failed, %s", M_ERROR());
        return -1;
    }

    //2. mapping
    _ngInfo = M_POINTER(NG_INFO_T, v);

    //3. init header
    return InitHeader(_ngInfo);
}

int NGInfo::Attach(MEM_HANDLE_T v)
{
    if (INVALID_HANDLE == v)
    {
        snprintf(_errmsg, sizeof(_errmsg), "attach nginfo failed, memory handle = 0");
        return -1;
    }

    _ngInfo = M_POINTER(NG_INFO_T, v);

    /* check system reserved zone:
     *   1. the present of empty lru list
     */
    {
        NODE_SET *sysNG = M_POINTER(NODE_SET, _ngInfo->ni_sys_zone);
        if (!sysNG)
            return -1;

        int ret = sysNG->system_reserved_check();
        if (ret < 0)
            return ret;
        if (ret > 0)
        {
            emptyStartupMode = UPGRADED;
        }
        else
        {
            emptyStartupMode = ATTACHED;
        }
    }

    return 0;
}

int NGInfo::Detach(void)
{
    _ngInfo = NULL;
    return 0;
}
