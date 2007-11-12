// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

/****************************************************************************
 *  This file is part of PPMd project                                       *
 *  Written and distributed to public domain by Dmitry Shkarin 1997,        *
 *  1999-2000                                                               *
 *  Contents: memory allocation routines                                    *
 ****************************************************************************/

#include "rar.hpp"

#include "suballoc_inl.h"

SubAllocator::SubAllocator( Rar_Error_Handler* eh )
{
	error_handler = eh;
	Clean();
}

void SubAllocator::Clean()
{
	SubAllocatorSize=0;
}

void SubAllocator::StopSubAllocator()
{
	if ( SubAllocatorSize ) 
	{
		SubAllocatorSize=0;
		free( HeapStart );
		HeapStart = NULL;
	}
}

bool SubAllocator::StartSubAllocator(int SASize)
{
	uint t=SASize << 20;
	if (SubAllocatorSize == t)
		return true;
	StopSubAllocator();
	uint AllocSize=t/FIXED_UNIT_SIZE*UNIT_SIZE+UNIT_SIZE;
	HeapStart = (byte*) malloc( AllocSize );
	if ( !HeapStart )
		rar_out_of_memory( error_handler );
	
	HeapEnd=HeapStart+AllocSize-UNIT_SIZE;
	SubAllocatorSize=t;
	return true;
}

void SubAllocator::InitSubAllocator()
{
	int i, k;
	memset(FreeList,0,sizeof(FreeList));
	pText=HeapStart;
	uint Size2=FIXED_UNIT_SIZE*(SubAllocatorSize/8/FIXED_UNIT_SIZE*7);
	uint RealSize2=Size2/FIXED_UNIT_SIZE*UNIT_SIZE;
	uint Size1=SubAllocatorSize-Size2;
	uint RealSize1=Size1/FIXED_UNIT_SIZE*UNIT_SIZE+Size1%FIXED_UNIT_SIZE;
	HiUnit=HeapStart+SubAllocatorSize;
	LoUnit=UnitsStart=HeapStart+RealSize1;
	FakeUnitsStart=HeapStart+Size1;
	HiUnit=LoUnit+RealSize2;
	for (i=0,k=1;i < N1     ;i++,k += 1)
		Indx2Units[i]=k;
	for (k++;i < N1+N2      ;i++,k += 2)
		Indx2Units[i]=k;
	for (k++;i < N1+N2+N3   ;i++,k += 3)
		Indx2Units[i]=k;
	for (k++;i < N1+N2+N3+N4;i++,k += 4)
		Indx2Units[i]=k;
	for (GlueCount=k=i=0;k < 128;k++)
	{
		i += (Indx2Units[i] < k+1);
		Units2Indx[k]=i;
	}
}

void* SubAllocator::AllocUnitsRare(int indx)
{
	if ( !GlueCount )
	{
		GlueCount = 255;
		GlueFreeBlocks();
		if ( FreeList[indx].next )
			return RemoveNode(indx);
	}
	int i=indx;
	do
	{
		if (++i == N_INDEXES)
		{
			GlueCount--;
			i=U2B(Indx2Units[indx]);
			int j=12*Indx2Units[indx];
			if (FakeUnitsStart-pText > j)
			{
				FakeUnitsStart-=j;
				UnitsStart -= i;
				return(UnitsStart);
			}
			return(NULL);
		}
	} while ( !FreeList[i].next );
	void* RetVal=RemoveNode(i);
	SplitBlock(RetVal,i,indx);
	return RetVal;
}

void* SubAllocator::AllocContext()
{
	if (HiUnit != LoUnit)
		return (HiUnit -= UNIT_SIZE);
	if ( FreeList->next )
		return RemoveNode(0);
	return AllocUnitsRare(0);
}

void* SubAllocator::ExpandUnits(void* OldPtr,int OldNU)
{
	int i0=Units2Indx[OldNU-1], i1=Units2Indx[OldNU-1+1];
	if (i0 == i1)
		return OldPtr;
	void* ptr=AllocUnits(OldNU+1);
	if ( ptr ) 
	{
		memcpy(ptr,OldPtr,U2B(OldNU));
		InsertNode(OldPtr,i0);
	}
	return ptr;
}

void* SubAllocator::ShrinkUnits(void* OldPtr,int OldNU,int NewNU)
{
	int i0=Units2Indx[OldNU-1], i1=Units2Indx[NewNU-1];
	if (i0 == i1)
		return OldPtr;
	if ( FreeList[i1].next )
	{
		void* ptr=RemoveNode(i1);
		memcpy(ptr,OldPtr,U2B(NewNU));
		InsertNode(OldPtr,i0);
		return ptr;
	} 
	else 
	{
		SplitBlock(OldPtr,i0,i1);
		return OldPtr;
	}
}

void SubAllocator::FreeUnits(void* ptr,int OldNU)
{
	InsertNode(ptr,Units2Indx[OldNU-1]);
}
