// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QLibraryInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTranslator>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QApplication>
#include <math.h>
#include <memory>

#include <input/dsoinput.h>

// Settings
#include "dsosettings.h"
#include "viewconstants.h"
#include "viewsettings.h"

// DSO core logic
#include "capturing.h"
#include "dsomodel.h"
#include "input/DsoInput.h"

// Post processing
#include "post/graphgenerator.h"
// #include "post/mathchannelgenerator.h"
#include "post/postprocessing.h"
#include "post/spectrumgenerator.h"

// Exporter
#include "exporting/exportcsv.h"
#include "exporting/exporterprocessor.h"
#include "exporting/exporterregistry.h"
#include "exporting/exportjson.h"

// GUI
#include "mainwindow.h"
//#include "selectdevice/selectsupporteddevice.h"

// OpenGL setup
#include "glscope.h"

#include "models/modelDEMO.h"

#ifndef VERSION
#error "You need to run the cmake buildsystem!"
#endif
#include "OH_VERSION.h"


using namespace Hantek;

// verboseLevel allows the fine granulated tracing of the program for easy testing and debugging
int verboseLevel = 0; // 0: quiet; 1,2: startup; 3,4: + user actions; 5,6: + data processing; 7 + USB

struct InitializeArgs
{
public:
    int theme = 0;
    int toolTipVisible = 1;
    int fontSize = defaultFontSize;
    int condensed = defaultCondensed;
    QString font = defaultFont;

    bool demoMode = false;
    bool styleFusion = false;

    bool useGLES = false;
    bool useGLSL120 = false;
    bool useGLSL150 = false;
    bool useLocale = true;
    bool resetSettings = false;

    QString configFileName = QString();
};

