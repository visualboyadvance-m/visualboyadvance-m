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


#include "main.h"

#include "MainWnd.h"

int main( int argc, char *argv[] )
{
	QApplication theApp( argc, argv );

	QTranslator translator;
	translator.load( "lang/german" );
	theApp.installTranslator( &translator );

	MainWnd *mainWnd = new MainWnd();
	mainWnd->show();

	return theApp.exec();
}
