//
//  MyCustomWindow.h
//  SDL Custom View App
//
//  Created by Darrell Walisser on Fri Jul 18 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>

// Be a subclass of SDL_QuartzWindow so SDL will
// handle the redraw problems when minimizing the window
// This class is defined in SDL.framework
@interface SDL_QuartzWindow : NSWindow
@end

// Also assign SDL_QuartzWindowDelegate to the window
// to perform other tasks. You can subclass this delegate
// if you want to add your own delegation methods
// This class is defined in SDL.framework
@interface SDL_QuartzWindowDelegate : NSObject
@end

// Declare our custom class
@interface MyCustomWindow : SDL_QuartzWindow
@end