void ParseCommandLine( int argc, char *argv[], InitializeArgs& Args )
{
    // do this early at program start ...
    // get font size and other global program settings:
    // Linux, Unix: $HOME/.config/OpenHantek/OpenHantek6022.conf
    // macOS:       $HOME/Library/Preferences/org.openhantek.OpenHantek6022.plist
    // Windows:     HKEY_CURRENT_USER\Software\OpenHantek\OpenHantek6022"
    // more info:   https://doc.qt.io/qt-5/qsettings.html#platform-specific-notes
    // use default value if setting is not available
    QSettings storeSettings;
    storeSettings.beginGroup( "view" );
    Args.fontSize = storeSettings.value( "fontSize", defaultFontSize ).toInt();
    Args.styleFusion = storeSettings.value( "styleFusion", false ).toBool();
    Args.theme = storeSettings.value( "theme", 0 ).toInt();
    Args.toolTipVisible = storeSettings.value( "toolTipVisible", 1 ).toInt();
    storeSettings.endGroup();

    // Pre-parse international flag so it can affect the command line help texts
    QCoreApplication translationApp( argc, argv );
    QCommandLineParser tp;
    QCommandLineOption tintOption(
                { "i", "international" }, QCoreApplication::translate( "main", "Show the international interface, do not translate" ) );
    tp.addOption( tintOption );
    tp.parse( translationApp.arguments() );
    Args.useLocale = !tp.isSet( tintOption );

    QCoreApplication parserApp( argc, argv );
    QCommandLineParser p;

    //////// Load translations for command line help texts ////////
    QTranslator qtTranslator;
    QTranslator parserTranslator;

    if ( Args.useLocale && QLocale().name() != "en_US" ) { // somehow Qt on MacOS uses the german translation for en_US?!
        if ( qtTranslator.load( "qt_" + QLocale().name(), QLibraryInfo::location( QLibraryInfo::TranslationsPath ) ) ) {
            parserApp.installTranslator( &qtTranslator );
        }
        if ( parserTranslator.load( QLocale(), QLatin1String( "openhantek" ), QLatin1String( "_" ),
                                    QLatin1String( ":/translations" ) ) ) {
            parserApp.installTranslator( &parserTranslator );
        }
    }

    p.addHelpOption();
    p.addVersionOption();
    QCommandLineOption configFileOption( { "c", "config" }, QCoreApplication::translate( "main", "Load config file" ),
                                         QCoreApplication::translate( "main", "File" ) );
    p.addOption( configFileOption );
    QCommandLineOption demoModeOption( { "d", "demoMode" },
                                       QCoreApplication::translate( "main", "Demo mode without scope HW" ) );
    p.addOption( demoModeOption );
    QCommandLineOption useGlesOption( { "e", "useGLES" },
                                      QCoreApplication::translate( "main", "Use OpenGL ES instead of OpenGL" ) );
    p.addOption( useGlesOption );
    QCommandLineOption useGLSL120Option( "useGLSL120", QCoreApplication::translate( "main", "Force OpenGL SL version 1.20" ) );
    p.addOption( useGLSL120Option );
    QCommandLineOption useGLSL150Option( "useGLSL150", QCoreApplication::translate( "main", "Force OpenGL SL version 1.50" ) );
    p.addOption( useGLSL150Option );
    QCommandLineOption intOption( { "i", "international" },
                                  QCoreApplication::translate( "main", "Show the international interface, do not translate" ) );
    p.addOption( intOption );
    QCommandLineOption fontOption( { "f", "font" }, QCoreApplication::translate( "main", "Define the system font" ),
                                   QCoreApplication::translate( "main", "Font" ) );
    p.addOption( fontOption );
    QCommandLineOption sizeOption(
                { "s", "size" },
                QString( QCoreApplication::translate( "main", "Set the font size (default = %1, 0: automatic from dpi)" ) )
                .arg( Args.fontSize ),
                QCoreApplication::translate( "main", "Size" ) );
    p.addOption( sizeOption );
    QCommandLineOption condensedOption(
                "condensed", QCoreApplication::translate( "main", "Set the font condensed value (default = %1)" ).arg( Args.condensed ),
                QCoreApplication::translate( "main", "Condensed" ) );
    p.addOption( condensedOption );
    QCommandLineOption resetSettingsOption(
                "resetSettings", QCoreApplication::translate( "main", "Reset persistent settings, start with default" ) );
    p.addOption( resetSettingsOption );
    QCommandLineOption verboseOption(
                "verbose", QCoreApplication::translate( "main", "Verbose tracing of program startup, ui and processing steps" ),
                QCoreApplication::translate( "main", "Level" ) );
    p.addOption( verboseOption );
    p.process( parserApp );
    if ( p.isSet( configFileOption ) )
        Args.configFileName = p.value( "config" );
    Args.demoMode = p.isSet( demoModeOption );
    if ( p.isSet( fontOption ) )
        Args.font = p.value( "font" );
    if ( p.isSet( sizeOption ) )
        Args.fontSize = p.value( "size" ).toInt();
    if ( p.isSet( condensedOption ) ) // allow range from UltraCondensed (50) to UltraExpanded (200)
        Args.condensed = qBound( 50, p.value( "condensed" ).toInt(), 200 );
    Args.useGLES = p.isSet( useGlesOption );
    Args.useGLSL120 = p.isSet( useGLSL120Option );
    Args.useGLSL150 = p.isSet( useGLSL150Option );
    Args.useLocale = !p.isSet( intOption );
    if ( p.isSet( verboseOption ) )
        verboseLevel = p.value( "verbose" ).toInt();
    Args.resetSettings = p.isSet( resetSettingsOption );
    // ... and forget the no more needed variables
}

