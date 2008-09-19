#ifndef MAINWND_H
#define MAINWND_H

#include "precompile.h"

#include "EmuManager.h"
#include "GraphicsOutput.h"

class MainWnd : public QMainWindow
{
	Q_OBJECT

public:
	MainWnd( QTranslator **translator, QSettings *settings, QTimer *emuTimer, QWidget *parent = 0 );
	~MainWnd();

public slots:
	void closeEvent( QCloseEvent * );

private:
	void loadSettings();
	void saveSettings();
	bool loadTranslation();
	void createActions();
	void createMenus();
	void createDockWidgets();
	bool createDisplay();

	QTranslator **translator;
	QString languageFile;
	QSettings *settings;
	QMenu *fileMenu;
	QMenu *settingsMenu;
	QAction *enableTranslationAct;
	QMenu *toolsMenu;
	QMenu *helpMenu;
	QDockWidget *dockWidget_cheats;
	GraphicsOutput *graphicsOutput;

	EmuManager *emuManager;
	QTimer *emuTimer;

private slots:
	bool selectLanguage();
	bool enableTranslation( bool enable );
	void showAbout();
	void showAboutOpenGL();
	void showOpenRom();
	void playRom();
	void pauseRom();
	void executeRom();
	void closeRom();
	void showMainOptions();
};

#endif // #ifndef MAINWND_H
