#include "configmanager.h"
int cheatsEnabled = true;
int saveType = 0;
int layerEnable = 0xff00;
int layerSettings = 0xff00;
bool parseDebug = true;
bool cpuIsMultiBoot = false;
int cpuDisableSfx = false;
bool mirroringEnable = true;
int skipBios = 0;
bool speedup = false;
bool speedHack = false;
const char* loadDotCodeFile;
const char* saveDotCodeFile;
int useBios = 0;
int rtcEnabled;
int cpuSaveType = 0;