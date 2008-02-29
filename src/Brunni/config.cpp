//Brunni's portable config system (qui déchire trop sa race... hem en fait non)
#include "config.h"
#include "../../src/System.h"
#include "../../src/NLS.h"
#include "../../src/gb/GB.h"
#include "../../src/gb/gbCheats.h"
#include "../../src/gb/gbGlobals.h"
#include "../../src/gb/gbMemory.h"
#include "../../src/gb/gbSGB.h"
#include "../../src/gb/gbSound.h"

int gblConfigAutoShowCrc=0;
int retourScript;
int arretdurgence=0;
char *trouveLabel=NULL, chneLabel[1000];
enum types_objets {TYPE_FONCTION, TYPE_ENTIER, TYPE_BOOL, TYPE_CHAINE, TYPE_REEL, TYPE_OBJET, TYPE_TABLEAU};
//Indique que ce n'est pas sauvé automatiquement
#define FLAG_NOSAVE 0x1
//Pas encore utilisé
#define FLAG_CALLBACK 0x2
//Pas sauver pour les fichiers de config des jeux
#define FLAG_NOSAVEGAME 0x4

//Octets utilisés pour les types
#define MASQUE_TYPE 15
//Taille max (0 à 15: 0 = 1, 1 = 2, 2 = 3, 4 = 16, etc.)
#define TYPE_MAXSIZE(x)				((x) << 4)
#define TYPE_MAXSIZE_MASK(flags)	((flags >> 4) & 15)
#define ScriptErrorEx(...)	{ char __str[1000]; sprintf(__str , __VA_ARGS__); ScriptError(__str); }

void ScriptError(char *msg);

VIRTUAL_FILE *gblFichierConfig;
int bModeWideCrLf=1;
int gblIndice;
char gblVarName[TAILLE_MAX_CHAINES];
enum {FCT_SET=1, FCT_GET, FCT_SET_BEFORE, FCT_SET_AFTER};
char tempValue[TAILLE_MAX_CHAINES];
int GetEntier(char *valeur);
int atox(char *chn);
char *ProchArgEntier(char *ptr);
char *GetChaine(char **dest);
#define SAUTE_ESPACES(ptr)	 { while(*ptr==' ' || *ptr=='	') ptr++; }

int myMessageBox(const char *msg, const char *title, int buttons)		{
	return MessageBox(NULL, msg, title, buttons);
}


char *FctFin(int type, char *ptr)		{
	arretdurgence=1;
	return NULL;
}

char *FctGoto(int type, char *ptr)		{
	//Note: les labels ne peuvent être qu'en dessous
	char *nom = GetChaine(&ptr);
	if (nom)		{
		strcpy(chneLabel, nom);
		trouveLabel = chneLabel;
	}
	return NULL;
}

char *FctFaire(int type, char *ptr)			{
	char *nom, *fct;
	nom=GetChaine(&ptr);
	while(*ptr!=',' && *ptr!=':' && *ptr)
		ptr++;
	if (*ptr)
		ptr++;
	while(*ptr!='\"' && *ptr)
		ptr++;
	fct = GetChaine(&ptr);
	if (nom)			{
		if (!OuvreFichierConfig(nom, fct))			{
			ScriptErrorEx("Unable to execute the specified file: %s", nom);
		}
	}
	else
		ScriptError("Function: Do\nFilename not defined correctly (don't forget the quote marks)");
	return NULL;
}

char *FctMsgBox(int type, char *ptr)		{
	char *chne, *label, *tmp, *titre;

	chne=GetChaine(&ptr);
	if (!chne)
		chne = "Breakpoint defined here (MsgBox without argument).";

	titre = "User message";

//prochArg:
	if (*ptr==',')
		ptr++;
	while(*ptr==' ' || *ptr=='	')		ptr++;

	tmp=GetChaine(&ptr);

	label=NULL;

	if (!tmp)		{
		if (!strncmp(ptr, "else goto ", 10))
			label=ptr+10;
	}
	else		{
		titre=tmp;
//		goto prochArg;
	}

	if (myMessageBox(chne, titre, MB_OKCANCEL) == 0)		{
		if (label)		{
			strcpy(chneLabel,label);
			trouveLabel=chneLabel;
		}
		else
			arretdurgence=1;
	}
	return NULL;
}

extern byte tilePalList[384];
extern short tilePalPalette[NB_PALETTES][4];
extern byte tilePalCrc[384 / 8];
extern short tileMapTileList[384];
char gblColorItPaletteFileName[MAX_PATH];

