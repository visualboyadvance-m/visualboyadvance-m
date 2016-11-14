# The Visualboyadvance-M todo list for those that care

- [x] remove silly type defs, Come on... It's 2016
- [ ] Fix the updater in the wxwidgets interface, some people have been reporting issues of timing out with not internet.
- [ ] Fix the libretro interface
- [ ] Next will be removing the majority of the interface code and going straight libretro (Will make it easier to maintain one interface that's platform independent.
- [ ] add OnSize handler for GLDrawingPanel in wx back to reset the GL viewport, and set viewport on init as well
- [x] fix wx accels that are a game key with a modifier, e.g. ALT+ENTER when ENTER is a game key 
- [ ] add an option to the video config dialog to choose native or non-native fullscreen for Mac (and check if the OS supports it)
- [ ] fix SFML cmake stuff so that static linking works on Mac
- [ ] update FindSDL2.cmake to use sdl2-config if available and pkg-config is not, get PR merged upstream
- [ ] optimize all options defaults for modern hardware
- [ ] fix filter options in wx to apply to both fullscreen and window mode
- [ ] update homebrew wxmac formula to also make static libs, this will allow static linking of wx, not really as necessary anymore but would still be nice
- [ ] submit Mac homebrew formula, and make sure everything for "make install" works on Mac
- [ ] update Mac .app plist stuffs
- [ ] make full set of quality icons
- [ ] update cmake code to allow building fully-redistributable binaries with msys2 on windows, possibly with an installer
- [ ] write a ./quickbuild script that installs all deps and builds the project, for mac, msys2, and some linux dists
- [ ] use SDL2 input introspection to automatically create a sane set of default control bindings
- [ ] fix the literally hundreds of fucking warnings, and start using -Wall
- [ ] set up automatic builds for all platforms
- [ ] see what code we can steal from other emu folks, e.g. filters etc.
- [ ] update config handling, to switch to XDG on linux etc.
- [ ] add simple 'mute audio' option for wx interface
- [ ] HiDPI support on Windows
- [ ] ./installdeps for more platforms
- [ ] console-mode Wx app on Windows in debug mode
- [ ] fix keyboard game input on msys2 builds
- [ ] fix the -D\*DIR defines to have the correct paths on various platforms

# Coding Guidelines (for those that want to help out and send a pull request.)

Coding guidelines, well I've switched to webkit's style meaning we use spaces and not hard tabs
C has 8 spaces to an indent and c++ 4 spaces for an indent as well. This makes for cleaner code to read and to maintain. 
