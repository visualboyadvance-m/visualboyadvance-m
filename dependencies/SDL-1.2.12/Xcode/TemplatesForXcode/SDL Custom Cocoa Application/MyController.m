//
//  MyController.m
//  SDL Custom Cocoa App
//
//  Created by Darrell Walisser on Fri Jul 18 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "MyController.h"
#import "SDL.h"

id gController;

@implementation MyController

- (id)init
{
    self = [ super init ];
    if (self) {
    
        _nSprites = 10;
        _max_speed = 4;
        _doFlip = 0;
        _mem = NULL;
        _screen = NULL;
        _sprite_rects = NULL;
        _positions = NULL;
        _velocities = NULL;
        _sprites_visible = 1;
        _sprite_w = 0;
        _sprite_h = 0;
        _mouse_x = 0;
        _mouse_y = 0;
    }
    
    return self;
}

- (void)setupCocoaWindow
{
    // If you want the close button to send SDL_Quit,
    // you can attach the SDL_QuartzWindowDelegate to the window
    [ _window setDelegate:
        [ [ [ SDL_QuartzWindowDelegate alloc ] init ] autorelease ] ];
        
    // Make the window visible and the frontmost window
    [ _window makeKeyAndOrderFront:nil ];
    
    // Make the window accept passive motion events (when the button is released)
    [ _window setAcceptsMouseMovedEvents:YES ];
}

- (void)setCocoaEnvironmentVariables
{
    // Export cocoa objects to environment
    // SDL will use these when you call SDL_SetVideoMode
    
    // 	The window must be visible when you call SDL_SetVideoMode,
    // 	and the view must lie completely within the window.
    //
    //  The width and height passed to SDL_SetVideoMode should match
    //  the width/height of the view (the window can be any size)
    //
    // 	For resizing to work, you must set the appropriate
    // 	attributes on the window and view. Then the SDL_RESIZABLE
    //  flag will be set automatically
    //
    //  SDL will retain a reference to the window, and
    //  will release it when unsetting the video mode
    //
    //  The view is not retained (the window object manages this).
    //
    char buffer[256];
    printf ("NSWindow=%p\n", _window);
    sprintf (buffer, "%d", (int)_window);
    setenv ("SDL_NSWindowPointer", buffer, 1);
    
    printf ("NSQuickDrawView=%p\n", _view);
    sprintf (buffer, "%d", (int)_view);
    setenv ("SDL_NSQuickDrawViewPointer", buffer, 1);
    
    // Also tell SDL to pass keyboard events to Cocoa
    // In this case, Cocoa will get the event before SDL_PollEvent returns it
    // Note that mouse events (button, motion) will always be passed, regardless of this setting
    setenv ("SDL_ENABLEAPPEVENTS", "1", 1);
}

- (void)printFlags:(Uint32)flags withName:(const char*)name
{
    printf ("%s", name);
    
    if (flags & SDL_SWSURFACE)
        printf (" - SDL_SWSURFACE");
    if (flags & SDL_HWSURFACE)
        printf (" - SDL_HWSURFACE");
    if (flags & SDL_ASYNCBLIT)
        printf (" - SDL_ASYNCBLIT");
    if (flags & SDL_ANYFORMAT)
        printf (" - SDL_ANYFORMAT");
    if (flags & SDL_HWPALETTE)
        printf (" - SDL_HWPALETTE");
    if (flags & SDL_DOUBLEBUF)
        printf (" - SDL_DOUBLEBUF");
    if (flags & SDL_FULLSCREEN)
        printf (" - SDL_FULLSCREEN");
    if (flags & SDL_OPENGL)
        printf (" - SDL_OPENGL");
    if (flags & SDL_OPENGLBLIT)
        printf (" - SDL_OPENGLBLIT");
    if (flags & SDL_RESIZABLE)
        printf (" - SDL_RESIZABLE");
    if (flags & SDL_NOFRAME)
        printf (" - SDL_NOFRAME");
    
    printf ("\n");
}
// This is a way of telling whether or not to use hardware surfaces
// Note: this does nothing on Mac OS X at the moment - there is no
// hardware-accelerated blitting support.
- (Uint32)fastestFlags:(Uint32)flags :(int)width :(int)height :(int)bpp
{
	const SDL_VideoInfo *info;

	// Hardware acceleration is only used in fullscreen mode
	flags |= SDL_FULLSCREEN;

	// Check for various video capabilities
	info = SDL_GetVideoInfo();
	if ( info->blit_hw_CC && info->blit_fill ) {
		// We use accelerated colorkeying and color filling
		flags |= SDL_HWSURFACE;
	}
	// If we have enough video memory, and will use accelerated
	//   blits directly to it, then use page flipping.
	if ( (flags & SDL_HWSURFACE) == SDL_HWSURFACE ) {
		
        // Direct hardware blitting without double-buffering
		// causes really bad flickering.
		if ( info->video_mem*1024 > (height*width*bpp/8) ) {
			flags |= SDL_DOUBLEBUF;
		} else {
			flags &= ~SDL_HWSURFACE;
		}
	}

	// Return the flags
	return flags;
}

