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


#include "GraphicsOutput.h"


GraphicsOutput::GraphicsOutput( QWidget *parent)
: QGLWidget( parent ),
  m_api( NONE )
{
}


GraphicsOutput::~GraphicsOutput()
{
}


bool GraphicsOutput::setAPI( GraphicsOutput::DISPLAY_API api )
{
	if( ( api == OPENGL ) && ( !QGLFormat::hasOpenGL() ) ) {
		return false;
	}

	m_api = api;

	return true;
}


GraphicsOutput::DISPLAY_API GraphicsOutput::getAPI()
{
	return m_api;
}


void GraphicsOutput::render()
{
	repaint(); // immediately calls paintEvent()
}


void GraphicsOutput::paintEvent( QPaintEvent * )
{
	if( m_api == NONE ) return;

	QPainter painter;

	painter.begin( this );

	painter.setRenderHint( QPainter::Antialiasing ); // enable AA if supported by OpenGL device

	painter.setRenderHint( QPainter::TextAntialiasing );

	painter.setRenderHint( QPainter::HighQualityAntialiasing );

	painter.fillRect( rect(), QBrush( QColor( 0xFF, 0x00, 0x00 ), Qt::SolidPattern ) );

	painter.setPen( QColor( 0x00, 0x00, 0xFF ) );
	painter.drawEllipse( rect() );

	static unsigned int counter = 0;
	static QTime firstTime = QTime::currentTime();

	int tDiff = firstTime.secsTo( QTime::currentTime() );

	int fps = 0;

	if( tDiff != 0 ) {
		fps = counter / tDiff;
	}

	painter.setPen( QColor( 0x00, 0xFF, 0x00 ) );
	painter.setFont( QFont( "Arial", 14, QFont::Bold ) );
	painter.drawText( rect(), Qt::AlignCenter, "Frame Number " + QString::number( counter++ ) +
		"\nFPS: " + QString::number( fps ) );

	painter.end();
}
