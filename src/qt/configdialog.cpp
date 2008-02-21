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

 #include <QtGui>

 #include "configdialog.h"
 #include "MainOptions.h"

 ConfigDialog::ConfigDialog()
 {
     contentsWidget = new QListWidget;
     contentsWidget->setViewMode(QListView::IconMode);
     contentsWidget->setIconSize(QSize(64, 64));
     contentsWidget->setMovement(QListView::Static);
     contentsWidget->setResizeMode(QListView::Adjust);
     contentsWidget->setMaximumWidth(130);
     contentsWidget->setSpacing(12);

     pagesWidget = new QStackedWidget;
     pagesWidget->addWidget(new VideoOptionsPage);
     pagesWidget->addWidget(new SoundOptionsPage);
     pagesWidget->addWidget(new InputOptionsPage);

     QPushButton *closeButton = new QPushButton(tr("Close"));

     createIcons();
     contentsWidget->setCurrentRow(0);

     connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

     QHBoxLayout *horizontalLayout = new QHBoxLayout;
     horizontalLayout->addWidget(contentsWidget);
     horizontalLayout->addWidget(pagesWidget, 1);

     QHBoxLayout *buttonsLayout = new QHBoxLayout;
     buttonsLayout->addStretch(1);
     buttonsLayout->addWidget(closeButton);

     QVBoxLayout *mainLayout = new QVBoxLayout;
     mainLayout->addLayout(horizontalLayout);
     mainLayout->addStretch(1);
     mainLayout->addSpacing(12);
     mainLayout->addLayout(buttonsLayout);
     setLayout(mainLayout);

     setWindowTitle(tr("VBA-M Options"));
 }

 void ConfigDialog::createIcons()
 {
     QListWidgetItem *VideoButton = new QListWidgetItem(contentsWidget);
     VideoButton->setIcon(QIcon(":/resources/video.png"));
     VideoButton->setText(tr("Video"));
     VideoButton->setTextAlignment(Qt::AlignHCenter);
     VideoButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

     QListWidgetItem *SoundButton = new QListWidgetItem(contentsWidget);
     SoundButton->setIcon(QIcon(":/resources/sound.png"));
     SoundButton->setText(tr("Sound"));
     SoundButton->setTextAlignment(Qt::AlignHCenter);
     SoundButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

     QListWidgetItem *InputButton = new QListWidgetItem(contentsWidget);
     InputButton->setIcon(QIcon(":/resources/input.png"));
     InputButton->setText(tr("Input"));
     InputButton->setTextAlignment(Qt::AlignHCenter);
     InputButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

     connect(contentsWidget,
             SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
             this, SLOT(changePage(QListWidgetItem *, QListWidgetItem*)));
 }

 void ConfigDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
 {
     if (!current)
         current = previous;

     pagesWidget->setCurrentIndex(contentsWidget->row(current));
 } 