- (int)loadSpriteFromFile:(const char*)file
{
	SDL_Surface *temp;

	// Load the sprite image
	_sprite = SDL_LoadBMP (file);
	if ( _sprite == NULL ) {
		fprintf (stderr, "Couldn't load %s: %s", file, SDL_GetError ());
		return (-1);
	}

	// Set transparent pixel as the pixel at (0,0)
	if ( _sprite->format->palette ) {
		SDL_SetColorKey (_sprite, (SDL_SRCCOLORKEY|SDL_RLEACCEL),
						 *(Uint8 *)_sprite->pixels);
	}

	/* Convert sprite to video format */
	temp = SDL_DisplayFormat (_sprite);
	SDL_FreeSurface (_sprite);
	if ( temp == NULL ) {
		fprintf(stderr, "Couldn't convert background: %s\n",
                SDL_GetError ());
		return (-1);
	}
	_sprite = temp;

	// We're ready to roll. :)
	return 0;
}

- (void)allocSprites
{
    /* Allocate memory for the sprite info */
	_mem = (Uint8 *)malloc (4*sizeof(SDL_Rect)*_nSprites);
	if ( _mem == NULL ) {
		fprintf (stderr, "Out of memory!\n");
		exit (2);
	}
    
	_sprite_rects = (SDL_Rect *)_mem;
	_positions = _sprite_rects;
	_sprite_rects += _nSprites;
	_velocities = _sprite_rects;
	_sprite_rects += _nSprites;
	_sprite_w = _sprite->w;
	_sprite_h = _sprite->h;
}

- (void)freeSprites
{
    free (_mem);
    _mem = NULL;
}

- (void)initSprites
{
    // Give each sprite a random starting position and velocity
    int i;
    
	srand(time(NULL));
	for ( i=0; i < _nSprites; ++i ) {
		_positions[i].x = rand()%(_screen->w - _sprite->w);
		_positions[i].y = rand()%(_screen->h - _sprite->h);
		_positions[i].w = _sprite->w;
		_positions[i].h = _sprite->h;
		_velocities[i].x = 0;
		_velocities[i].y = 0;
		while ( ! _velocities[i].x && ! _velocities[i].y ) {
			_velocities[i].x = (rand()%(_max_speed*2+1)) - _max_speed;
			_velocities[i].y = (rand()%(_max_speed*2+1)) - _max_speed;
		}
	}
}

- (void)moveSprites:(int)backgroundColor;
{
	int i, nupdates;
	SDL_Rect area, *position, *velocity;

	nupdates = 0;
	// Erase all the sprites if necessary
	if ( _sprites_visible ) {
		SDL_FillRect (_screen, NULL, backgroundColor);
	}

	// Move the sprite, bounce at the wall, and draw
	for ( i=0; i < _nSprites; ++i ) {
		position = &_positions[i];
		velocity = &_velocities[i];
		position->x += velocity->x;
		if ( (position->x < 0) || (position->x >= (_screen->w - _sprite_w)) ) {
			velocity->x = -velocity->x;
			position->x += velocity->x;
		}
		position->y += velocity->y;
		if ( (position->y < 0) || (position->y >= (_screen->h - _sprite_w)) ) {
			velocity->y = -velocity->y;
			position->y += velocity->y;
		}

		// Blit the sprite onto the screen
		area = *position;
		SDL_BlitSurface (_sprite, NULL, _screen, &area);
		_sprite_rects[nupdates++] = area;
	}
    
	// Update the screen!
	if ( _doFlip ) {
		SDL_Flip (_screen);
	} else {
		SDL_UpdateRects (_screen, nupdates, _sprite_rects);
	}
    
	_sprites_visible = 1;
}

