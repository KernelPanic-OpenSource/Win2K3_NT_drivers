//
//      clist.cpp
//
//      Copyright (c) 1996-2000 Microsoft Corporation.  All Rights Reserved.
//

#include "common.h"

ULONG CListItem::GetCount(void) const
{
    ULONG l;
    const CListItem *li;

    for(l=0,li=this; li!=NULL ; li=li->m_pNext,++l);
    return l;
}

BOOL CListItem::IsMember(CListItem *pItem)

{
    CListItem *li = this;
    for (;li != NULL; li=li->m_pNext)
    {
        if (li == pItem) return (TRUE);
    }
    return (FALSE);
}

CListItem* CListItem::Cat(CListItem *pItem)
{
    CListItem *li;

    if (this==NULL)
        return pItem;
    for(li=this ; li->m_pNext!=NULL ; li=li->m_pNext);
    li->m_pNext=pItem;
    return this;
}

CListItem* CListItem::Remove(CListItem *pItem)
{
    CListItem *li,*prev;

    if (pItem==this)
        return m_pNext;
    prev=NULL;
    for(li=this; li!=NULL && li!=pItem ; li=li->m_pNext)
        prev=li;
    if (li==NULL)     // item not found in list
        return this;

//  here it is guaranteed that prev is non-NULL since we checked for
//  that condition at the very beginning

    prev->SetNext(li->m_pNext);
    li->SetNext(NULL);
    return this;
}

CListItem* CListItem::GetPrev(CListItem *pItem) const
{
    const CListItem *li,*prev;

    prev=NULL;
    for(li=this ; li!=NULL && li!=pItem ; li=li->m_pNext)
        prev=li;
    return (CListItem*)prev;
}

CListItem * CListItem::GetItem(LONG index)
{
    CListItem *scan;
    for (scan = this; scan!=NULL && index; scan = scan->m_pNext) index--;
    return (scan);
}

// pItem is the new last element of the truncated list.
void CList::Truncate(CListItem *pItem)
{
    ASSERT(pItem);
    
    CListItem *pCutListHead = pItem->GetNext();

    if (pCutListHead)
    {
        m_ulCount -= pCutListHead->GetCount();
        pItem->SetNext(NULL);
    }
}

void CList::InsertAfter(CListItem *pItem, CListItem *pInsert)
{
    ASSERT(pItem); 
    ASSERT(pInsert);

    CListItem *next = pItem->GetNext();
    pItem->SetNext(pInsert);
    pInsert->SetNext(next);
    m_ulCount++;
}

void CList::InsertBefore(CListItem *pItem,CListItem *pInsert)
{
    ASSERT(pItem);
    ASSERT(pInsert);

    CListItem *prev = GetPrev(pItem);
    pInsert->SetNext(pItem);
    if (prev) prev->SetNext(pInsert);
    else m_pHead = pInsert;
    m_ulCount++;
}

