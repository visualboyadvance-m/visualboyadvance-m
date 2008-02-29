#include "common.h"

/*
	Note: seek ne retourne plus rien!
*/

VIRTUAL_FILE_SOURCE *VirtualFileSources[VF_MAX_SOURCES];
int VirtualFileSourcesNb = 0;
int VF_MEMORY = -1;
//List of files for RAM based devices
OSL_VIRTUALFILENAME *osl_virtualFileList;
//Number of entries
int osl_virtualFileListNumber = 0;
//For system use
int osl_virtualFileListSize;
int osl_defaultVirtualFileSource = -1;
const char *osl_tempFileName = "*tmp*";
OSL_VIRTUALFILENAME osl_tempFile;

#define DEFAULT_TABLE_SIZE 128


//Enregistre une nouvelle source
int VirtualFileRegisterSource(VIRTUAL_FILE_SOURCE *vfs)		{
	//Espace libre?
	if (VirtualFileSourcesNb >= VF_MAX_SOURCES)
		return -1;
	//Ajoute la source
	VirtualFileSources[VirtualFileSourcesNb] = vfs;
	return VirtualFileSourcesNb++;
}

VIRTUAL_FILE *VirtualFileOpen(void *param1, int param2, int type, int mode)			{
	VIRTUAL_FILE *f = NULL;
	if (type == VF_AUTO)		{
		if (param2 == 0)			{
		   OSL_VIRTUALFILENAME *file = oslFindFileInVirtualFilenameList((const char*)param1, type);
		   if (file)		{
			   param1 = file->data;
			   param2 = file->size;
			   type = *file->type;
			}
		   else
			   type = osl_defaultVirtualFileSource;
		}
	}

	if (type >= 0)			{
		f = (VIRTUAL_FILE*)malloc(sizeof(*f));
		if (f)		{
			memset(f, 0, sizeof(*f));
			f->type = type;
			if (!VirtualFileGetSource(f)->fOpen(param1, param2, type, mode, f))			{
				free(f);
				f = NULL;
			}
		}
	}
	return f;
}

int VirtualFileClose(VIRTUAL_FILE *f)			{
	int result = VirtualFileGetSource(f)->fClose(f);
	if (result)
		free(f);
	return result;
}

/*
	SOURCE 1 par défaut: Mémoire
*/

int vfsMemOpen(void *param1, int param2, int type, int mode, VIRTUAL_FILE* f)			{
	//Tous les modes sont supportés mais pas très bien pour l'instant, ne vous amusez pas à écrire et lire en même temps
//	if (mode != VF_O_READ || mode != VK_O_WRITE)
//		return 0;

	//C'est un nom de fichier?
	if (param2 == 0)			{
	   OSL_VIRTUALFILENAME *file = oslFindFileInVirtualFilenameList((const char*)param1, type);
	   if (file)		{
		   param1 = file->data;
		   param2 = file->size;
		}
	}
		
	//Initialisation du bloc mémoire
	f->offset = 0;
	f->ioPtr = param1;
	f->maxSize = param2;
	return 1;
}

int vfsMemClose(VIRTUAL_FILE *f)				{
	return 1;
}

int vfsMemWrite(const void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	int realSize = size * n, writeSize = 0;
	if (f->ioPtr)		{
		//Débordement?
		writeSize = oslMin(realSize, f->maxSize - f->offset);
		if (writeSize > 0)		{
			memcpy((char*)f->ioPtr + f->offset, ptr, writeSize);
			//Brunni: ??? this is probably wrong ??? (08.07.2007)
//			f->offset += realSize;
			f->offset += writeSize;
		}
	}
	return writeSize;
}

int vfsMemRead(void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	int readSize = 0, realSize = size * n;

	if (f->ioPtr)		{
		//min => pour éviter les débordements
		readSize = oslMin(realSize, f->maxSize - f->offset);
		if (readSize > 0)		{
			memcpy(ptr, (char*)f->ioPtr + f->offset, readSize);
			//Brunni: Removed (08.07.2007)
//			f->offset += realSize;
			f->offset += readSize;
		}
	}
	return readSize;
}