void FctColorIt_SetPalette(int type, char *ptr)		{
	int palNo = GetEntier(ptr), color, i;
	if (palNo < 0 || palNo >= NB_PALETTES)
		return;
	for (i=0;i<4;i++)			{
		ptr = ProchArgEntier(ptr);
		if (*ptr)		{
			color = GetEntier(ptr) | 0xff000000;
			gbPalette[palNo * 4 + i] = oslConvertColor(OSL_PF_5551, OSL_PF_8888, color);
		}
	}
//	gb_invalidate_palette(palNo);
}

void FctColorIt_AddTileRule(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0, palNo, i;

	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	//+tileno signifie que c'est une tile dans la zone haute de la VRAM (+128)
	if (*ptr == '+')	{
		tileEnd = 128;
		ptr++;
	}
	tileEnd += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	if (*ptr)
		palNo = GetEntier(ptr);
	else	{
		//Only two args => the second is the palette number
		palNo = tileEnd;
		tileEnd = tileStart;
	}

	//Vérifie que les valeurs fournies sont correctes
	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384 || palNo < 0 || palNo >= NB_PALETTES)
		return;

	//Définit les palettes des tiles
	for (i=tileStart;i<=tileEnd;i++)
		tilePalList[i] = palNo;
}

void FctColorIt_AddTileCrc(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0;
	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	//+tileno signifie que c'est une tile dans la zone haute de la VRAM (+128)
	if (*ptr == '+')	{
		tileEnd = 128;
		ptr++;
	}
	tileEnd += GetEntier(ptr);
	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384)
		return;
	//Savoir si une tile a son crc compté:
	//(tilePalCrc[tileNo >> 8] & (1 << (tileNo & 7)))
	for (;tileStart<=tileEnd;tileStart++)
		tilePalCrc[tileStart >> 3] |= 1 << (tileStart & 7);
}

//Additional RAM for custom tiles - needed on VBA only, other emulators can use the space reserved for GBC (bank 1)
u8 *getGbExtVramAddr()		{
	//Allocate memory if it doesn't exists
	if (!gbExternalVram)
		gbExternalVram = (u8*)malloc(0x2000);
	return gbExternalVram;
}

void FctColorIt_SetTilesetData(int type, char *ptr)		{
	int tileNo = 0, i, j;
	char *text;
	char mode = 'N';
	u8 *tilePtr;
	//We need to do this with VBA because of the separate GB/GBC handling
	u8 *extVram = getGbExtVramAddr();

	//Récupère les arguments
	if (*ptr == '+')	{
		tileNo = 128;
		ptr++;
	}
	tileNo += GetEntier(ptr);

	if (tileNo < 0 || tileNo >= 384)
		return;

	ptr = ProchArgEntier(ptr);
	if (*ptr == 'l')		{
		ptr++;
		mode = 'L';
		while (*ptr == ' ' || *ptr == '	')
			ptr++;
	}
	text = GetChaine(&ptr);
	if (!text)
		return;

	//We have two VRAM banks just like the game boy color, the second one being used for the custom tileset.
//	tileNo += 512;
	tilePtr = (u8*)extVram + (tileNo << 4);

	//Mode normal: données RAW au format GB
	if (mode == 'N')		{
		//16 bytes per tile
		for (i=0;i<16;i++)		{
			//Récupération d'un nombre (doublet) hexadécimal
			u8 value1, value2;
			do	{
				value1 = *text++;
			} while (value1 == ' ');
			if (!value1)
				break;
			do	{
				value2 = *text++;
			} while (value2 == ' ');
			if (!value2)
				break;
			if (value1 >= 'a' && value1 <= 'f')
				value1 -= 'a' - ('9' + 1);
			if (value2 >= 'a' && value2 <= 'f')
				value2 -= 'a' - ('9' + 1);
			value1 -= '0';		
			value2 -= '0';

			//Store the final value
			*tilePtr++ = value2 + (value1 << 4);
		}
	}
	else if (mode == 'L')		{
		//8*8 pixels per tile
		for (j=0;j<8;j++)		{
			for (i=7;i>=0;i--)		{
				//Récupération d'un nombre
				u8 value;
				do	{
					value = *text++;
				} while (value == ' ');

				//Affectation des bits selon la valeur (1 à 4)
				if (value == '0' || value == '3')
					tilePtr[0] |= 1 << i;
				else
					tilePtr[0] &= ~(1 << i);

				if (value == '2' || value == '3')
					tilePtr[1] |= 1 << i;
				else
					tilePtr[1] &= ~(1 << i);
			}

			//2 bytes per line
			tilePtr += 2;
		}
	}
}

