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
		OPENGL
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