- (int)run:(int)argc argv:(char*[])argv
{
	int width, height;
	Uint8  video_bpp;
	Uint32 videoflags;
	Uint32 background;
	int    done;
	SDL_Event event;
    Uint32 then, now, frames;

	// Initialize SDL
	if ( SDL_Init (SDL_INIT_VIDEO) < 0 ) {
		fprintf (stderr, "Couldn't initialize SDL: %s\n", SDL_GetError ());
		exit (1);
	}
	atexit(SDL_Quit);

	videoflags = SDL_SWSURFACE|SDL_ANYFORMAT;
	width = 640;
	height = 480;
	video_bpp = 8;
	while ( argc > 1 ) {
		--argc;
		if ( strcmp(argv[argc-1], "-width") == 0 ) {
			width = atoi (argv[argc]);
			--argc;
		} else
		if ( strcmp(argv[argc-1], "-height") == 0 ) {
			height = atoi (argv[argc]);
			--argc;
		} else
		if ( strcmp(argv[argc-1], "-bpp") == 0 ) {
			video_bpp = atoi (argv[argc]);
			videoflags &= ~SDL_ANYFORMAT;
			--argc;
		} else
		if ( strcmp (argv[argc], "-fast") == 0 ) {
			videoflags = [ self fastestFlags:videoflags :width :height :video_bpp ];
		} else
		if ( strcmp (argv[argc], "-hw") == 0 ) {
			videoflags ^= SDL_HWSURFACE;
		} else
		if ( strcmp (argv[argc], "-flip") == 0 ) {
			videoflags ^= SDL_DOUBLEBUF;
		} else
		if ( strcmp (argv[argc], "-fullscreen") == 0 ) {
			videoflags ^= SDL_FULLSCREEN;
		} else
		if ( isdigit(argv[argc][0]) ) {
			_nSprites = atoi (argv[argc]);
		} else {
			fprintf (stderr, 
	"Usage: %s [-bpp N] [-hw] [-flip] [-fast] [-fullscreen] [numSprites]\n",
								argv[0]);
			exit (1);
		}
	}
    
    // The width/height passed to SDL_SetVideoMode should match the size of the NSView
    width = [ _view frame ].size.width;
    height = [ _view frame ].size.height;
    videoflags &= ~SDL_FULLSCREEN; // this only for windowed mode
    if ( [ _window styleMask ] & NSResizableWindowMask ) // enable resizable window
        videoflags |= SDL_RESIZABLE;
    [ self setupCocoaWindow ];
    [ self setCocoaEnvironmentVariables ];
    _mouse_x = width/2;
    _mouse_y = width/2;
    
	// Set video mode
	_screen = SDL_SetVideoMode (width, height, video_bpp, videoflags);
	if ( ! _screen ) {
		fprintf (stderr, "Couldn't set %dx%d video mode: %s\n",
                    width, height, SDL_GetError());
		exit (2);
	}

	// Print out surface info
	[ self printFlags:videoflags     withName:"Requested flags: " ];
	[ self printFlags:_screen->flags withName:"Got flags:       " ];

	// Load the sprite
	// The sprite is located in the Resources section of the .app bundle. 
	// SDL does not seem aware of bundle paths, so we must construct the path manually.
	NSString* resource_path = [[NSBundle mainBundle] resourcePath];
	NSString* icon_path = [resource_path stringByAppendingString:@"/icon.bmp"];
	if ( [ self loadSpriteFromFile:[icon_path UTF8String] ] < 0 ) {
		exit (1);
	}

    [ self allocSprites ];
    [ self initSprites ];
    
	background = SDL_MapRGB (_screen->format, 0x00, 0x00, 0x00);

	// Print out information about our surfaces
	printf("Screen is at %d bits per pixel\n", _screen->format->BitsPerPixel);
	if ( (_screen->flags & SDL_HWSURFACE) == SDL_HWSURFACE ) {
		printf ("Screen is in video memory\n");
	} else {
		printf ("Screen is in system memory\n");
	}
	if ( (_screen->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF ) {
		printf ("Screen has double-buffering enabled\n");
	}
	if ( (_sprite->flags & SDL_HWSURFACE) == SDL_HWSURFACE ) {
		printf ("Sprite is in video memory\n");
	} else {
		printf ("Sprite is in system memory\n");
	}
    
	// Run a sample blit to trigger blit acceleration
	{ 
        SDL_Rect dst;
		dst.x = 0;
		dst.y = 0;
		dst.w = _sprite->w;
		dst.h = _sprite->h;
		SDL_BlitSurface (_sprite, NULL, _screen, &dst);
		SDL_FillRect (_screen, &dst, background);
	}
	if ( (_sprite->flags & SDL_HWACCEL) == SDL_HWACCEL ) {
		printf("Sprite blit uses hardware acceleration\n");
	}
	if ( (_sprite->flags & SDL_RLEACCEL) == SDL_RLEACCEL ) {
		printf("Sprite blit uses RLE acceleration\n");
	}

	// Loop, blitting sprites and waiting for a keystroke
	frames = 0;
	then = SDL_GetTicks();
	done = 0;
	_sprites_visible = 0;
	while ( !done ) {
		// Check for events
		++frames;
		while ( SDL_PollEvent(&event) ) {
			switch (event.type) {
				
                case SDL_VIDEORESIZE:
                    
                    // Recreate the video mode. Use the same bpp and flags
                    // that the original window was created with
                    _screen = SDL_SetVideoMode (event.resize.w, event.resize.h,
                                                video_bpp, videoflags);
                    
                    // Clear screen and reinit sprite positions
                    SDL_FillRect (_screen, NULL, 0);
                    [ self initSprites ];
                    break;
               
                case SDL_MOUSEMOTION:
                    
                    _velocities[_nSprites-1].x = 0;
                    _velocities[_nSprites-1].y = 0;
                    _positions[_nSprites-1].x = event.motion.x - _sprite->w/2;
                    _positions[_nSprites-1].y = event.motion.y - _sprite->h/2;
                    //printf ("motion: %d %d\n", event.motion.x, event.motion.y);
                    break;
                
                case SDL_MOUSEBUTTONDOWN:
                    
                    _velocities[_nSprites-1].x = 0;
                    _velocities[_nSprites-1].y = 0;
                    _positions[_nSprites-1].x = event.button.x;
                    _positions[_nSprites-1].y = event.button.y;
                    //printf ("button: %d %d\n", event.button.x, event.button.y);
					break;
				
                case SDL_KEYDOWN:
                
					// Escape also quits the app
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        done = 1;
                    
                    // Control-W warps the cursor
                    if (event.key.keysym.sym == SDLK_w &&
                        ( SDL_GetModState () & KMOD_LCTRL ) )
                        SDL_WarpMouse (_screen->w/2, _screen->h/2);
                    
                    // Control-G toggles input grabbing
                    if (event.key.keysym.sym == SDLK_g &&
                        ( SDL_GetModState () & KMOD_LCTRL ) )
                        if ( SDL_WM_GrabInput (SDL_GRAB_QUERY) == SDL_GRAB_OFF )
                            SDL_WM_GrabInput (SDL_GRAB_ON);
                        else
                            SDL_WM_GrabInput (SDL_GRAB_OFF);
                    break;
                    
				case SDL_QUIT:
					done = 1;
					break;
				default:
					break;
			}
		}
        
        // Draw sprites
        [ self moveSprites:background ];
        
        // Update fps counter every 100 frames
        if ((frames % 100) == 0) {
            now = SDL_GetTicks ();
            [ _framesPerSecond setFloatValue:((float)frames*1000)/(now-then) ];
            frames = 0;
            then = now;
        }
	}
	[ self freeSprites ];
    SDL_FreeSurface (_sprite);

	SDL_Quit ();
	return 0;
}

- (IBAction)changeNumberOfSprites:(id)sender
{
    [ _numSprites setIntValue:[ sender intValue ] ];
    [ self freeSprites ];
    _nSprites = [ sender intValue ];
    [ self allocSprites ];
    [ self initSprites ];
    SDL_FillRect (_screen, NULL, 0);
    SDL_Flip (_screen);
}

- (IBAction)selectUpdateMode:(id)sender
{
    _doFlip = ![ sender selectedRow ]; // row is 0 or 1
}

@end

int main(int argc, char *argv[])
{
    // Hand off to instance of MyController
    // This instance is created when SDLMain.nib is loaded,
    // and the global variable is set in [SDLMain applicationDidFinishLaunching:]
    return [ gController run:argc argv:argv ];
}