void FctColorIt_SetTile(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0, newTile = 0;
	int redo = 2;

	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);

	tileEnd = tileStart;
	while (redo--)		{
		//Récupère les arguments
		if (*ptr == 'o')						//o like original (original tileset)
			ptr++, newTile = 0;
		else
			newTile = 512;
		if (*ptr == '+')	{
			newTile += 128;
			ptr++;
		}
		newTile += GetEntier(ptr);
		ptr = ProchArgEntier(ptr);

		//Un troisième argument?
		if (*ptr)
			//Ne pas prendre si 512 a été ajouté car ça n'est valable que pour newTile
			tileEnd = newTile & 511;
		else
			redo = 0;
	}

	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384 || newTile < 0 || newTile >= 384 + 512)
		return;

	for (;tileStart<=tileEnd;)
		tileMapTileList[tileStart++] = newTile++;
}

/*const IDENTIFICATEUR objGameBoy[]=	{
	{"colorization",		&menuConfig.gameboy.colorization,	TYPE_ENTIER, FLAG_NOSAVEGAME},
	{"palette",				&menuConfig.gameboy.palette,		TYPE_CHAINE},
	{"gbType",				&menuConfig.gameboy.gbType,			TYPE_ENTIER},
};*/

const IDENTIFICATEUR objColorIt[]=	{
	//ColorIt.setPalette palNo, palColor0, palColor1, palColor2, palColor3
	{"setPalette",			FctColorIt_SetPalette,				TYPE_FONCTION},
	//ColorIt.addTileRule tileStart, tileEnd, palNo
	{"addTileRule",			FctColorIt_AddTileRule,				TYPE_FONCTION},
	//ColorIt.addTileCrc tileNo
	{"addTileCrc",			FctColorIt_AddTileCrc,				TYPE_FONCTION},
	//ColorIt.setTileData tileNo, "hextiledata"
	{"setTilesetData",		FctColorIt_SetTilesetData,			TYPE_FONCTION},
	//ColorIt.setTile tileNo, newTileNo
	{"setTile",				FctColorIt_SetTile,					TYPE_FONCTION},
	//Automatically displays the VRAM CRC when the VRAM changes
	{"autoShowVramCrc",		&gblConfigAutoShowCrc,				TYPE_BOOL},
};

//INFOS_OBJET infoObjGameBoy[]={numberof(objGameBoy), objGameBoy};
INFOS_OBJET infoObjColorIt[]={numberof(objColorIt), objColorIt};

const IDENTIFICATEUR objets[]=		{
//	{"gameboy",				infoObjGameBoy,			TYPE_OBJET},
	{"colorit",				infoObjColorIt,			TYPE_OBJET, FLAG_NOSAVE},

	//Standard du langage
	{"end",					FctFin,					TYPE_FONCTION, FLAG_NOSAVE},
	{"msgbox",				FctMsgBox,				TYPE_FONCTION, FLAG_NOSAVE},
	{"goto",				FctGoto,				TYPE_FONCTION, FLAG_NOSAVE},
	{"do",					FctFaire,				TYPE_FONCTION, FLAG_NOSAVE},
};

//Un objet qui les contient tous
INFOS_OBJET infoObjMain[]={numberof(objets), objets};

//Comparaison entre une chaîne à casse basse et mixte
int strlwrmixcmp(const char *lower, const char *mix)			{
	while(*lower || *mix)		{
		char c = *mix;
		if (c >= 'A' && c <= 'Z')
			c += 32;
		if (c != *lower)
			return -1;
		lower++, mix++;
	}
	return 0;
}

void ScriptBegin()		{
}

void ScriptEnd()		{
}

int atox(char *chn)	{
	int i;
	int n;
	int v;
	char *deb;
	deb=chn;
	while((*chn>='0' && *chn<='9') || (*chn>='a' && *chn<='f') || (*chn>='A' && *chn<='F'))
		chn++;
	chn--;
	i=1;
	n=0;
	while(chn>=deb)	{
		v=*(chn)-48;
		if (v>=17+32)	v-=39;
		if (v>=17)		v-=7;
		n+=i*v;
		i*=16;
		chn--;
	}
	return n;
}

