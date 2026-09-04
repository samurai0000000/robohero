/*
 * main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <QApplication>
#include <QIcon>
#include "MainWindow.hxx"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("RoboHero Teleoperation");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/robohero_head.png"));

    qRegisterMetaType<PoseEstimator::PersonPose>("PoseEstimator::PersonPose");

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
