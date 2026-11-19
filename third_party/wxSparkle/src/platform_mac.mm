/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

// macOS-specific helpers used by the UI.

#include <wx/image.h>
#include <wx/log.h>
#include <wx/mstream.h>

#import <AppKit/AppKit.h>

// Returns the application's icon rendered at the given size, or an invalid
// image if the application bundle doesn't have an icon (in which case macOS
// would only give us the generic executable icon).
wxImage wxsparkle_GetMacApplicationIcon(int sizeInPixels)
{
    @autoreleasepool
    {
        NSBundle *bundle = [NSBundle mainBundle];
        if ( ![bundle objectForInfoDictionaryKey:@"CFBundleIconFile"] &&
             ![bundle objectForInfoDictionaryKey:@"CFBundleIconName"] )
            return wxImage();

        NSImage *icon = NSApp ? [NSApp applicationIconImage] : nil;
        if ( !icon )
            return wxImage();

        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
                                    initWithBitmapDataPlanes:NULL
                                                  pixelsWide:sizeInPixels
                                                  pixelsHigh:sizeInPixels
                                               bitsPerSample:8
                                             samplesPerPixel:4
                                                    hasAlpha:YES
                                                    isPlanar:NO
                                              colorSpaceName:NSCalibratedRGBColorSpace
                                                 bytesPerRow:0
                                                bitsPerPixel:0];
        if ( !rep )
            return wxImage();
        [rep setSize:NSMakeSize(sizeInPixels, sizeInPixels)];

        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:rep]];
        [icon drawInRect:NSMakeRect(0, 0, sizeInPixels, sizeInPixels)
                fromRect:NSZeroRect
               operation:NSCompositingOperationCopy
                fraction:1.0];
        [NSGraphicsContext restoreGraphicsState];

        NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
#if !__has_feature(objc_arc)
        [rep release];
#endif
        if ( !png )
            return wxImage();

        if ( !wxImage::FindHandler(wxBITMAP_TYPE_PNG) )
            wxImage::AddHandler(new wxPNGHandler);

        wxMemoryInputStream stream([png bytes], [png length]);
        wxImage img;
        wxLogNull noLog;
        img.LoadFile(stream, wxBITMAP_TYPE_PNG);
        return img;
    }
}
