/*
 * UrdfViewerWidget.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_URDF_VIEWER_WIDGET_HXX
#define ROBOHERO_URDF_VIEWER_WIDGET_HXX

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPoint>
#include <array>
#include <string>

class UrdfViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit UrdfViewerWidget(QWidget *parent = nullptr);
    ~UrdfViewerWidget() override;

    void setJointAngles(const std::array<double, 17> &angles);
    void setJointPwm(const std::array<int, 17> &pwm);

    void setViewerConfig(bool showGrid, bool showAxes, bool showJointLabels,
                         const std::string &meshMode, double fov);
    void resetCamera();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    std::array<double, 17> _jointAngles;
    std::array<int, 17> _jointPwm;

    bool _showGrid;
    bool _showAxes;
    bool _showJointLabels;
    std::string _meshMode;
    double _fov;

    // Camera view parameters
    float _cameraDistance;
    float _cameraYaw;
    float _cameraPitch;
    float _panX;
    float _panY;
    QPoint _lastMousePos;

    void drawGrid();
    void drawAxes(float length);
    void drawRobot();
    void drawBox(float dx, float dy, float dz, float r, float g, float b);
    void drawUrdfBox(float sx, float sy, float sz,
                     float ox, float oy, float oz,
                     const float color[3]);

    void drawBaseLink();
    void drawHead();
    void drawLeftArm();
    void drawRightArm();
    void drawLeftLeg();
    void drawRightLeg();
};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
