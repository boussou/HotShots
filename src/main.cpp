/******************************************************************************
   HotShots: Screenshot utility
   Copyright(C) 2011-2014  xbee@xbee.net
   2017-2018

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *******************************************************************************/

#include <QtCore/QTextStream>

#include <QApplication>

#include <QDesktopWidget>
#include <QMessageBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTimer>

#include "MainWindow.h"
#include "SingleApplication.h"
#include "AppSettings.h"
#include "MiscFunctions.h"
#include "SplashScreen.h"
#include "editor/EditorWidget.h"

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep
#endif

#ifdef Q_OS_UNIX
#include <time.h>
#endif

// extract from QTest ...
void qSleep(int ms)
{
#ifdef Q_OS_WIN
    Sleep( uint(ms) );
#else
    struct timespec ts = {
        ms / 1000, (ms % 1000) * 1000 * 1000
    };
    nanosleep(&ts, NULL);
#endif
}



int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(hotshots);

    qSleep(1000); // waiting sometimes for restart case ...

    SingleApplication app(argc, argv, PACKAGE_NAME);
    app.setQuitOnLastWindowClosed(false); // because of systray
    app.setApplicationVersion(QStringLiteral("2.2.1"));

/* it seems to me it is useless code ?
    // add possible image plugin path
#ifdef Q_OS_MAC
    app.addLibraryPath(QApplication::applicationDirPath() + "/../plugins");
#endif
    app.addLibraryPath( QApplication::applicationDirPath() + "./plugins");
*/

    // update some application infos (use by some platform for temp storage, ...)
    MiscFunctions::updateApplicationIdentity();

    // set default language for application (computed or saved)
    MiscFunctions::setDefaultLanguage();

    // check for special argument
    bool forceResetConfig = false;
    bool ignoreSingleInstance = false;
    bool editorOnly = false;
    QCommandLineOption resetOpt(QStringList() << "r" << "reset-config", "Reset all configuration settings to defaults");
    QCommandLineOption portablOpt(QStringList() << "p" << "portable", "Run in portable mode (settings stored locally)");
    QCommandLineOption nosingleOpt(QStringList() << "n" << "no-singleinstance", "Allow multiple instances of the application");
    QCommandLineOption fileOpt(QStringList() << "f" << "file", "Load specified file in editor", "filename");
    QCommandLineOption editorOpt(QStringList() << "e" << "edit", "Launch editor window directly (bypass main window)");


    QString fileToLoad;
    QCommandLineParser parser;
    parser.setApplicationDescription("HotShots - Screenshot utility with built-in editor\n\n"
                                   "HotShots allows you to capture screenshots and edit them with "
                                   "various tools including shapes, text, arrows, and effects.\n\n"
                                   "You can also specify a filename directly as an argument to open it in the editor.");
    parser.addHelpOption();  //help -  adds -h and --help options to the command line
    parser.addVersionOption();
    parser.addOption(resetOpt);
    parser.addOption(portablOpt);
    parser.addOption(nosingleOpt);
    parser.addOption(editorOpt);
    parser.addOption(fileOpt);

    parser.process(app.arguments());

    if(parser.isSet(resetOpt))
    {
        forceResetConfig = true;
    }
    
    if(parser.isSet(portablOpt))
    {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QApplication::applicationDirPath());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QApplication::applicationDirPath());
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }
    
    if(parser.isSet(nosingleOpt))
    {
        ignoreSingleInstance = true;
    }
    
    if(parser.isSet(editorOpt))
    {
        editorOnly = true;
        ignoreSingleInstance = true;
    }
    
    if(parser.isSet(fileOpt))
    {
        fileToLoad = parser.value(fileOpt);
        editorOnly = true;  // Auto-enable editor mode when loading a file
        ignoreSingleInstance = true;
    }
    
    // Check for positional arguments (non-option parameters) and treat them as filenames
    QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty() && fileToLoad.isEmpty()) {
        fileToLoad = positionalArgs.first();
        editorOnly = true;  // Auto-enable editor mode when loading a file
        ignoreSingleInstance = true;
    }
    
    // Handle combined -e and -f options
    if(parser.isSet(editorOpt) && parser.isSet(fileOpt))
    {
        fileToLoad = parser.value(fileOpt);
        editorOnly = true;
        ignoreSingleInstance = true;
    }

    // check for multiple instance of program
    if (app.isRunning() && !ignoreSingleInstance)
    {
        app.sendMessage( fileToLoad );
        if ( fileToLoad.isEmpty() )
            QMessageBox::critical( 0,PACKAGE_NAME,QObject::tr("%1 is already running!!").arg(PACKAGE_NAME) );
        return 0;
    }

    AppSettings settings;

    // in order to display splashscreen on the same screen than application ...
    settings.beginGroup("MainWindow");
    int screenNumber = ( settings.value( "screenNumber",QApplication::desktop()->primaryScreen() ).toInt() ) % ( QApplication::desktop()->screenCount() ); // in order to be sure to display on a right screen
    settings.endGroup();

    settings.beginGroup("Application");

    // reset the saved configuration if needed
    bool resetConfig = settings.value("resetConfig",false).toBool();
    if (resetConfig || forceResetConfig)
    {
        settings.clear();
        settings.sync();
    }

    bool splashscreenAtStartup = settings.value("splashscreenAtStartup",true).toBool();
    bool splashscreenTransparentBackground = settings.value("splashscreenTransparentBackground",true).toBool();
    bool startInTray = settings.value("startInTray",false).toBool();

    settings.endGroup();

    SplashScreen *sScreen = NULL;

    // splash screen
    if (splashscreenAtStartup)
    {
        sScreen = new SplashScreen(QPixmap(":/hotshots/splashscreen.png"), 3000, screenNumber,splashscreenTransparentBackground);
        sScreen->show();
    }

    if (editorOnly)
    {
        // Launch editor directly
        EditorWidget *editor = new EditorWidget();
        editor->show();
        if ( !fileToLoad.isEmpty() )
        {
            editor->load(fileToLoad);
            // Ensure the image is properly scaled to fit the window
            QTimer::singleShot(100, [editor]() {
                editor->fitToView();
            });
        }
        return app.exec();
    }
    else
    {
        MainWindow w;
        QObject::connect( &app, SIGNAL( messageAvailable(const QString &) ), &w, SLOT( wakeUp(const QString &) ) );

        // splash screen
        if (splashscreenAtStartup)
        {
            if (!startInTray)
                sScreen->delayedFinish(&w);
        }

        {
#ifdef _DEBUG // ugly hack for debugging
            if (startInTray)
                w.show();
#endif
            if (!startInTray)
                w.show();
        }

        if ( !fileToLoad.isEmpty() )
            w.openEditor(fileToLoad);

        return app.exec();
    }
}
