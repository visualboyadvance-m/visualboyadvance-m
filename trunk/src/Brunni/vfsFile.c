#include "common.h"

/*
	SOURCE VFS: file
*/

#define FLAG_EOF 1

int VF_FILE = -1;

#define _file_			((FILE*)f->ioPtr)

int vfsFileOpen(void *param1, int param2, int type, int mode, VIRTUAL_FILE* f)			{
	char *stdMode = "rb";
	if (mode == VF_O_WRITE)
		stdMode = "wb";
	else if (mode == VF_O_READWRITE)
		stdMode = "a+";
	
	f->ioPtr = (void*)fopen((char*)param1, stdMode);
	return (int)f->ioPtr;
}

int vfsFileClose(VIRTUAL_FILE *f)				{
	fclose(_file_);
	return 1;
}

int vfsFileWrite(const void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	return fwrite(ptr, size, n, _file_);
}

int vfsFileRead(void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	return fread(ptr, size, n, _file_);
}

int vfsFileGetc(VIRTUAL_FILE *f)		{
	return fgetc(_file_);
}

int vfsFilePutc(int caractere, VIRTUAL_FILE *f)		{
	return fputc(caractere, _file_);
}

char *vfsFileGets(char *str, int maxLen, VIRTUAL_FILE *f)			{
	return vfsMemGets(str, maxLen, f);
}

void vfsFilePuts(const char *s, VIRTUAL_FILE *f)		{
	fputs(s, _file_);
}

void vfsFileSeek(VIRTUAL_FILE *f, int offset, int whence)		{
	fseek(_file_, offset, whence);
}

int vfsFileTell(VIRTUAL_FILE *f)		{
	return ftell(_file_);
}

int vfsFileEof(VIRTUAL_FILE *f)		{
	return feof(_file_);
}

VIRTUAL_FILE_SOURCE vfsFile =		{
	vfsFileOpen,
	vfsFileClose,
	vfsFileRead,
	vfsFileWrite,
	vfsFileGetc,
	vfsFilePutc,
	vfsFileGets,
	vfsFilePuts,
	vfsFileSeek,
	vfsFileTell,
	vfsFileEof,
};

int oslInitVfsFile()		{
   VF_FILE = VirtualFileRegisterSource(&vfsFile);
   return VF_FILE;
}