void InitPalette(QApplication& openHantekApplication, int theme, bool isKvantum)
{
    // adapt the palette according to the user selected theme (Auto, Light, Dark)
    QPalette palette = QPalette();
    palette.setColor( QPalette::LinkVisited, QPalette().color( QPalette::Link ) ); // do not change the link color

    if ( Dso::Themes::THEME_LIGHT == Dso::Themes( theme ) ) { // Colors from "Breeze" theme
        palette.setColor( QPalette::WindowText, QColor( 35, 38, 39 ) );
        palette.setColor( QPalette::Button, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::Light, QColor( 255, 255, 255 ) );
        palette.setColor( QPalette::Midlight, QColor( 246, 247, 247 ) );
        palette.setColor( QPalette::Dark, QColor( 136, 142, 147 ) );
        palette.setColor( QPalette::Mid, QColor( 196, 200, 204 ) );
        palette.setColor( QPalette::Text, QColor( 35, 38, 39 ) );
        palette.setColor( QPalette::BrightText, QColor( 255, 255, 255 ) );
        palette.setColor( QPalette::ButtonText, QColor( 35, 38, 39 ) );
        palette.setColor( QPalette::Base, QColor( 252, 252, 252 ) );
        palette.setColor( QPalette::Window, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::Shadow, QColor( 71, 74, 76 ) );
        palette.setColor( QPalette::Highlight, QColor( 61, 174, 233 ) );
        palette.setColor( QPalette::HighlightedText, QColor( 252, 252, 252 ) );
        palette.setColor( QPalette::Link, QColor( 41, 128, 185 ) );
        palette.setColor( QPalette::LinkVisited, QColor( 41, 128, 185 ) ); // was 127, 140, 141;
        palette.setColor( QPalette::AlternateBase, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::NoRole, QColor( 0, 0, 0 ) ); // #17
        palette.setColor( QPalette::ToolTipBase, QColor( 35, 38, 39 ) );
        palette.setColor( QPalette::ToolTipText, QColor( 252, 252, 252 ) );
#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 12, 0 ) )
        palette.setColor( QPalette::PlaceholderText, QColor( 35, 38, 39 ) ); // #20, introduced in Qt 5.12
#endif
    } else if ( Dso::Themes::THEME_DARK == Dso::Themes( theme ) || isKvantum ) { // Colors from "Breeze Dark" theme
        palette.setColor( QPalette::WindowText, QColor( 239, 240, 241 ) );       // #0
        palette.setColor( QPalette::Button, QColor( 49, 54, 59 ) );
        palette.setColor( QPalette::Light, QColor( 70, 77, 84 ) );
        palette.setColor( QPalette::Midlight, QColor( 60, 66, 72 ) );
        palette.setColor( QPalette::Dark, QColor( 29, 32, 35 ) );
        palette.setColor( QPalette::Mid, QColor( 43, 48, 52 ) );
        palette.setColor( QPalette::Text, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::BrightText, QColor( 255, 255, 255 ) );
        palette.setColor( QPalette::ButtonText, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::Base, QColor( 35, 38, 41 ) );
        palette.setColor( QPalette::Window, QColor( 49, 54, 59 ) );
        palette.setColor( QPalette::Shadow, QColor( 21, 23, 25 ) );
        palette.setColor( QPalette::Highlight, QColor( 61, 174, 233 ) );
        palette.setColor( QPalette::HighlightedText, QColor( 239, 240, 241 ) );
        palette.setColor( QPalette::Link, QColor( 41, 128, 185 ) );
        palette.setColor( QPalette::LinkVisited, QColor( 41, 128, 185 ) ); // was 127, 140, 141;
        palette.setColor( QPalette::AlternateBase, QColor( 49, 54, 59 ) );
        palette.setColor( QPalette::NoRole, QColor( 0, 0, 0 ) ); // #17
        palette.setColor( QPalette::ToolTipBase, QColor( 49, 54, 59 ) );
        palette.setColor( QPalette::ToolTipText, QColor( 239, 240, 241 ) );
#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 12, 0 ) )
        palette.setColor( QPalette::PlaceholderText, QColor( 239, 240, 241 ) ); // #20, introduced in Qt 5.12
#endif
    }
    openHantekApplication.setPalette( palette );
}

void InitFont(QApplication& openHantekApplication, const QString& font, int fontSize, int condensed)
{
    QFont appFont = openHantekApplication.font();
    if ( 0 == fontSize ) {                               // option -s0 -> use system font size
        fontSize = qBound( 6, appFont.pointSize(), 24 ); // values < 6 do not scale correctly
    }

    appFont.setFamily( font ); // Fusion (or Windows) style + Arial (default) -> fit on small screen (Y >= 720)
    appFont.setStretch( condensed );
    appFont.setPointSize( fontSize ); // scales the widgets accordingly
    // apply new font settings for the scope application

    openHantekApplication.setFont( appFont );
    openHantekApplication.setFont( appFont, "QWidget" ); // on some systems the 2nd argument is required
}

void InitTranslation(QApplication& openHantekApplication, bool useLocale)
{
    //////// Load translations ////////

    QTranslator qtTranslator;
    QTranslator openHantekTranslator;
    if ( useLocale && QLocale().name() != "en_US" ) { // somehow Qt on MacOS uses the german translation for en_US?!
        if ( qtTranslator.load( "qt_" + QLocale().name(), QLibraryInfo::location( QLibraryInfo::TranslationsPath ) ) ) {
            openHantekApplication.installTranslator( &qtTranslator );
        }
        if ( openHantekTranslator.load( QLocale(), QLatin1String( "openhantek" ), QLatin1String( "_" ),
                                        QLatin1String( ":/translations" ) ) ) {
            openHantekApplication.installTranslator( &openHantekTranslator );
        }
    }
}