int vfsMemGetc(VIRTUAL_FILE *f)		{
	unsigned char car;
	//Pour la sécurité, quand même un cas à part pour les fichiers
	if (VirtualFileRead(&car, sizeof(car), 1, f) < 1)
		return -1;
	else
		return (int)car;
}

int vfsMemPutc(int caractere, VIRTUAL_FILE *f)		{
	unsigned char car = caractere;
	if (VirtualFileWrite(&car, sizeof(car), 1, f) < 1)
		return -1;
	else
		return caractere;
}

char *vfsMemGets(char *str, int maxLen, VIRTUAL_FILE *f)			{
	const int blockSize = 16;
	int offset = 0, i, size;
	while(1)			{
		size = VirtualFileRead(str + offset, 1, oslMin(maxLen - offset, blockSize), f);
		if (offset + size < maxLen)
			str[offset + size] = 0;
		for (i=offset;i<offset+blockSize;i++)		{
			if (str[i] == 0)
				return str;
			//\r\n (Windows)
			if (str[i] == '\r')			{
				str[i] = 0;
				//Dernier bloc de la liste?
				if (i + 1 >= offset + blockSize)			{
					char temp[1];
					int tempSize;
					tempSize = VirtualFileRead(temp, 1, 1, f);
					//Prochain caractère est un \n?
					if (!(tempSize > 0 && temp[0] == '\n'))
						//Sinon on annule
						i--;
				}
				else	{
					if (str[i + 1] == '\n')
						i++;
				}
				//Retourne le pointeur
				VirtualFileSeek(f, -size + (i - offset) + 1, SEEK_CUR);
				return str;
			}
			else if (str[i] == '\n')			{
				str[i] = 0;
				//ATTENTION: MODIFIE DE -blockSize + i à -blockSize + i + 1, à vérifier!!!
				VirtualFileSeek(f, -blockSize + i + 1, SEEK_CUR);
				//Retourne le pointeur
				return str;
			}
		}
		offset += blockSize;
	}
	return str;
}

void vfsMemPuts(const char *s, VIRTUAL_FILE *f)		{
	VirtualFileWrite(s, strlen(s), 1, f);
}

void vfsMemSeek(VIRTUAL_FILE *f, int offset, int whence)		{
//	int oldOffset = f->offset;
	if (f->ioPtr)		{
		if (whence == SEEK_SET)
			f->offset = offset;
		else if (whence == SEEK_CUR)
			f->offset += offset;
		else if (whence == SEEK_END)
			f->offset = f->maxSize + offset;
		f->offset = oslMax(oslMin(f->offset, f->maxSize), 0);
	}
//	return oldOffset;
}

int vfsMemTell(VIRTUAL_FILE *f)		{
	return f->offset;
}

int vfsMemEof(VIRTUAL_FILE *f)		{
	return (f->offset < f->maxSize);
}

VIRTUAL_FILE_SOURCE vfsMemory =		{
	vfsMemOpen,
	vfsMemClose,
	vfsMemRead,
	vfsMemWrite,
	vfsMemGetc,
	vfsMemPutc,
	vfsMemGets,
	vfsMemPuts,
	vfsMemSeek,
	vfsMemTell,
	vfsMemEof,
};

//Définit la liste de fichiers pour les sources basées sur la mémoire
/*void oslSetVirtualFilenameList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries)		{
   osl_virtualFileList = vfl;
   osl_virtualFileListNumber = numberOfEntries;
}*/

