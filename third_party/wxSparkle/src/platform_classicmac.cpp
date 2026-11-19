/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// Classic Mac OS (Retro68) specific code.

#include "platform.h"

#if WXSPARKLE_CLASSIC_MAC

#include "error.h"

#include <wx/filename.h>
#include <wx/string.h>
#include <wx/retro68/private.h> // wxMacPathToFSSpec()

#include <Multiverse.h>

#include <cstring>

namespace winsparkle
{

namespace
{

// Finder Event Suite's "open selection" event, understood by all System 7+
// Finders (not defined in Multiverse.h)
const AEEventClass FINDER_EVENT_CLASS  = 'FNDR';
const AEEventID    FINDER_OPEN_SELECTION = 'sope';
const AEKeyword    FINDER_KEY_SELECTION  = 'fsel';
const OSType       FINDER_SIGNATURE      = 'MACS';

// Gestalt selector for Finder attributes and the bit telling if the Finder is
// scriptable (Finder 7.5+, or System 7 with Finder Scripting Extension)
const OSType       GESTALT_FINDER_ATTR   = 'fndr';
const int          GESTALT_OSL_COMPLIANT_FINDER = 0;

bool IsFinderScriptable()
{
    long attr = 0;
    return Gestalt(GESTALT_FINDER_ATTR, &attr) == noErr &&
           (attr & (1L << GESTALT_OSL_COMPLIANT_FINDER)) != 0;
}

struct FileKind
{
    const char *ext;
    OSType      type;
    OSType      creator;
};

// The types and creators of common formats of downloads for classic Mac OS.
const FileKind FILE_KINDS[] =
{
    // StuffIt Expander handles all archive and encoding formats
    { "sit",   'SIT!', 'SITx' },
    { "sitx",  'SITX', 'SITx' },
    { "sea",   'APPL', 'aust' }, // self-extracting archive (if MacBinary-decoded)
    { "hqx",   'TEXT', 'SITx' }, // BinHex
    { "bin",   'BINA', 'SITx' }, // MacBinary
    { "cpt",   'PACT', 'SITx' }, // Compact Pro
    { "zip",   'ZIP ', 'SITx' },
    { "gz",    'Gzip', 'SITx' },
    { "tgz",   'Gzip', 'SITx' },
    { "tar",   'TARF', 'SITx' },
    // Disk Copy handles disk images
    { "img",   'dImg', 'dCpy' },
    { "image", 'dImg', 'dCpy' },
    { "dsk",   'dImg', 'dCpy' },
    { "smi",   'APPL', 'oneb' }, // self-mounting image (if MacBinary-decoded)
};

// Sets the file type and creator from the extension, if it is known and the
// file doesn't have them yet.
void SetFileTypeFromExtension(FSSpec& spec, const wxString& path)
{
    FInfo info;
    if ( FSpGetFInfo(&spec, &info) != noErr )
        return;

    const wxString ext = wxFileName(path).GetExt().Lower();
    for ( const FileKind& kind : FILE_KINDS )
    {
        if ( ext != kind.ext )
            continue;

        if ( info.fdType == kind.type && info.fdCreator == kind.creator )
            return;

        // An application can't be created this way: unless the download was
        // decoded from MacBinary (in which case it already has a type), it is
        // only a data fork with no code.
        if ( kind.type == 'APPL' )
            return;

        info.fdType = kind.type;
        info.fdCreator = kind.creator;
        if ( FSpSetFInfo(&spec, &info) != noErr )
            LogWarning("Failed to set the type and creator of the update file");
        return;
    }
}

// RAII helper for AEDesc
class AEDescHolder
{
public:
    AEDescHolder() { desc.descriptorType = typeNull; desc.dataHandle = NULL; }
    ~AEDescHolder() { AEDisposeDesc(&desc); }

    AEDescHolder(const AEDescHolder&) = delete;
    AEDescHolder& operator=(const AEDescHolder&) = delete;