void InitOpenGLVersion(const InitializeArgs& Args)
{
    // set appropriate OpenGLSL version
    // some not so new intel graphic driver report a very conservative version
    // even if they deliver OpenGL 4.x functions
    // e.g. debian buster -> "2.1 Mesa 18.3.6"
    // Standard W10 installation -> "OpenGL ES 2.0 (ANGLE 2.1.0.57ea533f79a7)"
    // MacOS supports OpenGL 4.4 since 2011, 3.3 before

    // This is the default setting for Mesa (Linux, FreeBSD)
    QString GLSLversion = GLSL120;

#if defined( Q_OS_MAC )
    // recent MacOS uses OpenGL 4.4, but at least 3.3 for very old systems (<2011)
    GLSLversion = GLSL150;
#elif defined( Q_PROCESSOR_ARM )
    // Raspberry Pi crashes with OpenGL, use OpenGLES
    GLSLversion = GLES100;
#endif

    // some fresh W10 installations announce this
    // "OpenGL ES 2.0 (ANGLE ...)"
    if ( QRegularExpression( "OpenGL ES " ).match( GlScope::getOpenGLversion() ).hasMatch() )
        GLSLversion = GLES100; // set as default

    // override default with command line option
    if ( Args.useGLES ) // 1st priority
        GLSLversion = GLES100;
    else if ( Args.useGLSL120 ) // next
        GLSLversion = GLSL120;
    else if ( Args.useGLSL150 ) // least prio
        GLSLversion = GLSL150;

    GlScope::useOpenGLSLversion( GLSLversion ); // prepare the OpenGL renderer
}

