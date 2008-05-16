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


#include "MainOptions.h"

VideoOptionsPage::VideoOptionsPage(QWidget *parent)
	: QWidget(parent)
{
        QGroupBox *RenderGroup = new QGroupBox(tr("Renderer Options"));
        QLabel *RenderLabel = new QLabel(tr("Renderer:"));
        QComboBox *RenderCombo = new QComboBox;
        RenderCombo->addItem("OpenGL");
        RenderCombo->addItem("QPainter");

        QLabel *ResLabel = new QLabel(tr("Resolution:"));
        QComboBox *ResCombo = new QComboBox;
        ResCombo->addItem("320x200");
        ResCombo->addItem("320x240");
        ResCombo->addItem("400x300");
        ResCombo->addItem("512x384");
        ResCombo->addItem("640x480");
        ResCombo->addItem("800x600");
        ResCombo->addItem("1024x768");
        ResCombo->addItem("1280x1024");

        QLabel *SWFilterLabel = new QLabel(tr("Software Filter:"));
        QComboBox *SWFilterCombo = new QComboBox;
        SWFilterCombo->addItem("2xSai");
        SWFilterCombo->addItem("HQ2X");
        SWFilterCombo->addItem("HQ3X");
        SWFilterCombo->addItem("HQ4X");
        SWFilterCombo->addItem("Super Eagle");
        SWFilterCombo->addItem("SuperScale");
        SWFilterCombo->addItem("Bilinear Plus");

        QLabel *HWFilterLabel = new QLabel(tr("Hardware Filter:"));
        QComboBox *HWFilterCombo = new QComboBox;
        HWFilterCombo->addItem("Nearest");
        HWFilterCombo->addItem("Bilinear");

        QHBoxLayout *RenderLayout = new QHBoxLayout;
        RenderLayout->addWidget(RenderLabel);
        RenderLayout->addWidget(RenderCombo);

        QHBoxLayout *ResLayout = new QHBoxLayout;
        ResLayout->addWidget(ResLabel);
        ResLayout->addWidget(ResCombo);

        QHBoxLayout *SWFilterLayout = new QHBoxLayout;
        SWFilterLayout->addWidget(SWFilterLabel);
        SWFilterLayout->addWidget(SWFilterCombo);

        QHBoxLayout *HWFilterLayout = new QHBoxLayout;
        HWFilterLayout->addWidget(HWFilterLabel);
        HWFilterLayout->addWidget(HWFilterCombo);

        QCheckBox *TripleBufCheckBox = new QCheckBox(tr("Triple Buffering"));
        QCheckBox *VerticalSyncCheckBox = new QCheckBox(tr("Vertical Sync"));
        QCheckBox *DisableStatCheckBox = new QCheckBox(tr("Disable status messages"));


        QGridLayout *CheckLayout = new QGridLayout;
        CheckLayout->addWidget(TripleBufCheckBox, 3, 2);
        CheckLayout->addWidget(VerticalSyncCheckBox, 4, 2);
        CheckLayout->addWidget(DisableStatCheckBox, 5, 2);

        QVBoxLayout *configLayout = new QVBoxLayout;
        configLayout->addLayout(RenderLayout);
        configLayout->addLayout(ResLayout);
        configLayout->addLayout(HWFilterLayout);
        configLayout->addLayout(SWFilterLayout);
        configLayout->addLayout(CheckLayout);
        RenderGroup->setLayout(configLayout);

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(RenderGroup);
        mainLayout->addStretch(1);
        setLayout(mainLayout);
}


