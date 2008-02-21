 /****************************************************************************
 **
 ** Copyright (C) 2005-2007 Trolltech ASA. All rights reserved.
 **
 ** This file is part of the example classes of the Qt Toolkit.
 **
 ** This file may be used under the terms of the GNU General Public
 ** License version 2.0 as published by the Free Software Foundation
 ** and appearing in the file LICENSE.GPL included in the packaging of
 ** this file.  Please review the following information to ensure GNU
 ** General Public Licensing requirements will be met:
 ** http://trolltech.com/products/qt/licenses/licensing/opensource/
 **
 ** If you are unsure which license is appropriate for your use, please
 ** review the following information:
 ** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
 ** or contact the sales department at sales@trolltech.com.
 **
 ** In addition, as a special exception, Trolltech gives you certain
 ** additional rights. These rights are described in the Trolltech GPL
 ** Exception version 1.0, which can be found at
 ** http://www.trolltech.com/products/qt/gplexception/ and in the file
 ** GPL_EXCEPTION.txt in this package.
 **
 ** In addition, as a special exception, Trolltech, as the sole copyright
 ** holder for Qt Designer, grants users of the Qt/Eclipse Integration
 ** plug-in the right for the Qt/Eclipse Integration to link to
 ** functionality provided by Qt Designer and its related libraries.
 **
 ** Trolltech reserves all rights not expressly granted herein.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ****************************************************************************/

 #include <QtGui>

 #include "configdialog.h"
 #include "MainOptions.h"

 ConfigDialog::ConfigDialog()
 {
     contentsWidget = new QListWidget;
     contentsWidget->setViewMode(QListView::IconMode);
     contentsWidget->setIconSize(QSize(96, 84));
     contentsWidget->setMovement(QListView::Static);
     contentsWidget->setMaximumWidth(128);
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