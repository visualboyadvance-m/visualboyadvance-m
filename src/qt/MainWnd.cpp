// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


#include "MainWnd.h"

#include "version.h"
#include "glwidget.h"
#include "configdialog.h"
#include "sidewidget_cheats.h"


MainWnd::MainWnd( QTranslator **trans, QSettings *settings, QWidget *parent )
	: QMainWindow( parent ),
	translator( trans ),
	settings( settings ),
	fileMenu( 0 ),
	settingsMenu( 0 ),
	toolsMenu( 0 ),
	helpMenu( 0 ),
	enableTranslationAct( 0 ),
	dockWidget_cheats( 0 ),
	emuManager( 0 )
{
	createDisplay();
	setMinimumSize( 320, 240 );
	setWindowTitle( "VBA-M" );

	createDockWidgets();
	createActions();
	createMenus();

	loadSettings();

	emuManager = new EmuManager();
}


MainWnd::~MainWnd()
{
	if( emuManager != 0 ) {
		delete emuManager;
		emuManager = 0;
	}
}


void MainWnd::loadSettings()
{
	QVariant v;

	v = settings->value( "MainWnd/state" );
	if( v.isValid() ) {
		restoreState( v.toByteArray() );
	}
	
	v = settings->value( "App/language_file" );
	if( v.isValid() ) {
		languageFile = v.toString();
		if( loadTranslation() ) {
			// only enable language if it loaded correctly
			v = settings->value( "App/language_enable" );
			enableTranslation( v.toBool() );
		}
	}
}


void MainWnd::saveSettings()
{
	QVariant v;
	
	// state of toolbars and dock widgets
	// all memorizable widgets need an objectName!
	v = saveState();
	settings->setValue( "MainWnd/state", v );

	v = enableTranslationAct->isChecked();
	settings->setValue( "App/language_enable", v );
}


void MainWnd::createActions()
{
	bool enabled, checked;


	if( enableTranslationAct != 0 ) {
		enabled = enableTranslationAct->isEnabled(); // memorize state
		checked = enableTranslationAct->isChecked();
		delete enableTranslationAct;
		enableTranslationAct = 0;
	} else {
		enabled = false;
		checked = false;
	}
	enableTranslationAct = new QAction( tr( "Enable translation" ), this );
	enableTranslationAct->setEnabled( enabled );
	enableTranslationAct->setCheckable( true );
	enableTranslationAct->setChecked( checked );
	connect( enableTranslationAct, SIGNAL( toggled( bool ) ), this, SLOT( enableTranslation( bool ) ) );
}


void MainWnd::createMenus()
{
	if( fileMenu ) {
		delete fileMenu;
		fileMenu = 0;
	}

	if( settingsMenu ) {
		delete settingsMenu;
		settingsMenu = 0;
	}

	if( toolsMenu ) {
		delete toolsMenu;
		toolsMenu = 0;
	}

	if( helpMenu ) {
		delete helpMenu;
		helpMenu = 0;
	}


	// File menu
	fileMenu = menuBar()->addMenu( tr( "File" ) );
	fileMenu->addAction( QIcon( ":/resources/open.png" ), tr( "Open ROM" ), this, SLOT( showOpenROM() ) );
	fileMenu->addAction( QIcon( ":/resources/exit.png" ), tr( "Exit" ), this, SLOT( close() ) );


	// Settings menu
	settingsMenu = menuBar()->addMenu( tr( "Settings" ) );
	settingsMenu->addAction( QIcon( ":/resources/settings.png" ), tr( "Main options..." ), this, SLOT( showMainOptions() ) );
	settingsMenu->addAction( QIcon( ":/resources/locale.png" ), tr( "Select language..." ), this, SLOT( selectLanguage() ) );
	settingsMenu->addAction( enableTranslationAct );


	// Tools menu
	toolsMenu = menuBar()->addMenu( tr( "Tools" ) );
	QAction *toggleCheats = dockWidget_cheats->toggleViewAction();
	toggleCheats->setText( tr( "Show cheats sidebar" ) );
	toolsMenu->addAction( toggleCheats ) ;


	// Help menu
	helpMenu = menuBar()->addMenu( tr( "Help" ) );

	helpMenu->addAction( QIcon( ":/resources/vba-m.png" ), tr( "About VBA-M..." ), this, SLOT( showAbout() ) );
	helpMenu->addAction( QIcon( ":/resources/gl.png" ), tr( "About OpenGL..." ), this, SLOT( showAboutOpenGL() ) );
	helpMenu->addAction( QIcon( ":/resources/qt_logo.png" ), tr( "About Qt..." ), qApp, SLOT( aboutQt() ) );
}


