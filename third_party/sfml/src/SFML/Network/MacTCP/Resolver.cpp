////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2025 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Name resolution and address text for the MacTCP socket layer.
//
// MacTCP's domain name resolver is not part of the driver: it is a 68k code
// resource, 'dnrp' ID 1, in the MacTCP control panel (or Open Transport's
// TCP/IP control panel, which keeps the MacTCP creator, or the System file).
// Apple's dnr.c loads it, locks it and calls it with a selector; this is the
// same procedure, with Mixed Mode doing the call from PowerPC code.
////////////////////////////////////////////////////////////

#include "Resolver.hpp"

#include "mactcp-sockets.h"

#include "SFML/System/MacOS/Microseconds.hpp"

#include <Multiverse.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

namespace
{
////////////////////////////////////////////////////////////
// AddressXlation.h
////////////////////////////////////////////////////////////
constexpr long kOpenResolver  = 1;
constexpr long kCloseResolver = 2;
constexpr long kStrToAddr     = 3;

#pragma pack(push, 2)
struct DnrHostInfo
{
    int32_t  rtnCode;
    char     cname[255];
    uint32_t addr[4];
};
#pragma pack(pop)

static_assert(sizeof(DnrHostInfo) == 276, "hostInfo must have MacTCP's layout");

// Called by the resolver (at interrupt time) when a lookup that answered
// cacheFault completes.
struct DnrWait
{
    volatile int32_t done;
};

pascal void dnrDone(DnrHostInfo* info, char* userData)
{
    (void)info;
    reinterpret_cast<DnrWait*>(userData)->done = 1;
}

Handle g_dnrCode = nullptr;

#if defined(__m68k__)

using DnrOpenProc      = OSErr (*)(long selector, char* fileName);
using DnrCloseProc     = OSErr (*)(long selector);
using DnrResultProc    = pascal void (*)(DnrHostInfo*, char*);
using DnrStrToAddrProc = OSErr (*)(long selector, char* hostName, DnrHostInfo* info, DnrResultProc done, char* userData);

OSErr callOpen()
{
    return reinterpret_cast<DnrOpenProc>(*g_dnrCode)(kOpenResolver, nullptr);
}

OSErr callClose()
{
    return reinterpret_cast<DnrCloseProc>(*g_dnrCode)(kCloseResolver);
}

OSErr callStrToAddr(char* hostName, DnrHostInfo* info, DnrWait* wait)
{
    return reinterpret_cast<DnrStrToAddrProc>(*g_dnrCode)(kStrToAddr, hostName, info, dnrDone, reinterpret_cast<char*>(wait));
}

#else // PowerPC: the resolver is 68k code

constexpr ProcInfoType kTwoBytes  = 2; // kTwoByteCode
constexpr ProcInfoType kFourBytes = 3; // kFourByteCode

constexpr ProcInfoType resultSize(ProcInfoType size)
{
    return size << kCallingConventionWidth;
}

constexpr ProcInfoType stackParam(int n, ProcInfoType size)
{
    return size << (kStackParameterPhase + (n - 1) * kStackParameterWidth);
}

constexpr ProcInfoType kOpenInfo = kCStackBased | resultSize(kTwoBytes) | stackParam(1, kFourBytes) |
                                   stackParam(2, kFourBytes);
constexpr ProcInfoType kCloseInfo     = kCStackBased | resultSize(kTwoBytes) | stackParam(1, kFourBytes);
constexpr ProcInfoType kStrToAddrInfo = kCStackBased | resultSize(kTwoBytes) | stackParam(1, kFourBytes) |
                                        stackParam(2, kFourBytes) | stackParam(3, kFourBytes) |
                                        stackParam(4, kFourBytes) | stackParam(5, kFourBytes);
constexpr ProcInfoType kResultInfo = kPascalStackBased | stackParam(1, kFourBytes) | stackParam(2, kFourBytes);

UniversalProcPtr g_doneUPP = nullptr;

UniversalProcPtr dnrProc()
{
    return reinterpret_cast<UniversalProcPtr>(*g_dnrCode);
}

OSErr callOpen()
{
    return static_cast<OSErr>(CallUniversalProc(dnrProc(), kOpenInfo, kOpenResolver, static_cast<char*>(nullptr)));
}

OSErr callClose()
{
    return static_cast<OSErr>(CallUniversalProc(dnrProc(), kCloseInfo, kCloseResolver));
}

OSErr callStrToAddr(char* hostName, DnrHostInfo* info, DnrWait* wait)
{
    if (!g_doneUPP)
        g_doneUPP = NewRoutineDescriptor(reinterpret_cast<ProcPtr>(dnrDone), kResultInfo, kPowerPCISA);
    return static_cast<OSErr>(CallUniversalProc(dnrProc(),
                                                kStrToAddrInfo,
                                                kStrToAddr,
                                                hostName,
                                                info,
                                                g_doneUPP,
                                                reinterpret_cast<char*>(wait)));
}

#endif

////////////////////////////////////////////////////////////
// Opens the first file of the given type and creator in a folder that
// holds the resolver; -1 if there is none. Like dnr.c's SearchFolderForDNRP.
short searchFolder(OSType type, OSType creator, short vRefNum, long dirID)
{
    for (short index = 1;; ++index)
    {
        unsigned char name[64] = {0};
        CInfoPBRec    pb{};
        pb.hFileInfo.ioNamePtr   = name;
        pb.hFileInfo.ioVRefNum   = vRefNum;
        pb.hFileInfo.ioFDirIndex = index;
        pb.hFileInfo.ioDirID     = dirID;
        if (PBGetCatInfoSync(&pb) != noErr)
            return -1;
        if (pb.hFileInfo.ioFlAttrib & 0x10) // a folder
            continue;
        if (pb.hFileInfo.ioFlFndrInfo.fdType != type || pb.hFileInfo.ioFlFndrInfo.fdCreator != creator)
            continue;

        const short ref = HOpenResFile(vRefNum, dirID, name, fsRdPerm);
        if (ref == -1)
            continue;
        if (Get1IndResource('dnrp', 1))
            return ref;
        CloseResFile(ref);
    }
}

short openResolverFile()
{
    constexpr short kOnSystemDisk = -32768;

    // The MacTCP control panel ('ztcp'; also Open Transport's TCP/IP panel),
    // then MacTCP 1.0's 'mtcp', in the System Folder and in Control Panels.
    const struct
    {
        OSType folder;
        OSType creator;
    } places[] = {{'ctrl', 'ztcp'}, {'macs', 'ztcp'}, {'macs', 'mtcp'}, {'ctrl', 'mtcp'}};

    for (const auto& place : places)
    {
        short   vRefNum = 0;
        int32_t dirID   = 0;
        if (FindFolder(kOnSystemDisk, place.folder, false, &vRefNum, &dirID) != noErr)
            continue;
        const short ref = searchFolder('cdev', place.creator, vRefNum, dirID);
        if (ref != -1)
            return ref;
    }
    return -1;
}

OSErr openResolver()
{
    if (g_dnrCode)
        return noErr;

    const short saved = CurResFile();
    const short ref   = openResolverFile();

    // Searches the whole chain: the control panel just opened, or the System
    // file where some installations keep it.
    Handle code = GetIndResource('dnrp', 1);
    if (code)
        DetachResource(code);
    if (ref != -1)
        CloseResFile(ref);
    UseResFile(saved);
    if (!code)
        return resNotFound;

    MoveHHi(code);
    HLock(code);
    g_dnrCode = code;

    const OSErr err = callOpen();
    if (err != noErr)
    {
        HUnlock(code);
        DisposeHandle(code);
        g_dnrCode = nullptr;
    }
    return err;
}

////////////////////////////////////////////////////////////
bool parseDotted(const char* text, uint32_t& address)
{
    uint32_t result = 0;
    for (int part = 0; part < 4; ++part)
    {
        if (*text < '0' || *text > '9')
            return false;
        unsigned value  = 0;
        int      digits = 0;
        while (*text >= '0' && *text <= '9')
        {
            value = value * 10 + static_cast<unsigned>(*text++ - '0');
            if (++digits > 3 || value > 255)
                return false;
        }
        result = (result << 8) | value;
        if (part < 3 && *text++ != '.')
            return false;
    }
    if (*text != '\0')
        return false;
    address = result;
    return true;
}

} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
int resolveName(const char* name, std::uint32_t& address)
{
    if (mactcp_available() != 0)
        return EAI_FAIL;
    if (openResolver() != noErr)
        return EAI_FAIL;

    // One lookup at a time: the link and the updater's libcurl may ask from
    // different (cooperative) threads, and the state below is shared.
    static bool busy = false;
    while (busy)
        if (!idleNap())
            return EAI_AGAIN;
    busy = true;
    struct Release
    {
        ~Release()
        {
            busy = false;
        }
    } release;

    // The resolver keeps using these until it calls back, so they must not
    // live on the stack: a lookup given up on below may still finish later.
    static char        hostName[256];
    static DnrHostInfo info;
    static DnrWait     wait;
    static bool        pending = false;

    if (pending)
    {
        // An earlier lookup timed out; let it finish before reusing its state.
        const std::uint64_t start = macMicroseconds();
        while (!wait.done && macMicroseconds() - start < 5'000'000)
            if (!idleNap())
                return EAI_AGAIN;
        if (!wait.done)
            return EAI_AGAIN;
        pending = false;
    }

    std::strncpy(hostName, name, sizeof(hostName) - 1);
    hostName[sizeof(hostName) - 1] = '\0';
    std::memset(&info, 0, sizeof(info));
    wait.done = 0;

    OSErr err = callStrToAddr(hostName, &info, &wait);
    if (err == cacheFault)
    {
        // A query went out; the resolver gives up on its own after its
        // retries, so this bound only guards against a callback never
        // coming.
        const std::uint64_t start = macMicroseconds();
        bool cancelled = false;
        while (!wait.done && macMicroseconds() - start < 60'000'000)
            if (!idleNap())
            {
                cancelled = true;
                break;
            }
        if (!wait.done || cancelled)
        {
            pending = true;
            return EAI_AGAIN;
        }
        err = static_cast<OSErr>(info.rtnCode);
    }

    switch (err)
    {
        case noErr:
            break;
        case noNameServer:
        case noAnsErr:
        case cacheFault:
            return EAI_AGAIN;
        case nameSyntaxErr:
        case authNameErr:
            return EAI_NONAME;
        case outOfMemory:
            return EAI_MEMORY;
        default:
            return EAI_FAIL;
    }
    if (info.addr[0] == 0)
        return EAI_NONAME;
    address = info.addr[0];
    return 0;
}

