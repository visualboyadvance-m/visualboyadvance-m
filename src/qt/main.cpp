#include "main.h"

#include <QApplication>
#include <QPushButton>


int main( int argc, char *argv[] )
{
	QApplication theApp( argc, argv );

	QPushButton hello( TEST_STRING );
	hello.resize( 192, 48 );
	hello.show();

	return theApp.exec();
}
