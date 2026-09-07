/*
 * UrdfViewerWidget.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef ROBOHERO_MOTION_URDF_VIEWER_WIDGET_HXX
#define ROBOHERO_MOTION_URDF_VIEWER_WIDGET_HXX

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

    void setTelemetryJointAngles(const std::array<double, 17> &angles,
                                 const std::array<bool, 17> &validMask);
    void setTelemetryJointPwm(const std::array<int, 17> &pwm,
                              const std::array<bool, 17> &validMask);

    void setShowGhost(bool show);
    void setShowCoM(bool show);
    void setShowSupportPolygon(bool show);
    void setTreadmillMode(bool treadmill);

    void setViewerConfig(bool showGrid, bool showAxes, bool showJointLabels,
                         const std::string &meshMode, double fov);
    void resetCamera();
    void setViewPreset(const QString &preset); // "persp", "front", "side", "top"

signals:
    void cameraChanged(float yaw, float pitch, float distance, float panX, float panY);

public slots:
    void syncCamera(float yaw, float pitch, float distance, float panX, float panY);

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

    std::array<double, 17> _telemetryAngles;
    std::array<int, 17> _telemetryPwm;
    std::array<bool, 17> _telemetryValid;
    bool _hasTelemetry;

    bool _showGhost;
    bool _showCoM;
    bool _showSupportPolygon;
    bool _treadmillMode;

    bool _showGrid;
    bool _showAxes;
    bool _showJointLabels;
    std::string _meshMode;
    double _fov;

    float _cameraDistance;
    float _cameraYaw;
    float _cameraPitch;
    float _panX;
    float _panY;
    QPoint _lastMousePos;

    void drawGrid();
    void drawAxes(float length);
    void drawRobot(const std::array<double, 17> &angles, bool isGhost = false);
    void drawBox(float dx, float dy, float dz, float r, float g, float b, float a = 1.0f);
    void drawUrdfBox(float sx, float sy, float sz,
                     float ox, float oy, float oz,
                     const float color[3], float alpha = 1.0f);

    void drawBaseLink(bool isGhost);
    void drawHead(const std::array<double, 17> &angles, bool isGhost);
    void drawLeftArm(const std::array<double, 17> &angles, bool isGhost);
    void drawRightArm(const std::array<double, 17> &angles, bool isGhost);
    void drawLeftLeg(const std::array<double, 17> &angles, bool isGhost);
    void drawRightLeg(const std::array<double, 17> &angles, bool isGhost);

    void drawBalanceIndicators(const std::array<double, 17> &angles);
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