//Idem mais en binaire
int atob(char *chn)	{
	int i;
	int n;
	int v;
	char *deb;
	deb=chn;
	while(*chn=='0' || *chn=='1')
		chn++;
	chn--;
	i=1;
	n=0;
	while(chn>=deb)	{
		v=*(chn)-48;
		n+=i*v;
		i*=2;
		chn--;
	}
	return n;
}

char *ProchArgEntier(char *ptr)		{
	int compteParentheses=0;

	while(1)		{
		if (*ptr==0)
			break;
		if (*ptr=='(')
			compteParentheses++;
		if (compteParentheses==0)		{
			if (*ptr==',' || *ptr==' ' || *ptr==')')		{
				ptr++;
				break;
			}
		}
		if (*ptr==')')
			compteParentheses--;
		ptr++;
	}
	while(*ptr==' ' || *ptr=='	')
		ptr++;
	return ptr;
}

int GetEntier(char *valeur)		{
	int r,v,b;

	if ((*valeur<'0' || *valeur>'9') && *valeur != '-')		{
		if (*valeur=='t' || *valeur=='v')
			return 1;
		else if (*valeur=='f')
			return 0;
		else if (!strncmp(valeur,"rgb",3))		{
			valeur+=3;
			while(*valeur!='(' && *valeur)	valeur++;
			if (!(*valeur))
				return 0;
			valeur++;
			r=GetEntier(valeur);
			valeur=ProchArgEntier(valeur);
			v=atoi(valeur);
			valeur=ProchArgEntier(valeur);
			b=GetEntier(valeur);
			return RGB(r,v,b);
		}
		return 0;
	}
	if (*valeur=='0')		{
		valeur++;
		if (*valeur=='x')
			return atox(valeur+1);
		else if (*valeur=='b')		{
			return atob(valeur+1);
		}
		else
			return atoi(valeur-1);
	}
	return atoi(valeur);
}

char *GetChaine(char **dest)		{
	char *retour, *chne;
	chne=*dest;
	if (*chne=='\"')		{
		chne++;
		retour=chne;
		while(*chne!='\"' && *chne)
			chne++;
	}
	else if (*chne=='\'')		{
		chne++;
		retour=chne;
		while(*chne!='\'' && *chne)
			chne++;
	}
	else if (*chne=='*')		{
		chne++;
		retour=chne;
		while(*chne!='*' && *chne)
			chne++;
	}
	else
		//Chaîne telle quelle
		return chne;
	if (!(*chne))
		return NULL;
	*chne=0;
	*dest=chne+1;
	return retour;
}

const IDENTIFICATEUR *TrouveIdentificateur(const IDENTIFICATEUR *source, int taille, char *objet)
{
	int i, bNotFound = 0;
	if (!source)		{
		source = (const IDENTIFICATEUR*)objets;
		taille = sizeof(objets)/sizeof(objets[0]);
		bNotFound = 1;
	}
	//Trouve la référence d'objet dans la table
	for (i=0;i<taille;i++)		{
		//Ne prend pas la casse de 'objet'
		if (!strlwrmixcmp(objet,(char*)source[i].nom))
			return &source[i];
	}
	return NULL;
}

void SetVariableValue(const IDENTIFICATEUR *id, char *ptr)		{
	char* (*laFonction)(int, char*);
	char *arg;
	int i, val;
	switch (id->type & MASQUE_TYPE)			{
		case TYPE_FONCTION:
			//Depuis que je m'amuse sur GBA avec ce genre de trucs, ça me semble déjà plus sûr comme méthode. Ne vous inquiétez pas si vous trouvez ça sale :p
			laFonction=(char* (*)(int, char*))id->handle;
			laFonction(FCT_SET, ptr);
			break;

		case TYPE_ENTIER:
		case TYPE_BOOL:
			val=GetEntier(ptr);
			*(int*)id->handle=val;
			break;
		case TYPE_REEL:
			*(double*)id->handle=atof(ptr);
			break;

		case TYPE_CHAINE:
			arg=GetChaine(&ptr);
			if (arg)			{
				int maxSize = 1 << TYPE_MAXSIZE_MASK(id->type);
				ptr=(char*)id->handle;
				for (i=0;i<maxSize-1 && *arg;i++)
					*ptr++=*arg++;
				(*ptr)=0;
			}
			break;
	}
}

