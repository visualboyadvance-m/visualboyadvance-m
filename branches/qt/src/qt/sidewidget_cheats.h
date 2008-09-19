#ifndef SIDEWIDGET_CHEAT_H
#define SIDEWIDGET_CHEAT_H

#include "precompile.h"

#include "ui_sidewidget_cheats.h"

class SideWidget_Cheats : public QWidget
{
    Q_OBJECT

public:
    SideWidget_Cheats( QWidget *parent = 0 );

private:
    Ui::sidewidget_cheats ui;
};

#endif // #ifndef SIDEWIDGET_CHEAT_H