    AEDesc desc;
};

// Creates an alias descriptor for the given file or folder.
OSErr CreateAliasDesc(FSSpec& spec, AEDesc *desc)
{
    AliasHandle alias = NULL;
    OSErr err = NewAliasMinimal(&spec, &alias);
    if ( err != noErr )
        return err;

    HLock(reinterpret_cast<Handle>(alias));
    err = AECreateDesc(typeAlias, *alias,
                       GetHandleSize(reinterpret_cast<Handle>(alias)), desc);
    DisposeHandle(reinterpret_cast<Handle>(alias));
    return err;
}

// Sends an Apple event to the Finder without waiting for a reply.
OSErr SendToFinder(AppleEvent *event)
{
    AppleEvent reply;
    reply.descriptorType = typeNull;
    reply.dataHandle = NULL;
    const OSErr err = AESend(event, &reply, kAENoReply, kAENormalPriority,
                             kAEDefaultTimeout, NULL, NULL);
    AEDisposeDesc(&reply);
    return err;
}

// Scriptable Finder: "open documents" event sent to the Finder.
OSErr OpenWithFinderOdoc(AEAddressDesc& finder, FSSpec& spec)
{
    AEDescHolder file, list, event;

    OSErr err = CreateAliasDesc(spec, &file.desc);
    if ( err == noErr )
        err = AECreateList(NULL, 0, false, &list.desc);
    if ( err == noErr )
        err = AEPutDesc(&list.desc, 0, &file.desc);
    if ( err == noErr )
        err = AECreateAppleEvent(kCoreEventClass, kAEOpenDocuments, &finder,
                                 kAutoGenerateReturnID, kAnyTransactionID, &event.desc);
    if ( err == noErr )
        err = AEPutParamDesc(&event.desc, keyDirectObject, &list.desc);
    if ( err == noErr )
        err = SendToFinder(&event.desc);
    return err;
}

// Any System 7 Finder: "open selection" event, with the folder as the direct
// object and the file in it as the selection.
OSErr OpenWithFinderSelection(AEAddressDesc& finder, FSSpec& spec)
{
    FSSpec folderSpec;
    OSErr err = FSMakeFSSpec(spec.vRefNum, spec.parID, NULL, &folderSpec);
    if ( err != noErr )
        return err;

    AEDescHolder folder, file, list, event;

    err = CreateAliasDesc(folderSpec, &folder.desc);
    if ( err == noErr )
        err = CreateAliasDesc(spec, &file.desc);
    if ( err == noErr )
        err = AECreateList(NULL, 0, false, &list.desc);
    if ( err == noErr )
        err = AEPutDesc(&list.desc, 0, &file.desc);
    if ( err == noErr )
        err = AECreateAppleEvent(FINDER_EVENT_CLASS, FINDER_OPEN_SELECTION, &finder,
                                 kAutoGenerateReturnID, kAnyTransactionID, &event.desc);
    if ( err == noErr )
        err = AEPutParamDesc(&event.desc, keyDirectObject, &folder.desc);
    if ( err == noErr )
        err = AEPutParamDesc(&event.desc, FINDER_KEY_SELECTION, &list.desc);
    if ( err == noErr )
        err = SendToFinder(&event.desc);
    return err;
}

} // anonymous namespace


bool ClassicMacOpenFile(const wxString& path)
{
    FSSpec spec;
    if ( wxMacPathToFSSpec(path, &spec) != noErr )
    {
        LogError("Update file not found");
        return false;
    }

    SetFileTypeFromExtension(spec, path);

    AEDescHolder finder;
    const OSType finderSig = FINDER_SIGNATURE;
    OSErr err = AECreateDesc(typeApplSignature, &finderSig, sizeof(finderSig), &finder.desc);
    if ( err == noErr )
    {
        // (the events are sent without waiting for a reply, so a failure to
        // handle them can't be detected: choose the right one upfront)
        if ( IsFinderScriptable() )
            err = OpenWithFinderOdoc(finder.desc, spec);
        else
            err = OpenWithFinderSelection(finder.desc, spec);
    }

    if ( err != noErr )
    {
        LogError(("Failed to open the update file in the Finder (error " +
                  std::to_string(err) + ")").c_str());
        return false;
    }

    return true;
}

} // namespace winsparkle

#endif // WXSPARKLE_CLASSIC_MAC
