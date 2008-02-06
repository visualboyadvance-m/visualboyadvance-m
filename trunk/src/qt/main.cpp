#include "main.h"

#include <QApplication>
#include <QPushButton>
#include <QTranslator>


int main( int argc, char *argv[] )
{
	QApplication theApp( argc, argv );

	QTranslator translator;
	translator.load( "lang/german" );
	theApp.installTranslator( &translator );

	QPushButton hello( QPushButton::tr( "Hello World!" ) );
	hello.resize( 192, 48 );
	hello.show();

	return theApp.exec();
}
