
 #ifndef CONFIGDIALOG_H
 #define CONFIGDIALOG_H

 #include <QDialog>

 class QListWidget;
 class QListWidgetItem;
 class QStackedWidget;

 class ConfigDialog : public QDialog
 {
     Q_OBJECT

 public:
     ConfigDialog();

 public slots:
     void changePage(QListWidgetItem *current, QListWidgetItem *previous);

 private:
     void createIcons();

     QListWidget *contentsWidget;
     QStackedWidget *pagesWidget;
 };

 #endif 