InputOptionsPage::InputOptionsPage(QWidget *parent)
	: QWidget(parent)
{
        QGroupBox *InputGroup = new QGroupBox(tr("Input Keys"));

        QLabel *StartLabel = new QLabel(tr("Start:"));
        QLineEdit *StartEdit = new QLineEdit;

        QLabel *SelectLabel = new QLabel(tr("Select:"));
        QLineEdit *SelectEdit = new QLineEdit;

        QLabel *UpLabel = new QLabel(tr("Up:"));
        QLineEdit *UpEdit = new QLineEdit;

        QLabel *DownLabel = new QLabel(tr("Down:"));
        QLineEdit *DownEdit = new QLineEdit;

        QLabel *LeftLabel = new QLabel(tr("Left:"));
        QLineEdit *LeftEdit = new QLineEdit;

        QLabel *RightLabel = new QLabel(tr("Right:"));
        QLineEdit *RightEdit = new QLineEdit;

        QLabel *ALabel = new QLabel(tr("A:"));
        QLineEdit *AEdit = new QLineEdit;

        QLabel *BLabel = new QLabel(tr("B:"));
        QLineEdit *BEdit = new QLineEdit;

        QLabel *LLabel = new QLabel(tr("L:"));
        QLineEdit *LEdit = new QLineEdit;

        QLabel *RLabel = new QLabel(tr("R:"));
        QLineEdit *REdit = new QLineEdit;

        QLabel *GSLabel = new QLabel(tr("Gameshark:"));
        QLineEdit *GSEdit = new QLineEdit;

        QLabel *SpeedUpLabel = new QLabel(tr("Speed Up:"));
        QLineEdit *SpeedUpEdit = new QLineEdit;

        QLabel *ScreenshotLabel = new QLabel(tr("Screenshot:"));
        QLineEdit *ScreenshotEdit = new QLineEdit;

        QCheckBox *MultipleAssignCheckBox = new QCheckBox(tr("Multiple key assignments"));



        QGridLayout *InputLayout = new QGridLayout;
        InputLayout->addWidget(StartLabel, 0, 0);
        InputLayout->addWidget(StartEdit, 0, 1);
        InputLayout->addWidget(SelectLabel, 1, 0);
        InputLayout->addWidget(SelectEdit, 1, 1);
        InputLayout->addWidget(UpLabel, 2, 0);
        InputLayout->addWidget(UpEdit, 2, 1);
        InputLayout->addWidget(DownLabel, 3, 0);
        InputLayout->addWidget(DownEdit, 3, 1);
        InputLayout->addWidget(LeftLabel, 4, 0);
        InputLayout->addWidget(LeftEdit, 4, 1);
        InputLayout->addWidget(RightLabel, 5, 0);
        InputLayout->addWidget(RightEdit, 5, 1);
        InputLayout->addWidget(ALabel, 6, 0);
        InputLayout->addWidget(AEdit, 6, 1);
        InputLayout->addWidget(BLabel, 7, 0);
        InputLayout->addWidget(BEdit, 7, 1);
        InputLayout->addWidget(LLabel, 8, 0);
        InputLayout->addWidget(LEdit, 8, 1);
        InputLayout->addWidget(RLabel, 9, 0);
        InputLayout->addWidget(REdit, 9, 1);
        InputLayout->addWidget(GSLabel, 0, 2);
        InputLayout->addWidget(GSEdit, 0, 3);
        InputLayout->addWidget(SpeedUpLabel, 1, 2);
        InputLayout->addWidget(SpeedUpEdit, 1, 3);
        InputLayout->addWidget(ScreenshotLabel, 2, 2);
        InputLayout->addWidget(ScreenshotEdit, 2, 3);
        InputLayout->addWidget(MultipleAssignCheckBox, 3, 2);


        InputGroup->setLayout(InputLayout);

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(InputGroup);
        mainLayout->addSpacing(12);
        mainLayout->addStretch(1);
        setLayout(mainLayout);

}


SoundOptionsPage::SoundOptionsPage(QWidget *parent)
: QWidget(parent)
{
       	QGroupBox *SoundGroup = new QGroupBox(tr("Sound Options"));

       	QLabel *APILabel = new QLabel(tr("Sound API:"));
       	QComboBox *APICombo = new QComboBox;
       	APICombo->addItem("DirectSound");
       	APICombo->addItem("OpenAL");
       	APICombo->addItem("XAudio2");
       	APICombo->addItem("OSS");

       	QLabel *SRLabel = new QLabel(tr("Sample Rate:"));
       	QComboBox *SRCombo = new QComboBox;
       	SRCombo->addItem("11025 Hz");
       	SRCombo->addItem("22050 Hz");
       	SRCombo->addItem("44100 Hz");
       	SRCombo->addItem("48000 Hz");

       	QHBoxLayout *APILayout = new QHBoxLayout;
       	APILayout->addWidget(APILabel);
       	APILayout->addWidget(APICombo);

       	QHBoxLayout *SRLayout = new QHBoxLayout;
       	SRLayout->addWidget(SRLabel);
       	SRLayout->addWidget(SRCombo);

       	QCheckBox *SoundSyncCheckBox = new QCheckBox(tr("Sync game to audio"));
        QCheckBox *MuteAudioCheckBox = new QCheckBox(tr("Mute audio"));
        QCheckBox *InterpolateCheckBox = new QCheckBox(tr("Interpolate audio"));
        QCheckBox *EchoCheckBox = new QCheckBox(tr("GB audio echo"));

       	QGridLayout *CheckLayout = new QGridLayout;
       	CheckLayout->addWidget(SoundSyncCheckBox, 3, 2);
       	CheckLayout->addWidget(MuteAudioCheckBox, 4, 2);
       	CheckLayout->addWidget(InterpolateCheckBox, 5, 2);
        CheckLayout->addWidget(EchoCheckBox, 6, 2);

       	QVBoxLayout *configLayout = new QVBoxLayout;
       	configLayout->addLayout(APILayout);
       	configLayout->addLayout(SRLayout);
        configLayout->addLayout(CheckLayout);
       	SoundGroup->setLayout(configLayout);

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(SoundGroup);
        mainLayout->addStretch(1);
        setLayout(mainLayout);
}

DirecOptionsPage::DirecOptionsPage(QWidget *parent)
: QWidget(parent)
{
        QGroupBox *FolderGroup = new QGroupBox(tr("Directory Options"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(FolderGroup);
        mainLayout->addStretch(1);
        setLayout(mainLayout);
}

