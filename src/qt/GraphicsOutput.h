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


#ifndef GRAPHICSOUTPUT_H
#define GRAPHICSOUTPUT_H

#include "precompile.h"

// this class uses a QGLWidget with QPainter, which uses OpenGL acceleration if supported
class GraphicsOutput : public QGLWidget
{
	Q_OBJECT

public:
	GraphicsOutput( QWidget *parent );
	~GraphicsOutput();

	enum DISPLAY_API
	{
		NONE,
		QPAINTER,
		OPENGL,
		DIRECT3D
	};

	bool setAPI( DISPLAY_API api );
	DISPLAY_API getAPI();

public slots:
	void render();

protected:
	void paintEvent( QPaintEvent *event );

private:
	DISPLAY_API m_api;
};

#endif // #ifndef GRAPHICSOUTPUT_H
