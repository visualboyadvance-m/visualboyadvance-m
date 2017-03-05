// on win32 and mac, pointer typedefs only happen with AL_NO_PROTOTYPES
// on mac, ALC_NO_PROTOTYPES as well

#define AL_NO_PROTOTYPES 1

// on mac, alc pointer typedefs ony happen for ALC if ALC_NO_PROTOTYPES
// unfortunately, there is a bug in the system headers (use of ALCvoid when
// void should be used; shame on Apple for introducing this error, and shame
// on Creative for making a typedef to void in the first place)
//#define ALC_NO_PROTOTYPES 1

#include <al.h>
#include <alc.h>

// since the ALC typedefs are broken on Mac:

#ifdef __WXMAC__
typedef ALCcontext*(ALC_APIENTRY* LPALCCREATECONTEXT)(ALCdevice* device, const ALCint* attrlist);
typedef ALCboolean(ALC_APIENTRY* LPALCMAKECONTEXTCURRENT)(ALCcontext* context);
typedef void(ALC_APIENTRY* LPALCDESTROYCONTEXT)(ALCcontext* context);
typedef ALCdevice*(ALC_APIENTRY* LPALCOPENDEVICE)(const ALCchar* devicename);
typedef ALCboolean(ALC_APIENTRY* LPALCCLOSEDEVICE)(ALCdevice* device);
typedef ALCboolean(ALC_APIENTRY* LPALCISEXTENSIONPRESENT)(ALCdevice* device,
    const ALCchar* extname);
typedef const ALCchar*(ALC_APIENTRY* LPALCGETSTRING)(ALCdevice* device, ALCenum param);
#endif