////////////////////////////////////////////////////////////
void closeResolver()
{
    if (!g_dnrCode)
        return;
    callClose();
    HUnlock(g_dnrCode);
    DisposeHandle(g_dnrCode);
    g_dnrCode = nullptr;
#if !defined(__m68k__)
    if (g_doneUPP)
    {
        DisposeRoutineDescriptor(g_doneUPP);
        g_doneUPP = nullptr;
    }
#endif
}

} // namespace sf::priv


namespace
{
////////////////////////////////////////////////////////////
// A host name or dotted address to an address in host byte order; 0 or an
// EAI_* code.
int lookupHost(const char* node, uint32_t& address)
{
    if (parseDotted(node, address))
        return 0;
    if (strcasecmp(node, "localhost") == 0)
    {
        // MacTCP has no loopback interface: this machine is its own address.
        address = ntohl(mactcp_local_address());
        if (address == 0)
            address = INADDR_LOOPBACK;
        return 0;
    }
    return sf::priv::resolveName(node, address);
}

} // namespace


extern "C" {

int h_errno = 0;

////////////////////////////////////////////////////////////
int inet_pton(int af, const char* src, void* dst)
{
    if (af != AF_INET)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }
    uint32_t address = 0;
    if (!parseDotted(src, address))
        return 0;
    const in_addr in{htonl(address)};
    std::memcpy(dst, &in, sizeof(in));
    return 1;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
    if (af != AF_INET)
    {
        errno = EAFNOSUPPORT;
        return nullptr;
    }
    in_addr in{};
    std::memcpy(&in, src, sizeof(in));
    const uint32_t a = ntohl(in.s_addr);
    char           text[INET_ADDRSTRLEN];
    std::snprintf(text,
                  sizeof(text),
                  "%u.%u.%u.%u",
                  static_cast<unsigned>((a >> 24) & 0xff),
                  static_cast<unsigned>((a >> 16) & 0xff),
                  static_cast<unsigned>((a >> 8) & 0xff),
                  static_cast<unsigned>(a & 0xff));
    if (std::strlen(text) + 1 > size)
    {
        errno = ENOSPC;
        return nullptr;
    }
    std::strcpy(dst, text);
    return dst;
}

