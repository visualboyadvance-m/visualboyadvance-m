//
//  MyCustomView.m
//  SDL Custom View App
//
//  Created by Darrell Walisser on Fri Jul 18 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "MyCustomView.h"


@implementation MyCustomView

- (id)initWithFrame:(NSRect)frame
{
    self = [ super initWithFrame:frame ];
    if (self) {
        // Initialization code here.
    }
    return self;
}

- (void)drawRect:(NSRect)rect
{
    // Drawing code here.
}

- (BOOL)mouseDownCanMoveWindow
{
    // If you use textured windows, this disables
    // moving the window by dragging in the view
    return NO;
}
@end
