#ifndef MAINOPTIONS_H
#define MAINOPTIONS_H

#include "precompile.h"

class VideoOptionsPage : public QWidget
{
    Q_OBJECT

public:
    VideoOptionsPage( QWidget *parent = 0 );
};

class InputOptionsPage : public QWidget
{
    Q_OBJECT

public:
    InputOptionsPage( QWidget *parent = 0 );
};

class SoundOptionsPage : public QWidget
{
    Q_OBJECT

public:
    SoundOptionsPage( QWidget *parent = 0 );
};

class DirecOptionsPage : public QWidget
{
    Q_OBJECT

public:
    DirecOptionsPage( QWidget *parent = 0 );
};

#endif
