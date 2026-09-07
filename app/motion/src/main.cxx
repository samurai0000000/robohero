/*
 * main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <QApplication>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "MainWindow.hxx"
#include "UrdfLimits.hxx"
#include "MotionLibrary.hxx"
#include "ParametricGait.hxx"
#include "MotionSequence.hxx"

static QString s_lastQpaWarning;

static void gracefulMessageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg)
{
    Q_UNUSED(context);

    if (type == QtWarningMsg || type == QtCriticalMsg) {
        if (msg.contains("display", Qt::CaseInsensitive) ||
            msg.contains("xcb", Qt::CaseInsensitive) ||
            msg.contains("platform", Qt::CaseInsensitive)) {
            s_lastQpaWarning = msg;
        }
        std::cerr << msg.toStdString() << std::endl;
        return;
    }

    if (type == QtFatalMsg) {
        std::cerr << "\nrobohero_motion: fatal error: " << msg.toStdString() << "\n" << std::endl;
        if (msg.contains("platform plugin", Qt::CaseInsensitive) ||
            msg.contains("could not connect to display", Qt::CaseInsensitive) ||
            !s_lastQpaWarning.isEmpty()) {
            std::cerr << "=================================================================\n"
                      << "RoboHero Motion Studio requires a functioning graphical display.\n"
                      << "Failed to connect to the display server.\n\n"
                      << "Suggestions:\n"
                      << "  1. If running over SSH, ensure an X11 server is running locally\n"
                      << "     (e.g., XQuartz on macOS, VcXsrv/MobaXterm on Windows) and\n"
                      << "     reconnect with trusted X11 forwarding: 'ssh -Y <host>'.\n"
                      << "  2. For headless CLI operations, use:\n"
                      << "     ./robohero_motion --help\n"
                      << "     ./robohero_motion --generate-presets\n"
                      << "  3. If on Windows, compile and run natively with MSVC:\n"
                      << "     cmake --preset windows-msvc && cmake --build --preset windows-msvc\n"
                      << "=================================================================\n"
                      << std::endl;
        }
        // Exit cleanly with code 1 instead of calling abort() to prevent core dumps
        std::exit(1);
    }
}

static void printUsage(const char *progName)
{
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n"
              << "RoboHero Motion & Gait Studio\n\n"
              << "Options:\n"
              << "  --generate-presets   Regenerate all built-in motion files (.rhm.json)\n"
              << "  --help, -h           Show this help message\n"
              << std::endl;
}

int main(int argc, char *argv[])
{
    // 1. Check for command-line help before initializing GUI
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    // 2. Install graceful message handler to prevent core dumps if display/QPA fails
    qInstallMessageHandler(gracefulMessageHandler);

    // 3. For CLI preset generation, force offscreen platform so no display is needed
    bool generatePresets = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--generate-presets") {
            generatePresets = true;
            qputenv("QT_QPA_PLATFORM", "offscreen");
            break;
        }
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    // 4. Pre-check display environment on Linux/Unix if not running offscreen
    if (!generatePresets && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        const char *disp = std::getenv("DISPLAY");
        const char *wayland = std::getenv("WAYLAND_DISPLAY");
        if ((!disp || std::strlen(disp) == 0) && (!wayland || std::strlen(wayland) == 0)) {
            std::cerr << "robohero_motion: error: No graphical display detected ($DISPLAY or $WAYLAND_DISPLAY is unset).\n"
                      << "RoboHero Motion Studio requires a graphical display to launch the 3D studio.\n\n"
                      << "Use '--help' or '--generate-presets' for command-line operations.\n"
                      << std::endl;
            return 1;
        }
    }
#endif

    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("RoboHero");
    QCoreApplication::setApplicationName("RoboHeroMotion");
    QCoreApplication::setApplicationVersion("1.0.0");

    app.setWindowIcon(QIcon(":/icons/robohero_icon.png"));

    // Initialize calibration and URDF limits from compiled-in resources
    if (!UrdfLimits::instance().init(":/model/robohero.urdf", ":/model/calibration.json")) {
        qWarning("Failed to initialize URDF limits from resources.");
    }

    if (generatePresets) {
        QString motionsDir = "motions";
        if (!QDir(motionsDir).exists()) {
            motionsDir = "app/motion/motions";
        }
        if (!QDir(motionsDir).exists()) {
            motionsDir = "../motions";
        }

        ParametricGait::GaitParameters pFwd;
        pFwd.mode = "forward";
        pFwd.strideLengthMm = 30.0;
        pFwd.stepHeightMm = 15.0;
        pFwd.swayAmplitudeMm = 14.0;
        pFwd.stepDurationMs = 500;
        pFwd.torsoPitchDeg = 2.0;
        pFwd.armSwingDeg = 15.0;
        pFwd.cycleCount = 1;
        MotionSequence fwd = ParametricGait::generateGait(pFwd);
        fwd.setName("Forward Walk");
        fwd.setDescription("Smooth continuous bipedal forward walking gait with lateral balance sway and arm swing");
        fwd.saveToFile((motionsDir + "/walk_forward.rhm.json").toStdString());

        ParametricGait::GaitParameters pTurn;
        pTurn.mode = "turn_left";
        pTurn.strideLengthMm = 20.0;
        pTurn.stepHeightMm = 15.0;
        pTurn.swayAmplitudeMm = 14.0;
        pTurn.turnAngleDeg = 15.0;
        pTurn.stepDurationMs = 500;
        pTurn.torsoPitchDeg = 2.0;
        pTurn.armSwingDeg = 12.0;
        pTurn.cycleCount = 1;
        MotionSequence turn = ParametricGait::generateGait(pTurn);
        turn.setName("Turn Left Gait");
        turn.setDescription("Smooth turning gait with differential foot unweighting and anticipatory head pan");
        turn.saveToFile((motionsDir + "/walk_turn_left.rhm.json").toStdString());

        MotionSequence bow = ParametricGait::generateGesture("bow");
        bow.saveToFile((motionsDir + "/bow.rhm.json").toStdString());

        MotionSequence wave = ParametricGait::generateGesture("wave");
        wave.saveToFile((motionsDir + "/wave.rhm.json").toStdString());

        qInfo("Successfully generated all built-in motion presets in %s", qPrintable(motionsDir));
        return 0;
    }

    // Initialize motion library and scan presets & user directories
    MotionLibrary::instance().init();

    // Show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
