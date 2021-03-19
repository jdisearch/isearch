/*
 * =====================================================================================
 *
 *       Filename:  ng_list.h
 *
 *    Description:  double linked list method in sharing memory.
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

#ifndef __DTC_NG_LIST_H
#define __DTC_NG_LIST_H

#include "namespace.h"
#include "global.h"

DTC_BEGIN_NAMESPACE

struct ng_list
{
    MEM_HANDLE_T prev;
    MEM_HANDLE_T next;

    struct ng_list *Next() { return M_POINTER(struct ng_list, next); }
    struct ng_list *Prev() { return M_POINTER(struct ng_list, prev); }
};
typedef struct ng_list NG_LIST_T;

#define INIT_NG_LIST_HEAD(ptr)          \
    do                                  \
    {                                   \
        MEM_HANDLE_T v = M_HANDLE(ptr); \
        (ptr)->prev = v;                \
        (ptr)->next = v;                \
    } while (0)

inline void __NG_LIST_ADD(NG_LIST_T *p,
                          NG_LIST_T *prev,
                          NG_LIST_T *next)
{
    next->prev = M_HANDLE(p);
    p->next = M_HANDLE(next);
    p->prev = M_HANDLE(prev);
    prev->next = M_HANDLE(p);
}

inline void NG_LIST_ADD(NG_LIST_T *p, NG_LIST_T *head)
{
    __NG_LIST_ADD(p, head, head->Next());
}

inline void NG_LIST_ADD_TAIL(NG_LIST_T *p, NG_LIST_T *head)
{
    __NG_LIST_ADD(p, head->Prev(), head);
}

inline void __NG_LIST_DEL(NG_LIST_T *prev, NG_LIST_T *next)
{
    next->prev = M_HANDLE(prev);
    prev->next = M_HANDLE(next);
}

inline void NG_LIST_DEL(NG_LIST_T *p)
{
    __NG_LIST_DEL(p->Prev(), p->Next());
    p->next = INVALID_HANDLE;
    p->prev = INVALID_HANDLE;
}

inline void NG_LIST_DEL_INIT(NG_LIST_T *p)
{
    __NG_LIST_DEL(p->Prev(), p->Next());
    INIT_NG_LIST_HEAD(p);
}

inline void NG_LIST_MOVE(NG_LIST_T *p, NG_LIST_T *head)
{
    __NG_LIST_DEL(p->Prev(), p->Next());
    NG_LIST_ADD(p, head);
}

inline void NG_LIST_MOVE_TAIL(NG_LIST_T *p, NG_LIST_T *head)
{
    __NG_LIST_DEL(p->Prev(), p->Next());
    NG_LIST_ADD_TAIL(p, head);
}

inline int NG_LIST_EMPTY(NG_LIST_T *head)
{
    return head->next == M_HANDLE(head);
}

#define OFFSETOF(type, member) (unsigned long)(&((type *)0)->member)

#define NG_LIST_ENTRY(ptr, type, member) \
    ((type *)((char *)(ptr)-OFFSETOF(type, member)))

#define NG_LIST_FOR_EACH(pos, head) \
    for (pos = (head)->Next(); pos != (head); pos = pos->Next())

#define NG_LIST_FOR_EACH_ENTRY(pos, head, member)                   \
    for (pos = NG_LIST_ENTRY((head)->Next(), typeof(*pos), member), \
        &pos->member != (head);                                     \
         pos = list_entry((pos->member).Next(), typeof(*pos), member))

DTC_END_NAMESPACE

#endif
