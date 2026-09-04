/*
 * main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <QApplication>
#include <QSurfaceFormat>
#include <QIcon>
#include "TeleopConfig.hxx"
#include "MainWindow.hxx"

int main(int argc, char *argv[])
{
#ifdef _WIN32
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat glFormat;
    glFormat.setRenderableType(QSurfaceFormat::OpenGL);
    glFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
    glFormat.setVersion(2, 1);
    glFormat.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(glFormat);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("RoboHero Teleoperation");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/robohero_head.png"));

    TeleopConfig::instance().load();
    qRegisterMetaType<PoseEstimator::PersonPose>("PoseEstimator::PersonPose");
    qRegisterMetaType<std::array<int, 17>>("std::array<int, 17>");
    qRegisterMetaType<std::array<bool, 17>>("std::array<bool, 17>");
    qRegisterMetaType<std::array<double, 17>>("std::array<double, 17>");

    MainWindow window;
    window.show();

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
