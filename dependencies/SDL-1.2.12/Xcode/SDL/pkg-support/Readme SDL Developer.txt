SDL Mac OS X Developer Notes:
	This is an optional developer package to provide extras that an 
	SDL developer might benefit from.
	
	Make sure you have already installed the SDL.framework 
	from the SDL.dmg. 
	
	For more complete documentation, please see READMEs included 
	with the  SDL source code. Also, don't forget about the API 
	documentation (also included with this package).


This package contains:
- SDL API Documentation
- A variety of SDLMain and .Nib files to choose from
- Xcode/Project Builder project templates


SDL API Documentation:
	We include both the HTML documentation and the man files. For 
	the HTML documentation, we have previously been installing to 
	/Develoepr/Documentation, but Apple explicitly says this is not 
	intended for 3rd party documentation. Xcode installs/updates 
	reserve the right and have been known to completely purge the 
	/Developer directory before installation, so if you place stuff 
	there, be aware that it could be deleted.


SDLMain:
	We include several different variations of SDLMain and the 
	.Nibs. (Each of these are demonstrated by the different PB/Xcode 
	project templates.) You get to pick which one you want to use, 
	or you can write your own to meet your own specific needs. We do 
	not currently provide a libSDLMain.a. You can build it yourself
	once you decide which one you want to use though it is easier and 
	recommended in the SDL FAQ that you just copy the SDLMain.h and 
	SDLMain.m directly into your project. If you are puzzled by this, 
	we strongly recommend you look at the different PB/Xcode project 
	templates to understand their uses and differences. (See Project 
	Template info below.) Note that the "Nibless" version is the same 
	version of SDLMain we include the the devel-lite section of the 
	main SDL.dmg.
	
	
Project Builder/Xocde Project Templates:
	For convenience, we provide Project Templates for Xcode and 
	Project Builder. Using Xcode is *not* a requirement for using 
	the SDL.framework. However, for newbies, we do recommend trying 
	out the Xcode templates first (and work your way back to raw gcc 
	if you desire), as the Xcode templates try to setup everything
	for you in a working state. This avoids the need to ask those 
	many reoccuring questions that appear on the mailing list 
	or the SDL FAQ.


	Xcode 2.1+ users may use the templates in TemplatesForXcode folder.
	Project Builder and Xcode <= 2.0 users will need to use the 
	templates in TemplatesForProjectBuilder folder.

	We have provided 4 different kinds of SDL templates for Xcode. 
	We recommend you copy each of the template folders to:
	/Library/Application Support/Apple/Developer Tools/Project Templates/Appllcation (for system-wide)
	or
	~/Library/Application Support/Apple/Developer Tools/Project Templates/Appllcation (for per-user)

	(Project Builder users will need to copy to /Developer/ProjectBuilder Extras).


	After doing this, when doing a File->New Project, you will see the 
	projects under the Application category.


	How to create a new SDL project:

	1. Open Xcode (or Project Builder)
	2. Select File->New Project
	3. Select SDL Application
	4. Name, Save, and Finish
	5. Add your sources.
	*6. That's it!

	* If you installed the SDL.framework to $(HOME)/Library/Frameworks 
	instead of /Library/Frameworks, you will need to update the 
	location of the SDL.framework in the "Groups & Files" browser.
	
	** For Xcode users using Project Builder templates, you may 
	want to convert the project to native Xcode targets since 
	our current templates are Project Builder based. 
	Go to the Menu->Project->Upgrade All Targets to Native



	The project templates we provide are:
	- SDL Application
		This is the barebones, most basic version. There is no 
		customized .Nib file. While still utilizing Cocoa under 
		the hood, this version may be best suited for fullscreen 
		applications.

	- SDL Cocoa Application
		This demonstrates the integration of using native 
		Cocoa Menus with an SDL Application. For applications
		designed to run in Windowed mode, Mac users may appreciate 
		having access to standard menus for things
		like Preferences and Quiting (among other things).
	
	- SDL Custom Cocoa Application
		This application demonstrates the integration of SDL with 
		native Cocoa widgets. This shows how
		you can get native Cocoa widgets like buttons, sliders, 
		etc. to interact/integrate with your SDL application.
		
	- SDL OpenGL Application
		This reuses the same SDLMain from the "SDL Application" 
		temmplate, but also demonstrates how to 
		bring OpenGL into the mix.




Project Builder/Xcode Tips and Tricks:

- Building from command line
	Use pbxbuild in the same directory as your .pbproj file
	(Xcode has xcodebuild)
		 
- Running your app
    You can send command line args to your app by either 
	invoking it from the command line (in *.app/Contents/MacOS) 
	or by entering them in the "Executables" panel of the target 
	settings, or the "Executables" tab in the main window 
	in Project Builder 2.0.
    
- Working directory
    As defined in the SDLMain.m file, the working directory of 
    your SDL app is by default set to its parent. You may wish to 
    change this to better suit your needs.





Partial History:
2006-03-17 - Changed the package format from a .pkg based 
	installer to a .dmg to avoid requiring administrator/root 
	to access contents, for better transparency, and to allow 
	users to more easily control which components 
	they actually want to install. 
	Introduced and updated documentation.
	Created brand new Xcode project templates for Xcode 2.1 
	based on the old Project Builder templates as they 
	required Xcode users to "Upgrade to Native Target". The new 
	templates try to leveage more default options and leverage 
	more Xcode conventions. The major change that may introduce 
	some breakage is that I now link to the SDL framework
	via the "Group & Files" browser instead of using build 
	options. The downside to this is that if the user 
	installs the SDL.framework to a place other than 
	/Library/Frameworks (e.g. $(HOME)/Library/Frameworks),
	the framework will not be found to link to and the user 
	has to manually fix this. But the upshot is (in addition to 
	being visually displayed in the forefront) is that it is 
	really easy to copy (embed) the framework automatically 
	into the .app bundle on build. So I have added this 
	feature, which makes the application potentially 
	drag-and-droppable ready. The Project Builder templates 
	are mostly unchanged due to the fact that I don't have 
	Project Builder. I did rename a file extension to .pbxproj 
	for the SDL Custom Cocoa Application template because 
	the .pbx extension would not load in my version of Xcode.
	For both Project Builder and Xcode templates, I resync'd
	the SDLMain.* files for the SDL App and OpenGL App 
	templates. I think people forget that we have 2 other 
	SDLMain's (and .Nib's) and somebody needs to go 
	through them and merge the new changes into those.
	I also wrote a fix for the SDL Custom Cocoa App 
	template in MyController.m. The sprite loading code 
	needed to be able to find the icon.bmp in the .app
	bundle's Resources folder. This change was needed to get 
	the app to run out of the box. This might change is untested 
	with Project Builder though and might break it.
	There also seemed to be some corruption in the .nib itself.
	Merely opening it and saving (allowing IB to correct the
	.nib) seemed to correct things.
	(Eric Wing)