void MainWnd::createDockWidgets()
{
	if( dockWidget_cheats != 0 ) {
		delete dockWidget_cheats;
		dockWidget_cheats = 0;
	}

	// Cheat Widget
	dockWidget_cheats = new QDockWidget( tr( "Cheats" ), this );
	dockWidget_cheats->setObjectName( "dockWidget_cheats" ); // necessary for MainWnd::saveState
	SideWidget_Cheats *sw_cheats = new SideWidget_Cheats( dockWidget_cheats );
	dockWidget_cheats->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
	dockWidget_cheats->setWidget( sw_cheats );
	addDockWidget( Qt::LeftDockWidgetArea, dockWidget_cheats );
	dockWidget_cheats->hide();
}


bool MainWnd::createDisplay()
{
	if( !QGLFormat::hasOpenGL() ) return false;

	GLWidget *ogl = new GLWidget( this );
	if( ogl->isValid() ) {
		setCentralWidget( ogl );
		return true;
	}

	return false;
}


void MainWnd::closeEvent( QCloseEvent * )
{
	saveSettings();
}


bool MainWnd::selectLanguage()
{
	QString file = QFileDialog::getOpenFileName(
		this,
		tr( "Select language" ),
		"lang",
		tr( "Language files (*.qm)" ) );

	if( file.isNull() ) return false;

	languageFile = file;

	bool ret = loadTranslation();
	ret &= enableTranslation( true );

	if( ret == false ) {
		QMessageBox::critical( this, tr( "Error!" ), tr( "Language file can not be loaded!" ) );
	}
	return ret;
}


bool MainWnd::loadTranslation()
{
	settings->setValue( "App/language_file", languageFile );
	if( !languageFile.endsWith( tr( ".qm" ), Qt::CaseInsensitive ) ) return false;
	QString file = languageFile;

	// remove current translation
	enableTranslation( false );
	if( *translator != 0 ) {
		delete *translator;
		*translator = 0;
	}

	file.chop( 3 ); // remove file extension ".qm"

	// load new translation
	*translator = new QTranslator();
	bool ret = (*translator)->load( file );
	enableTranslationAct->setEnabled( ret );
	return ret;
}


bool MainWnd::enableTranslation( bool enable )
{
	if( enable ) {
		if( *translator != 0 ) {
			qApp->installTranslator( *translator );
			enableTranslationAct->setChecked( true );
		} else {
			return false;
		}
	} else {
		if( *translator != 0 ) {
			qApp->removeTranslator( *translator );
		} else {
			return false;
		}
	}

	// apply translation
	// the user might have to restart the application to apply changes completely
	QByteArray windowState = saveState();
	createDockWidgets();
	createActions();
	createMenus();
	restoreState( windowState );
	return true;
}


void MainWnd::showAbout()
{
	QString info( "VisualBoyAdvance-M\n" );
	info += tr( "Version" ) + " " + VERSION_STR + "\n";

	info += tr( "Compile date:" ) + " " + __DATE__ + "\n";

	// translators may use this string to give informations about the language file
	info += "\n" + tr ( "No language file loaded." ) + "\n";

	info += "\n" + tr ( "This program is licensed under terms of the GNU General Public License." );

	QMessageBox::about( this, tr( "About VBA-M" ), info );
}


void MainWnd::showOpenROM()
{
	QString file = QFileDialog::getOpenFileName(
		this,
		tr( "Select ROM" ),
		"",
		tr( "Game Boy Advance ROMs (*.gba);;All Files (*.*)" ) );

	if( file.isNull() ) return;

	emuManager->setROM( file );
	if( !emuManager->loadROM() ) {
		QMessageBox::critical( this, tr( "Error!" ), tr( "Can not load ROM!" ) );
		return;
	}
	
	// TODO: start emulation
}


void MainWnd::showMainOptions()
{
	ConfigDialog dialog;
	dialog.exec();
}


void MainWnd::showAboutOpenGL()
{
	QString info;
	if( QGLFormat::hasOpenGL() ) {
		QGLFormat::OpenGLVersionFlags flags = QGLFormat::openGLVersionFlags();
		if( flags & QGLFormat::OpenGL_Version_2_1 ) {
			info = tr( "OpenGL version 2.1 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_2_0 ) {
			info = tr( "OpenGL version 2.0 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_1_5 ) {
			info = tr( "OpenGL version 1.5 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_1_4 ) {
			info = tr( "OpenGL version 1.4 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_1_3 ) {
			info = tr( "OpenGL version 1.3 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_1_2 ) {
			info = tr( "OpenGL version 1.2 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_1_1 ) {
			info = tr( "OpenGL version 1.1 is present." );
		} else 
		if( flags & QGLFormat::OpenGL_Version_None ) {
			info = tr( "OpenGL is NOT available!" );
		} 
	} else {
		info = tr( "OpenGL is NOT available!" );
	}

	QMessageBox *test = new QMessageBox( QMessageBox::NoIcon, tr( "About OpenGL" ), info, QMessageBox::NoButton, this );
	test->setWindowIcon( QIcon( ":/resources/gl.png" ) );
	test->show();
}
