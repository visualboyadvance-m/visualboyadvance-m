// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef _RAR_ARRAY_
#define _RAR_ARRAY_

template <class T>
class Rar_Array {
public:
	Rar_Array( Rar_Error_Handler* eh )
	{
		error_handler = eh;
		CleanData();
	}
	Rar_Array( int Size, Rar_Error_Handler* );
	~Rar_Array() { free( Buffer ); }
	
	T& operator [] ( int Item ) { return Buffer [Item]; }
	int Size() { return BufSize; }
	void Add(int Items);
	void Alloc(int Items);
	void Reset();
	void Push(T Item);

private:
	T *Buffer;
	int BufSize;
	int AllocSize;
	Rar_Error_Handler* error_handler;
	void CleanData()
	{
		Buffer = NULL;
		BufSize = 0;
		AllocSize = 0;
	}
};

template <class T>
Rar_Array<T>::Rar_Array( int Size, Rar_Error_Handler* eh )
{
	error_handler = eh;
	AllocSize=BufSize=Size;
	Buffer=(T *)malloc(sizeof(T)*Size);
	if (Buffer==NULL && BufSize!=0)
		rar_out_of_memory( error_handler );
}

template <class T>
void Rar_Array<T>::Add(int Items)
{
	BufSize+=Items;
	if (BufSize>AllocSize)
	{
		int Suggested=AllocSize+AllocSize/4+32;
		
		int NewSize = BufSize;
		if ( NewSize < Suggested )
			NewSize = Suggested;

		Buffer=(T *)realloc(Buffer,NewSize*sizeof(T));
		if (Buffer==NULL)
			rar_out_of_memory( error_handler );
		AllocSize=NewSize;
	}
}


template <class T>
void Rar_Array<T>::Alloc(int Items)
{
	if ( Items > AllocSize )
		Add( Items - BufSize );
	else
		BufSize = Items;
}


template <class T>
void Rar_Array<T>::Reset()
{
	free( Buffer );
	CleanData();
}

template <class T>
void Rar_Array<T>::Push(T Item)
{
	Add( 1 );
	(*this) [Size() - 1] = Item;
}

#endif