//WithQuotes: pour inclure les guillemets autour des chaînes et autres
char *GetVariableValue(const IDENTIFICATEUR *id, int withQuotes)		{
	char* (*laFonction)(int, char*);
	int i;
	INFOS_TABLEAU *it = (INFOS_TABLEAU*)id->handle;

	switch (id->type & MASQUE_TYPE)			{
		case TYPE_FONCTION:
			//Depuis que je m'amuse sur GBA avec ce genre de trucs, ça me semble déjà plus sûr comme méthode. Ne vous inquiétez pas si vous trouvez ça sale :p
			laFonction=(char* (*)(int, char*))id->handle;
			return laFonction(FCT_GET, NULL);

		case TYPE_ENTIER:
			sprintf(tempValue, "%i", *(int*)id->handle);
			return tempValue;

		case TYPE_BOOL:
			strcpy(tempValue, (*(int*)id->handle) ? "true" : "false");
			return tempValue;

		case TYPE_TABLEAU:
			char tempValueTab[TAILLE_MAX_CHAINES];
			strcpy(tempValueTab, "{");
			for (i=0;i<it->taille;i++)			{
				IDENTIFICATEUR ident;
				char *val;
				int maxSize;
				switch (it->type & MASQUE_TYPE)			{
					case TYPE_ENTIER:
					case TYPE_BOOL:
						ident.handle = (int*)it->tableau+i;
						break;
					case TYPE_CHAINE:
						maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);
						ident.handle = (char *)it->tableau + maxSize * i;
						break;
					case TYPE_REEL:
						ident.handle = (double*)it->tableau+i;
						break;
				}
				ident.nom = NULL;
				ident.type = it->type;
				val = GetVariableValue(&ident, 1);
				if (val)		{
					strcat(tempValueTab, val);
					//Sépare les éléments d'une virgule, sauf le dernier
					if (i < it->taille - 1)
						strcat(tempValueTab, ", ");
				}
			}
			strcpy(tempValue, tempValueTab);
			strcat(tempValue, "}");
			return tempValue;

		case TYPE_REEL:
			sprintf(tempValue, "%f", *(float*)id->handle);
			return tempValue;

		case TYPE_CHAINE:
			if (!withQuotes)
				return (char*)id->handle;
			else		{
				strcpy(tempValue, "\"");
				strcat(tempValue, (char*)id->handle);
				strcat(tempValue, "\"");
				return tempValue;
			}

		default:
			return NULL;
	}
}

int OuvreFichierConfig(char *nom_fichier, char *fonction)		{
	VIRTUAL_FILE *f;

	retourScript=1;

	if (!(*nom_fichier))
		return 0;

	f = VirtualFileOpen((void*)nom_fichier, 0, VF_FILE, VF_O_READ);
	if (!f)
		return 0;

	return ExecScript(f,fonction, NULL);
}

