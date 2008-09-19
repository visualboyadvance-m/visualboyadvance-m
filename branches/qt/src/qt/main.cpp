#include "main.h"

#include "MainWnd.h"

int main( int argc, char *argv[] )
{
    QApplication theApp( argc, argv );
    // create/open ini file for settings
    QSettings settings( "vba-m.ini", QSettings::IniFormat, &theApp );
    QTranslator *translator = 0;
    QTimer emuTimer( 0 );

    MainWnd *mainWnd = new MainWnd( &translator, &settings, &emuTimer, 0 );
    mainWnd->show();

    return theApp.exec();
}