/// \brief Initialize resources and translations and show the main window.
int main( int argc, char *argv[] ) {

#ifdef Q_OS_WIN
    // Win: close "extra" console window but if started from cmd.exe use this console
    if ( FreeConsole() && AttachConsole( ATTACH_PARENT_PROCESS ) ) {
        freopen( "CONOUT$", "w", stdout );
        freopen( "CONOUT$", "w", stderr );
    }
#else
    // this ENV variable hides the LANG=xx setting, fkt. not available under Windows
    unsetenv( "LANGUAGE" );
#endif

    QElapsedTimer startupTime;
    startupTime.start(); // time tracking for verbose startup

    //////// Set application information ////////
    QCoreApplication::setOrganizationName( "OpenHantek" );
    QCoreApplication::setOrganizationDomain( "openhantek.org" );
    QCoreApplication::setApplicationName( "OpenHantek6022" );
    QCoreApplication::setApplicationVersion( VERSION );
    QCoreApplication::setAttribute( Qt::AA_UseHighDpiPixmaps, true );
#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 6, 0 ) )
    QCoreApplication::setAttribute( Qt::AA_EnableHighDpiScaling, true );
#endif

    qDebug() << ( QString( "%1 (%2)" ).arg( QCoreApplication::applicationName(), QCoreApplication::applicationVersion() ) )
                .toLocal8Bit()
                .data();

    InitializeArgs Args;
    ParseCommandLine(argc, argv, Args);

    QApplication openHantekApplication( argc, argv );

    // Qt5 linux styles ("Breeze", "Windows" or "Fusion")
    // Linux default:   "Breeze" (screen is taller compared to the other two styles)
    // Windows default: "Windows"
    // kvantum style disturbs UI, fall back to Fusion style with dark default theme
    bool isKvantum = false;//openHantekApplication.style()->objectName().startsWith( "kvantum" );
    if ( Args.styleFusion || isKvantum ) {
        // smaller "Fusion" widgets allow stacking of all four docks even on 1280x720 screen
        if ( verboseLevel )
            qDebug() << startupTime.elapsed() << "ms:"
                     << "set \"Fusion\" style";
        openHantekApplication.setStyle( QStyleFactory::create( "Fusion" ) );
    }
    InitPalette(openHantekApplication, Args.theme, isKvantum);
    openHantekApplication.setStyleSheet( "QToolTip { border: 2px solid white; padding: 2px; border-radius: 5px; font-weight: bold; "
                                         "color: white; background-color: black; }" );

    InitTranslation(openHantekApplication, Args.useLocale);
    InitFont(openHantekApplication, Args.font, Args.fontSize, Args.condensed);

    //////// Create settings object specific to this scope, use unique serial number ////////
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "create settings object";
    DsoSettings settings(4, verboseLevel, Args.resetSettings );
    if ( !Args.configFileName.isEmpty() )
        settings.loadFromFile( Args.configFileName );

    //////// Prepare visual appearance ////////
    // prepare the font size, style and theme settings for the scope application
    settings.scope.toolTipVisible = Args.toolTipVisible; // show hints for beginners
    settings.view.styleFusion = Args.styleFusion;
    settings.view.theme = Args.theme;
    // remember the actual fontsize setting
    settings.view.fontSize = Args.fontSize;


    QThread dsoControlThread;
    dsoControlThread.setObjectName( "dsoControlThread" );
    DsoInput dsoControl(&settings, verboseLevel);
    dsoControl.moveToThread( &dsoControlThread );

    //////// Create exporters ////////
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "create exporters";
    ExporterRegistry exportRegistry( &settings );
    ExporterCSV exporterCSV;
    ExporterJSON exporterJSON;
    ExporterProcessor samplesToExportRaw( &exportRegistry );
    exportRegistry.registerExporter( &exporterCSV );
    exportRegistry.registerExporter( &exporterJSON );

    //////// Create post processing objects ////////
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "create post processing objects";
    QThread postProcessingThread;
    postProcessingThread.setObjectName( "postProcessingThread" );
    PostProcessing postProcessing( settings.scope.countChannels(), verboseLevel );

    SpectrumGenerator spectrumGenerator( &settings.scope, &settings.analysis );
    // math channel is now calculated in DsoInput
    // MathChannelGenerator mathchannelGenerator( &settings.scope, spec->channels );
    GraphGenerator graphGenerator( &settings.scope, &settings.view );

    postProcessing.registerProcessor( &samplesToExportRaw );
    // postProcessing.registerProcessor( &mathchannelGenerator );
    //    postProcessing.registerProcessor( &spectrumGenerator );
    postProcessing.registerProcessor( &graphGenerator );

    postProcessing.moveToThread( &postProcessingThread );
    QObject::connect( &dsoControl, &DsoInput::samplesAvailable, &postProcessing, &PostProcessing::input );
    QObject::connect( &postProcessing, &PostProcessing::processingFinished, &exportRegistry, &ExporterRegistry::input, Qt::DirectConnection );
    QObject::connect( &dsoControl, &DsoInput::start, &dsoControl, &DsoInput::restartSampling);
    dsoControl.StartSample();

    InitOpenGLVersion(Args);

    //////// Create main window ////////
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "create main window";
    MainWindow openHantekMainWindow( &dsoControl, &settings, &exportRegistry );
    QObject::connect( &postProcessing, &PostProcessing::processingFinished, &openHantekMainWindow, &MainWindow::showNewData );
    QObject::connect( &exportRegistry, &ExporterRegistry::exporterProgressChanged, &openHantekMainWindow,
                      &MainWindow::exporterProgressChanged );
    QObject::connect( &exportRegistry, &ExporterRegistry::exporterStatusChanged, &openHantekMainWindow,
                      &MainWindow::exporterStatusChanged );
    openHantekMainWindow.show();

    //////// Start DSO thread and go into GUI main loop
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "start DSO control thread";
    dsoControl.enableSamplingUI();
    postProcessingThread.start();
    dsoControlThread.start();
    //    Capturing capturing( &dsoControlThread );
    //    capturing.start();

    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "execute GUI main loop";
    int appStatus = openHantekApplication.exec();

    //////// Application closed, clean up step by step ////////
    if ( verboseLevel )
        qDebug() << startupTime.elapsed() << "ms:"
                 << "application closed, clean up";

    std::cerr << std::unitbuf; // enable automatic flushing

    // the stepwise text output gives some hints about the shutdown timing
    // not needed with appropriate verbose level
    if ( verboseLevel < 2 )
        std::cerr << "OpenHantek6022 "; // 1st part

    dsoControl.quitSampling(); // send USB control command, stop bulk transfer

    // stop the capturing thread
    // wait 2 * record time (delay is ms) for dso to finish
    unsigned waitForDso = unsigned( 2000 * dsoControl.getSamplesize() / dsoControl.getSamplerate() );
    waitForDso = qMax( waitForDso, 10000U ); // wait for at least 10 s
    //    capturing.requestInterruption();
    //    capturing.wait( waitForDso );
    if ( verboseLevel < 2 )
        std::cerr << "has "; // 2nd part

    // now quit the data acquisition thread
    // wait 2 * record time (delay is ms) for dso to finish
    dsoControlThread.quit();
    dsoControlThread.wait( waitForDso );
    if ( verboseLevel < 2 )
        std::cerr << "stopped "; // 3rd part

    // next stop the data processing
    postProcessing.stop();
    postProcessingThread.quit();
    postProcessingThread.wait( 10000 );
    if ( verboseLevel < 2 )
        std::cerr << "after "; // 4th part

    //    dsoControl.prepareForShutdown();

    return appStatus;
}
