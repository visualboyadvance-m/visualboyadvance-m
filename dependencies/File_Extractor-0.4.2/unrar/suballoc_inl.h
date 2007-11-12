// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

/****************************************************************************
 *  This file is part of PPMd project                                       *
 *  Written and distributed to public domain by Dmitry Shkarin 1997,        *
 *  1999-2000                                                               *
 *  Contents: interface to memory allocation routines                       *
 ****************************************************************************/

inline uint SubAllocator::U2B(int NU) 
{ 
	return /*8*NU+4*NU*/UNIT_SIZE*NU;
}

inline void* SubAllocator::RemoveNode(int indx) 
{
	RAR_NODE* RetVal=FreeList[indx].next;
	FreeList[indx].next=RetVal->next;
	return RetVal;
}

inline void* SubAllocator::AllocUnits(int NU)
{
	int indx=Units2Indx[NU-1];
	if ( FreeList[indx].next )
		return RemoveNode(indx);
	void* RetVal=LoUnit;
	LoUnit += U2B(Indx2Units[indx]);
	if (LoUnit <= HiUnit)
		return RetVal;
	LoUnit -= U2B(Indx2Units[indx]);
	return AllocUnitsRare(indx);
}

inline void SubAllocator::InsertNode(void* p,int indx) 
{
	((RAR_NODE*) p)->next=FreeList[indx].next;
	FreeList[indx].next=(RAR_NODE*) p;
}

inline void SubAllocator::SplitBlock(void* pv,int OldIndx,int NewIndx)
{
	int i, UDiff=Indx2Units[OldIndx]-Indx2Units[NewIndx];
	byte* p=((byte*) pv)+U2B(Indx2Units[NewIndx]);
	if (Indx2Units[i=Units2Indx[UDiff-1]] != UDiff) 
	{
		InsertNode(p,--i);
		p += U2B(i=Indx2Units[i]);
		UDiff -= i;
	}
	InsertNode(p,Units2Indx[UDiff-1]);
}

inline void SubAllocator::GlueFreeBlocks()
{
	RAR_MEM_BLK s0, * p, * p1;
	int i, k, sz;
	if (LoUnit != HiUnit)
		*LoUnit=0;
	for (i=0, s0.next=s0.prev=&s0;i < N_INDEXES;i++)
		while ( FreeList[i].next )
		{
			p=(RAR_MEM_BLK*)RemoveNode(i);
			p->insertAt(&s0);
			p->Stamp=0xFFFF;
			p->NU=Indx2Units[i];
		}
	for (p=s0.next;p != &s0;p=p->next)
		while ((p1=p+p->NU)->Stamp == 0xFFFF && int(p->NU)+p1->NU < 0x10000)
		{
			p1->remove();
			p->NU += p1->NU;
		}
	while ((p=s0.next) != &s0)
	{
		for (p->remove(), sz=p->NU;sz > 128;sz -= 128, p += 128)
			InsertNode(p,N_INDEXES-1);
		if (Indx2Units[i=Units2Indx[sz-1]] != sz)
		{
			k=sz-Indx2Units[--i];
			InsertNode(p+(sz-k),k-1);
		}
		InsertNode(p,i);
	}
}