//Command: si FILE == NULL
int ExecScript(VIRTUAL_FILE *f, char *fonction, char *command)		{
	char str[1000], objet[1000], commande[1000], *ptr, tmp[4096], fonction_reel[1000];
	int i,j, type;
	void *handle;
	INFOS_OBJET *infos;
	int indice;
	const IDENTIFICATEUR *idObjet, *idEnCours;
	int tailleEnCours;

	arretdurgence=0;

	retourScript=1;

	if (fonction)		{				//Une fonction sur laquelle se brancher?
		strcpy(fonction_reel, fonction);
		fonction = fonction_reel;
		//Pas case sensitive, bien entendu :p
		for (ptr=fonction;*ptr;ptr++)
			if (*ptr>='A' && *ptr<='Z')
				*ptr+=32;
	}

	trouveLabel=fonction;

	ScriptBegin();

	while(f?(!VirtualFileEof(f)):(*command))		{
		if (arretdurgence)
			break;
		if (f)
			VirtualFileGets(str,sizeof(str),f);
		else	{
			i=0;
			while(*command != ';' && *command && i<sizeof(str)-1)
				str[i++] = *command++;
			if (*command == ';')
				command++;
//			str[i++]='\n';
			str[i]=0;
		}

		if (!str[0])
			continue;

		//Tout d'abord: ne pas être "case sensitive"...
		for (i=0;str[i];i++)	{
			if (str[i]>='A' && str[i]<='Z')
				str[i]+=32;

			if (str[i]=='\"')	{
				i++;
				while(str[i] && (str[i]!='\"' || (str[i-1]=='\\' && str[i]=='\"')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='\'')	{
				i++;
				while(str[i] && (str[i]!='\'' || (str[i-1]=='\\' && str[i]=='\'')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='*')	{
				i++;
				while(str[i] && (str[i]!='*' || (str[i-1]=='\\' && str[i]=='*')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='#' || str[i]==';')		{
				str[i]=0;
				if (i>0)	{
					i--;
					while(str[i]==' ' || str[i]=='	' && i>=0)
						str[i--]=0;
				}
				goto parsingTermine;
			}
		}
		i--;

parsingTermine:
		if (str[i]==':' && !trouveLabel)		//Label trouvé!
			continue;

		idEnCours = NULL;
		tailleEnCours = 0;
		i = 0;
		indice = -1;

continueParse:
		j = 0;
		//Sauter les espaces/tabs éventuels
		while(str[i]==' ' || str[i]=='	')
			i++;

		//Bon, c'est pas le tout, mais on a un label à trouver nous!
		if (trouveLabel)		{
			if (str[i]=='_')
				i++;
			j=i;
			while(!(str[i]==':' && str[i+1]==0) && str[i])
				i++;
			if (!str[i])									//Pas un label
				continue;
			str[i]=0;
			if (strlwrmixcmp(str+j, trouveLabel))			//Pas le bon label
				continue;
			trouveLabel=NULL;								//Label trouvé -> plus à rechercher.
			continue;
		}

		//Après, récupérer l'objet de la commande (opérande gauche)
		while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='.' && str[i]!='=' && str[i]!='[')
			objet[j++]=str[i++];

		//Si ça n'a pas été terminé par un point
		while(str[i]!='[' && str[i]!='.' && str[i]!='=' && str[i] && str[i]!=' ')		i++;

		//Termine la chaîne
		objet[j]=0;

		//L'objet est faux?
		if (!objet[0])
			continue;

		//Permet ensuite de savoir si aucune commande n'a été donnée
		commande[0]=0;

		if (str[i]=='=' || str[i]==' ' || str[i]=='.' || str[i]=='[')				//Pas de méthode
			goto sauteEspaces;

		//Fin de chaîne -> seul un objet est spécifié
		if (!str[i])
			goto sauteEspaces;

		//Saute les espaces
		while(str[i]==' ' || str[i]=='	')	i++;

//commande:
		//Après, récupérer la commande à exécuter (opérande droite)
		j=0;
		while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='=' && str[i]!='[')
			commande[j++]=str[i++];

		//Termine la chaîne
		commande[j]=0;

		//Sauter les espaces/tabulations qui viennent ensuite
sauteEspaces:
		if (str[i]==' ' || str[i]=='	')		{
			while(str[i]==' ' || str[i]=='	')
				i++;
		}

		if (str[i]=='.')	{
			i++;
//			goto commande;
		}
		if (str[i]=='=')	{
			i++;
			goto sauteEspaces;
		}
		if (str[i]=='[')	{
			i++;
			indice=GetEntier(str+i);
			while(str[i] && str[i]!=']')
				i++;
			if (str[i])		{
				i++;
				goto sauteEspaces;
			}
		}

		//On est prêt pour exécuter une commande!
		ptr=str+i;

		handle=NULL;
		gblFichierConfig = f;

		idObjet = TrouveIdentificateur(idEnCours, tailleEnCours, objet);

		//Aucun résultat
		if (!idObjet)		{
			if (idEnCours)
				sprintf(tmp, "The property %s doesn't exist.", objet);
			else
				sprintf(tmp, "The identifier %s doesn't exist.", objet);
			ScriptError(tmp);
			arretdurgence = 1;
			continue;
		}
		else		{
			handle = idObjet->handle;
			type = idObjet->type;
		}

		gblIndice=indice;

		if (type==TYPE_OBJET)		{
			infos = (INFOS_OBJET*)handle;

			idEnCours = infos->objet;
			tailleEnCours = infos->taille;
			goto continueParse;
		}

		if (type==TYPE_TABLEAU)		{
			INFOS_TABLEAU *it;
			it = (INFOS_TABLEAU*)handle;
			if (indice < 0)		{
				if (*ptr == '{')			{
					//Callback avant
//					if (idObjet->flags & FLAG_CALLBACK)
//						return ((char* (*)(int, char*))idObjet->handle2)(FCT_SET_BEFORE, ptr);
					//Saute le {
					ptr++;
					SAUTE_ESPACES(ptr);
					//CHAÎNES PAS TESTÉES!!!!
					for (i=0;i<it->taille;i++)		{
						char *val = NULL, value[TAILLE_MAX_CHAINES];
						handle = NULL;
						//Plus rien
						if (!(*ptr))
							break;

						switch (it->type & MASQUE_TYPE)			{
							case TYPE_ENTIER:
								handle=(int*)it->tableau+i;
								val = value;
								sprintf(value, "%i", GetEntier(ptr));
								ptr = ProchArgEntier(ptr);
								break;

							case TYPE_BOOL:
								handle=(int*)it->tableau+i;
								val = value;
								strcpy(value, GetEntier(ptr) ? "true" : "false");
								ptr = ProchArgEntier(ptr);
								break;

							case TYPE_CHAINE:
								;
								int maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);
								handle = (char *)it->tableau + maxSize * i;
								val = GetChaine(&ptr);
								break;
						}

						if (handle && val)		{
							IDENTIFICATEUR ident;
							ident.nom = NULL;
							ident.type = it->type;
							ident.handle = handle;
							SetVariableValue(&ident, val);
						}
					}
					//Callback après
					if (idObjet->flags & FLAG_CALLBACK)
						((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
				}
				else	{
					//Le tableau ne peut être évalué (erreur de syntaxe)
					if (idObjet->flags & FLAG_CALLBACK)
						((char* (*)(int, char*))idObjet->handle2)(FCT_SET, ptr);
					else
						goto error1;
				}
			}
			else	{
				IDENTIFICATEUR ident;
				int maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);

				if (indice < 0 || indice >= it->taille)		{
error1:
					if (indice == -1)
						sprintf(tmp,"Array %s.\nSyntax error.", objet, indice);
					else
						sprintf(tmp,"Array %s.\nThe subscript (%i) is out of bounds.", objet, indice);
					ScriptError(tmp);
					continue;
				}

				switch (it->type & MASQUE_TYPE)			{
					case TYPE_ENTIER:
					case TYPE_BOOL:
						ident.handle=(int*)it->tableau+indice;
						break;

					case TYPE_CHAINE:
						ident.handle = (char *)it->tableau + maxSize * indice;
						break;

					case TYPE_REEL:
						ident.handle=(double*)it->tableau+indice;
						break;

					default:
						//Vous devriez pouvoir adapter pour d'autres types sans trop de problèmes ;)
						sprintf(tmp, "The array %s has an uncomputable type.", objet);
						ScriptError(tmp);
						continue;
				}
				ident.nom = NULL;
				ident.type = it->type;
				SetVariableValue(&ident, ptr);
				//Callback éventuel
				if (idObjet->flags & FLAG_CALLBACK)
					((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
			}
			continue;
		}

		SetVariableValue(idObjet, ptr);
		//Callback éventuel
		if (idObjet->flags & FLAG_CALLBACK)
			((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
//		if (arretdurgence)
//			break;
	}

	ScriptEnd();
	if (f)
		VirtualFileClose(f);
	return (trouveLabel == NULL);
}

const IDENTIFICATEUR *GetVariableAddress(char *str)		{
	char objet[1000], *ptr;
	int i,j, type;
	void *handle;
	INFOS_OBJET *infos;
	int indice;
	const IDENTIFICATEUR *idObjet, *idEnCours;
	int tailleEnCours;

	idEnCours = NULL;
	tailleEnCours = 0;
	i = indice = 0;

continueParse:
	j = 0;
	//Sauter les espaces/tabs éventuels
	while(str[i]==' ' || str[i]=='	')
		i++;

	//Après, récupérer l'objet de la commande (opérande gauche)
	while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='.' && str[i]!='[')
		objet[j++]=str[i++];

	//Si ça n'a pas été terminé par un point
	while(str[i]!='[' && str[i]!='.' && str[i] && str[i]!=' ')		i++;

	//Termine la chaîne
	objet[j]=0;

	//L'objet est faux?
	if (!objet[0])
		return NULL;

	if (str[i]==' ' || str[i]=='.' || str[i]=='[')				//Pas de méthode
		goto sauteEspaces;

	//Fin de chaîne -> seul un objet est spécifié
	if (!str[i])
		goto sauteEspaces;

	//Saute les espaces
	while(str[i]==' ' || str[i]=='	')	i++;

	//Sauter les espaces/tabulations qui viennent ensuite
sauteEspaces:
	if (str[i]==' ' || str[i]=='	')		{
		while(str[i]==' ' || str[i]=='	')
			i++;
	}

	if (str[i]=='.')	{
		i++;
//			goto commande;
	}
	if (str[i]=='[')	{
		i++;
		indice=GetEntier(str+i)-1;
		while(str[i] && str[i]!=']')
			i++;
		if (str[i])		{
			i++;
			goto sauteEspaces;
		}
	}

	//On est prêt pour exécuter une commande!
	ptr=str+i;

	handle=NULL;

	idObjet = TrouveIdentificateur(idEnCours, tailleEnCours, objet);

	//Aucun résultat
	if (!idObjet)		{
		return NULL;
	}
	else		{
		handle = idObjet->handle;
		type = idObjet->type;
	}

	gblIndice=indice;

	if (type==TYPE_OBJET)		{
		infos = (INFOS_OBJET*)handle;

		idEnCours = infos->objet;
		tailleEnCours = infos->taille;
		goto continueParse;
	}
	return idObjet;
}

void ScriptError(char *msg)		{
	myMessageBox(msg, "Script error", MB_OK);
	arretdurgence = 1;
}


/** ADDITIONAL CODE */
int oslConvertColor(int pfDst, int pfSrc, int color)		{
	int r=0, g=0, b=0, a=0;
	if (pfSrc == pfDst)
		return color;
	if (pfSrc == OSL_PF_8888)
		oslRgbaGet8888(color, r, g, b, a);
	else if (pfSrc == OSL_PF_5650)
		oslRgbGet5650f(color, r, g, b), a = 0xff;
	else if (pfSrc == OSL_PF_5551)
		oslRgbaGet5551f(color, r, g, b, a);
	else if (pfSrc == OSL_PF_4444)
		oslRgbaGet4444f(color, r, g, b, a);
	if (pfDst == OSL_PF_8888)
		color = RGBA(r, g, b, a);
	else if (pfDst == OSL_PF_5650)
		color = RGB16(r, g, b);
	else if (pfDst == OSL_PF_5551)
		color = RGBA15(r, g, b, a);
	else if (pfDst == OSL_PF_4444)
		color = RGBA12(r, g, b, a);
	return color;
}

int fileExists(char *fileName)		{
	VIRTUAL_FILE *f;
	int success = 0;
	f = VirtualFileOpen(fileName, 0, VF_FILE, VF_O_READ);
	if (f)		{
		success = 1;
		VirtualFileClose(f);
	}
	return success;
}

void GetJustFileName(char *dst, char *src)		{
	int i, lastPt = -1;
	for (i=0;src[i];i++)		{
		if (src[i] == '.')
			lastPt = i;
		dst[i] = src[i];
	}
	if (lastPt >= 0)
		dst[lastPt] = 0;
}

int CheckFichierPalette(char *file)		{
	//Try to open ROM-specific palette file
	GetJustFileName(gblColorItPaletteFileName, file);
	strcat(gblColorItPaletteFileName, ".pal.ini");

	if (!fileExists(gblColorItPaletteFileName))			{
		//Search in the shared folder, using the cart name
/*		sprintf(gblColorItPaletteFileName, "COLORPAK/%s.pal.ini", info.cart_name);
		if (!fileExists(gblColorItPaletteFileName))		{
			//If it still doesn't exists, use the file name
			char temp[MAX_PATH], *fname;
			fname = extractFilePath(menuFileSelectFileName, temp, 1);
			GetJustFileName(temp, fname);
			sprintf(gblColorItPaletteFileName, "COLORPAK/%s.pal.ini", temp);
		}*/
	}
	return fileExists(gblColorItPaletteFileName);
}

void OuvreFichierPalette(int crc, char *fonction)		{
	if (gblModeColorIt)			{
		if (fonction)
			OuvreFichierConfig(gblColorItPaletteFileName, fonction);
		else		{
			char tempCrc[100];
			sprintf(tempCrc, "[%08x]", crc);
			tilePalReinit();
			//Si la section correspondant au crc actuel n'existe pas, on utilise le profil par défaut [default].
			if (!OuvreFichierConfig(gblColorItPaletteFileName, tempCrc))
				OuvreFichierConfig(gblColorItPaletteFileName, "[default]");
		}
	}
}

char *extractFilePath(char *file, char *dstPath, int level)		{
	int i, curLevel = 0;
	char *fileName = dstPath;

	strcpy(dstPath, file);
	//Remove the last '/' character, so that we get the path in nameTemp and the file name in ptrFileName
	for (i=strlen(dstPath) - 1; i>=0; i--)		{
		if (dstPath[i] == '/' || dstPath[i] == '\\')		{
			if (curLevel == 0)
				fileName = dstPath + i + 1;
			//Suppression de level slashs
			if (++curLevel >= level)		{
				dstPath[i] = 0;
				break;
			}
		}
	}

	return fileName;
}

