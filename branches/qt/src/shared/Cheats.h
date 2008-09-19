#ifndef GBA_CHEATS_H
#define GBA_CHEATS_H

struct CheatsData {
  int code;
  int size;
  int status;
  bool enabled;
  u32 rawaddress;
  u32 address;
  u32 value;
  u32 oldValue;
  char codestring[20];
  char desc[32];
};

extern void cheatsAdd(const char *,const char *,u32, u32,u32,int,int);
extern void cheatsAddCheatCode(const char *code, const char *desc);
extern void cheatsAddGSACode(const char *code, const char *desc, bool v3);
extern void cheatsAddCBACode(const char *code, const char *desc);
extern bool cheatsImportGSACodeFile(const char *name, int game, bool v3);
extern void cheatsDelete(int number, bool restore);
extern void cheatsDeleteAll(bool restore);
extern void cheatsEnable(int number);
extern void cheatsDisable(int number);
extern void cheatsSaveGame(gzFile file);
extern void cheatsReadGame(gzFile file, int version);
extern void cheatsSaveCheatList(const char *file);
extern bool cheatsLoadCheatList(const char *file);
extern int cheatsCheckKeys(u32,u32);
extern int cheatsNumber;
extern CheatsData cheatsList[100];
#endif // GBA_CHEATS_H
