#import <Cocoa/Cocoa.h>

@interface SDLMain : NSObject
{
    IBOutlet NSWindow *prefsWindow;
    IBOutlet NSPopUpButton *skipBios;
    IBOutlet NSPopUpButton *pauseWhenInactive;
    IBOutlet NSPopUpButton *realtimeClock;
    IBOutlet NSPopUpButton *interframe;
    IBOutlet NSPopUpButton *soundEcho;
    IBOutlet NSPopUpButton *soundLowPass;
    IBOutlet NSPopUpButton *soundOn;
    IBOutlet NSPopUpButton *soundQuality;
    IBOutlet NSPopUpButton *soundReverseStereo;
    IBOutlet NSPopUpButton *removeIntros;
    IBOutlet NSPopUpButton *flashSize;
    IBOutlet NSPopUpButton *videoBorder;
    IBOutlet NSPopUpButton *videoFilter;
    IBOutlet NSPopUpButton *videoFullscreen;
    IBOutlet NSPopUpButton *videoSize;
    IBOutlet NSPopUpButton *videoWashedColors;
    IBOutlet NSPopUpButton *gbaFrameSkip;
    IBOutlet NSPopUpButton *gbFrameSkip;
    IBOutlet NSPopUpButton *autoFrameSkip;
    IBOutlet NSPopUpButton *showSpeed;
    IBOutlet NSPopUpButton *useBios;
    IBOutlet NSTextField *biosFilePath;
    IBOutlet NSPopUpButton *throttle;
    IBOutlet NSPopUpButton *changeFileType;
}
- (IBAction)closePrefs:(id)sender;
- (IBAction)loadConfig;
- (IBAction)closePrefsNull:(id)sender;
- (IBAction)openPrefs:(id)sender;
- (IBAction)openRomFromMenu:(id)sender;
- (IBAction)quit:(id)sender;
- (IBAction)changeCreator:(char *)filename;
- (IBAction)changeSaveCreator:(char *)filename;
- (IBAction)changeSgmCreator:(char *)statename;
- (IBAction)selectBiosFile:(id)sender;
- (IBAction)addCheatCBA;
- (IBAction)addCheatGSA;
@end

@interface CheatClass : NSObject
{
    IBOutlet NSTextField *cheatField;
    IBOutlet NSTextField *cheatField2;
    IBOutlet NSTextField *cheatField3;
    IBOutlet NSTextField *cheatField4;
    IBOutlet NSTextField *cheatField5;
    IBOutlet NSTextField *cheatField6;
    IBOutlet NSWindow *cheatWindow;
    IBOutlet NSTextField *cheatFieldGSA;
    IBOutlet NSTextField *cheatFieldGSA2;
    IBOutlet NSTextField *cheatFieldGSA3;
    IBOutlet NSTextField *cheatFieldGSA4;
    IBOutlet NSTextField *cheatFieldGSA5;
    IBOutlet NSTextField *cheatFieldGSA6;
    IBOutlet NSWindow *cheatWindowGSA;
    IBOutlet NSMenuItem *cheatMenuCBA;
    IBOutlet NSMenuItem *cheatMenuGSA;
    IBOutlet NSMenu *cheatMenu;
}
- (IBAction)disableCheats;
- (IBAction)enableCheats;
- (IBAction)readCheatCBA:(id)sender;
- (IBAction)readCheatGSA:(id)sender;
- (IBAction)openCheatCBA:(id)sender;
- (IBAction)openCheatGSA:(id)sender;
@end

@interface MenuClass : NSObject
- (IBAction)closeRom:(id)sender;
- (IBAction)resetEmulation:(id)sender;
- (IBAction)pauseEmulation:(id)sender;
- (IBAction)advanceFrame:(id)sender;
- (IBAction)toggleFullscreen:(id)sender;
@end

@interface ConfigClass : NSObject
{
    IBOutlet NSButton *calibrateButton;
    IBOutlet NSButton *defaultButton;
    IBOutlet NSButton *OKButton;
    IBOutlet NSButton *CancelButton;
    IBOutlet NSTextField *aField;
    IBOutlet NSTextField *bField;
    IBOutlet NSTextField *downField;
    IBOutlet NSTextField *leftField;
    IBOutlet NSTextField *lField;
    IBOutlet NSTextField *rField;
    IBOutlet NSTextField *rightField;
    IBOutlet NSTextField *selectField;
    IBOutlet NSTextField *startField;
    IBOutlet NSTextField *upField;
    IBOutlet NSTextField *captureField;
    IBOutlet NSTextField *speedField;
    IBOutlet NSWindow *configWindow;
    IBOutlet NSWindow *noteWindow;
}
- (void)loadKeyValues;
- (IBAction)beginConfig:(id)sender;
- (IBAction)endConfig:(id)sender;
- (IBAction)calibrate:(id)sender;
- (IBAction)pollA:(id)sender;
- (IBAction)pollB:(id)sender;
- (IBAction)pollCapture:(id)sender;
- (IBAction)pollSpeed:(id)sender;
- (IBAction)pollDown:(id)sender;
- (IBAction)pollL:(id)sender;
- (IBAction)pollLeft:(id)sender;
- (IBAction)pollR:(id)sender;
- (IBAction)pollRight:(id)sender;
- (IBAction)pollSelect:(id)sender;
- (IBAction)pollStart:(id)sender;
- (IBAction)pollUp:(id)sender;
@end

extern SDLMain *gSDLMain;
extern bool changeType;
extern int done;
extern int emulating;
void sdlEmuReset( void );
void sdlEmuPause( void );
void sdlAdvanceFrame( void );
void sdlToggleFullscreen( void );
void cheatsAddCBACode(const char *code, const char *desc);
void cheatsAddGSACode(const char *code, const char *desc, bool v3);
void sdlWriteState(int num);
void sdlReadState(int num);