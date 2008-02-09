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

#include "glwidget.h"

MainWnd::MainWnd( QWidget *parent )
	: QMainWindow( parent )
{
	createDisplay();

	setMinimumSize( 320, 240 );
	setWindowTitle( tr( "VBA-M" ) );

	createMenus();
}

MainWnd::~MainWnd()
{
}

void MainWnd::createMenus()
{
	QMenu *fileMenu = menuBar()->addMenu( tr( "&File" ) );
	QMenu *settingsMenu = menuBar()->addMenu( tr( "&Settings" ) );
	QMenu *toolsMenu = menuBar()->addMenu( tr( "&Tools" ) );
	QMenu *helpMenu = menuBar()->addMenu( tr( "&Help" ) );

	QAction *showAboutOpenGLAct = new QAction( tr( "About &OpenGL..." ), this );
	connect( showAboutOpenGLAct, SIGNAL( triggered() ), this, SLOT( showAboutOpenGL() ) );
	helpMenu->addAction( showAboutOpenGLAct );

	QAction *showAboutAct = new QAction( tr( "About &VBA-M..." ), this );
	connect( showAboutAct, SIGNAL( triggered() ), this, SLOT( showAbout() ) );
	helpMenu->addAction( showAboutAct );

	QAction *showAboutQtAct = new QAction( tr( "About &Qt..." ), this );
	connect( showAboutQtAct, SIGNAL( triggered() ), this, SLOT( showAboutQt() ) );
	helpMenu->addAction( showAboutQtAct );
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

void MainWnd::showAbout()
{
	QMessageBox::about( this, tr( "About VBA-M" ),
		tr( "This program is licensed under terms of the GNU General Public License." ) );
}

void MainWnd::showAboutQt()
{
	QMessageBox::aboutQt( this );
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
	test->show();
}
