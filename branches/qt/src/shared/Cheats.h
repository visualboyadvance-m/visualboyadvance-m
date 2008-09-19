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

void cheatsAdd( const char *, const char *, u32, u32, u32, int, int );
void cheatsAddCheatCode( const char *code, const char *desc );
void cheatsAddGSACode( const char *code, const char *desc, bool v3 );
void cheatsAddCBACode( const char *code, const char *desc );
bool cheatsImportGSACodeFile( const char *name, int game, bool v3 );
void cheatsDelete( int number, bool restore );
void cheatsDeleteAll( bool restore );
void cheatsEnable( int number );
void cheatsDisable( int number );
void cheatsSaveGame( gzFile file );
void cheatsReadGame( gzFile file, int version );
void cheatsSaveCheatList( const char *file );
bool cheatsLoadCheatList( const char *file );
int cheatsCheckKeys( u32, u32 );

extern int cheatsNumber;
extern CheatsData cheatsList[100];

#endif
