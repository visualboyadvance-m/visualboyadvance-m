//
//  MyController.h
//  SDL Custom Cocoa App
//
//  Created by Darrell Walisser on Fri Jul 18 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "SDL.h"

extern id gController; // instance of this class from nib

// Declare SDL_QuartzWindowDelegate (defined in SDL.framework)
@interface SDL_QuartzWindowDelegate : NSObject
@end

@interface MyController : NSObject
{
    // Interface Builder Outlets
    IBOutlet id 	_framesPerSecond;
    IBOutlet id 	_numSprites;
    IBOutlet id 	_window;
    IBOutlet id 	_view;
    
    // Private instance variables
    int          _nSprites;
    int          _max_speed;
    int          _doFlip;
    Uint8*       _mem;
    
    SDL_Surface* _screen;
    SDL_Surface* _sprite;
    SDL_Rect*    _sprite_rects;
    SDL_Rect*    _positions;
    SDL_Rect*    _velocities;
    int          _sprites_visible;
    Uint16       _sprite_w, _sprite_h;
    
    int 		 _mouse_x, _mouse_y;
}
// Interface Builder Actions
- (IBAction)changeNumberOfSprites:(id)sender;
- (IBAction)selectUpdateMode:(id)sender;
@end