in_addr_t inet_addr(const char* cp)
{
    uint32_t address = 0;
    return parseDotted(cp, address) ? htonl(address) : INADDR_NONE;
}

char* inet_ntoa(struct in_addr in)
{
    static char text[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in, text, sizeof(text));
    return text;
}

int inet_aton(const char* cp, struct in_addr* addr)
{
    uint32_t address = 0;
    if (!parseDotted(cp, address))
        return 0;
    if (addr)
        addr->s_addr = htonl(address);
    return 1;
}

////////////////////////////////////////////////////////////
int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res)
{
    if (!res || (!node && !service))
        return EAI_NONAME;
    *res = nullptr;

    const int flags  = hints ? hints->ai_flags : 0;
    const int family = hints ? hints->ai_family : AF_UNSPEC;
    if (family != AF_UNSPEC && family != AF_INET)
        return EAI_FAMILY;

    uint16_t port = 0;
    if (service)
    {
        char*               end   = nullptr;
        const unsigned long value = std::strtoul(service, &end, 10);
        if (!*service || *end || value > 0xffff)
            return EAI_SERVICE; // no services database
        port = static_cast<uint16_t>(value);
    }

    uint32_t address = 0;
    if (!node)
    {
        address = (flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK;
    }
    else if ((flags & AI_NUMERICHOST) && !parseDotted(node, address))
    {
        return EAI_NONAME;
    }
    else if (const int err = lookupHost(node, address))
    {
        return err;
    }

    struct Block
    {
        addrinfo    info;
        sockaddr_in addr;
    };
    auto* block = static_cast<Block*>(std::calloc(1, sizeof(Block)));
    if (!block)
        return EAI_MEMORY;
    block->addr.sin_family      = AF_INET;
    block->addr.sin_port        = htons(port);
    block->addr.sin_addr.s_addr = htonl(address);
    block->info.ai_family       = AF_INET;
    block->info.ai_socktype     = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    block->info.ai_protocol     = hints ? hints->ai_protocol : 0;
    block->info.ai_addrlen      = sizeof(sockaddr_in);
    block->info.ai_addr         = reinterpret_cast<sockaddr*>(&block->addr);
    *res                        = &block->info;
    return 0;
}

struct hostent* gethostbyname(const char* name)
{
    static uint32_t address; // network byte order
    static char     hostName[256];
    static char*    addrList[2];
    static char*    aliases[1];
    static hostent  host;

    uint32_t found = 0;
    if (!name)
    {
        h_errno = HOST_NOT_FOUND;
        return nullptr;
    }
    switch (lookupHost(name, found))
    {
        case 0:
            break;
        case EAI_AGAIN:
            h_errno = TRY_AGAIN;
            return nullptr;
        case EAI_NONAME:
            h_errno = HOST_NOT_FOUND;
            return nullptr;
        default:
            h_errno = NO_RECOVERY;
            return nullptr;
    }

    address = htonl(found);
    std::strncpy(hostName, name, sizeof(hostName) - 1);
    hostName[sizeof(hostName) - 1] = '\0';
    addrList[0]                    = reinterpret_cast<char*>(&address);
    addrList[1]                    = nullptr;
    aliases[0]                     = nullptr;
    host.h_name                    = hostName;
    host.h_aliases                 = aliases;
    host.h_addrtype                = AF_INET;
    host.h_length                  = sizeof(address);
    host.h_addr_list               = addrList;
    h_errno                        = 0;
    return &host;
}

void freeaddrinfo(struct addrinfo* res)
{
    // One allocation per lookup, starting at the addrinfo.
    std::free(res);
}

const char* gai_strerror(int error)
{
    switch (error)
    {
        case 0:
            return "Success";
        case EAI_AGAIN:
            return "Temporary failure in name resolution";
        case EAI_FAIL:
            return "The MacTCP resolver is not available";
        case EAI_FAMILY:
            return "Address family not supported";
        case EAI_MEMORY:
            return "Out of memory";
        case EAI_NONAME:
            return "Host name not found";
        case EAI_SERVICE:
            return "Service not supported";
        default:
            return "Name resolution failed";
    }
}

} // extern "C"
