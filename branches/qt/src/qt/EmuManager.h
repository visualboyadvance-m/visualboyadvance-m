// The EmuManager class is the interface to the core.
// All communications with the core MUST be handled by this class for cleaner code.


#ifndef EMUMANAGER_H
#define EMUMANAGER_H


#include "precompile.h"

#include "../shared/System.h"


class EmuManager
{
public:
	enum SYSTEM_TYPE {
		SYSTEM_UNKNOWN,
		SYSTEM_GB,
		SYSTEM_GBA
	};

	EmuManager();
	~EmuManager();

	bool loadRom( const QString &filePath );
	void unloadRom();
	bool isRomLoaded();
	QString getRomPath();

	SYSTEM_TYPE getSystemType();

	bool startEmulation();
	void stopEmulation();
	bool isEmulating();
	void emulate(); // call to progress


private:
	QString romPath;
	unsigned char *romBuffer;
	int romSize;
	SYSTEM_TYPE systemType; // set by loadRom()
	bool romLoaded;
	bool emulating;
	struct EmulatedSystem emuSys;
};


#endif // #ifndef EMUMANAGER_H
