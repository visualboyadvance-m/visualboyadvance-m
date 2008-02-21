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
	QGroupBox *RenderGroup = new QGroupBox(tr("Renderer Selection"));
	QLabel *RenderLabel = new QLabel(tr("Renderer:"));
	QComboBox *RenderCombo = new QComboBox;
	RenderCombo->addItem(tr("OGL"));
	RenderCombo->addItem(tr("D3D"));
	RenderCombo->addItem(tr("QPainter"));

	QHBoxLayout *RenderLayout = new QHBoxLayout;
	RenderLayout->addWidget(RenderLabel);
	RenderLayout->addWidget(RenderCombo);

	QVBoxLayout *configLayout = new QVBoxLayout;
	configLayout->addLayout(RenderLayout);
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
}