//Adds files to the list
int oslAddVirtualFileList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries)		{
	//Need to allocate more?
	if (numberOfEntries + osl_virtualFileListNumber > osl_virtualFileListSize)		{
		int finalSize = numberOfEntries + osl_virtualFileListNumber;
		OSL_VIRTUALFILENAME *v;
		//Align (we are only reallocating this by blocks of DEFAULT_TABLE_SIZE)
		if (finalSize % DEFAULT_TABLE_SIZE > 0)
			finalSize = finalSize - finalSize % DEFAULT_TABLE_SIZE + DEFAULT_TABLE_SIZE;
		v = (OSL_VIRTUALFILENAME*)realloc(osl_virtualFileList, finalSize * sizeof(OSL_VIRTUALFILENAME));
		if (v)		{
			osl_virtualFileList = v;
			osl_virtualFileListSize = finalSize;
		}
		else
			//Failed
			return 0;
	}
	//Copy the new entries
	memcpy(osl_virtualFileList + osl_virtualFileListNumber, vfl, numberOfEntries * sizeof(OSL_VIRTUALFILENAME));
	osl_virtualFileListNumber += numberOfEntries;
	return 1;
}

void oslRemoveVirtualFileList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries)			{
	int i;
	for (i=0;i<=osl_virtualFileListNumber-numberOfEntries;i++)		{
		//Try to find it in the list
		if (!memcmp(osl_virtualFileList + i, vfl, numberOfEntries * sizeof(OSL_VIRTUALFILENAME)))		{
			//Décale le tout
			if (osl_virtualFileListNumber - i - numberOfEntries > 0)
				memmove(osl_virtualFileList + i, osl_virtualFileList + i + numberOfEntries, osl_virtualFileListNumber - i - numberOfEntries);
			osl_virtualFileListNumber -= numberOfEntries;
		}
	}
}


OSL_VIRTUALFILENAME *oslFindFileInVirtualFilenameList(const char *fname, int type)			{
	int i;
	OSL_VIRTUALFILENAME *file;
	if (fname)			{
		//Skip the first / that means root for libFat
		if (fname[0] == '/')
			fname++;
		for (i=-1;i<osl_virtualFileListNumber;i++)			{
			//Include the temporary file in the search
			if (i == -1)
				file = &osl_tempFile;
			else
				file = osl_virtualFileList + i;
			//Null file type => impossible
			if (!file->type)
				continue;
			//Compare the type and the file name
			if ((type == *file->type || type == VF_AUTO)
				&& !strcmp(fname, file->name))
			return file;
		}
	}
	return NULL;
}

void VirtualFileInit()			{
   //On enregistre les sources par défaut
   if (VF_MEMORY < 0)
		VF_MEMORY = VirtualFileRegisterSource(&vfsMemory);
   if (VF_FILE < 0)
		VF_FILE = oslInitVfsFile();
	//By default, load from files
	oslSetDefaultVirtualFileSource(VF_FILE);

	//Allocate data for the virtual file list
	osl_virtualFileListSize = DEFAULT_TABLE_SIZE;
	osl_virtualFileListNumber = 0;
	//I suppose it never fails
	osl_virtualFileList = (OSL_VIRTUALFILENAME*)malloc(DEFAULT_TABLE_SIZE * sizeof(OSL_VIRTUALFILENAME));

	osl_tempFile.name = osl_tempFileName;
	osl_tempFile.type = NULL;
}

//Par blocs de 4 ko
#define BLOCK_SIZE (4 << 10)

//Lit un fichier entier vers la mémoire
void *oslReadEntireFileToMemory(VIRTUAL_FILE *f, int *fileSize)		{
   void *block = NULL;
   int add = 0;
   int size = 0, readSize, finalSize = 0;
   
   do		{
      size += BLOCK_SIZE;
      if (block)
		   block = realloc(block, size);
      else
		   block = malloc(size);

		//L'allocation a échoué?
		if (!block)
			return NULL;

      readSize = VirtualFileRead((char*)block + add, 1, BLOCK_SIZE, f);
	   add += BLOCK_SIZE;
	   finalSize += readSize;
   } while (readSize >= BLOCK_SIZE);
   
   if (fileSize)
   	*fileSize = finalSize;
   return block;
}

void oslSetTempFileData(void *data, int size, int *type)		{
	osl_tempFile.data = data;
	osl_tempFile.size = size;
	osl_tempFile.type = type;
}

