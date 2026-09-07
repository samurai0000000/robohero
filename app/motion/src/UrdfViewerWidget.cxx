/*
 * UrdfViewerWidget.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "UrdfViewerWidget.hxx"
#include <QMouseEvent>
#include <QWheelEvent>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Color palette matching robohero.urdf materials
static const float COLOR_ARMOR_RED[3]    = {0.84f, 0.23f, 0.18f};
static const float COLOR_CHEST_GOLD[3]   = {0.86f, 0.68f, 0.48f};
static const float COLOR_SERVO_BLACK[3]  = {0.16f, 0.17f, 0.20f};
static const float COLOR_REACTOR_CYAN[3] = {0.18f, 0.82f, 1.00f};
static const float COLOR_LENS_DARK[3]    = {0.08f, 0.09f, 0.11f};
static const float COLOR_GHOST_CYAN[3]   = {0.20f, 0.85f, 1.00f};
static const float COLOR_GHOST_GOLD[3]   = {0.90f, 0.80f, 0.40f};

UrdfViewerWidget::UrdfViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , _hasTelemetry(false)
    , _showGhost(true)
    , _showCoM(true)
    , _showSupportPolygon(true)
    , _treadmillMode(true)
    , _showGrid(true)
    , _showAxes(true)
    , _showJointLabels(false)
    , _meshMode("boxes")
    , _fov(45.0)
    , _cameraDistance(0.45f)
    , _cameraYaw(20.0f)
    , _cameraPitch(15.0f)
    , _panX(0.0f)
    , _panY(0.02f)
{
    _jointAngles.fill(0.0);
    _jointPwm.fill(135);
    _telemetryAngles.fill(0.0);
    _telemetryPwm.fill(135);
    _telemetryValid.fill(false);
}

UrdfViewerWidget::~UrdfViewerWidget()
{
}

void UrdfViewerWidget::setJointAngles(const std::array<double, 17> &angles)
{
    _jointAngles = angles;
    update();
}

void UrdfViewerWidget::setJointPwm(const std::array<int, 17> &pwm)
{
    _jointPwm = pwm;
}

void UrdfViewerWidget::setTelemetryJointAngles(const std::array<double, 17> &angles,
                                              const std::array<bool, 17> &validMask)
{
    _telemetryAngles = angles;
    _telemetryValid = validMask;
    _hasTelemetry = true;
    update();
}

void UrdfViewerWidget::setTelemetryJointPwm(const std::array<int, 17> &pwm,
                                           const std::array<bool, 17> &validMask)
{
    _telemetryPwm = pwm;
    _telemetryValid = validMask;
    _hasTelemetry = true;
    update();
}

void UrdfViewerWidget::setShowGhost(bool show)
{
    _showGhost = show;
    update();
}

void UrdfViewerWidget::setShowCoM(bool show)
{
    _showCoM = show;
    update();
}

void UrdfViewerWidget::setShowSupportPolygon(bool show)
{
    _showSupportPolygon = show;
    update();
}

void UrdfViewerWidget::setTreadmillMode(bool treadmill)
{
    _treadmillMode = treadmill;
    update();
}

void UrdfViewerWidget::setViewerConfig(bool showGrid, bool showAxes,
                                       bool showJointLabels,
                                       const std::string &meshMode,
                                       double fov)
{
    _showGrid = showGrid;
    _showAxes = showAxes;
    _showJointLabels = showJointLabels;
    _meshMode = meshMode;
    _fov = fov;
    update();
}

void UrdfViewerWidget::resetCamera()
{
    _cameraDistance = 0.45f;
    _cameraYaw = 20.0f;
    _cameraPitch = 15.0f;
    _panX = 0.0f;
    _panY = 0.02f;
    emit cameraChanged(_cameraYaw, _cameraPitch, _cameraDistance, _panX, _panY);
    update();
}

void UrdfViewerWidget::setViewPreset(const QString &preset)
{
    if (preset == "persp") {
        _cameraDistance = 0.45f;
        _cameraYaw = 20.0f;
        _cameraPitch = 15.0f;
    } else if (preset == "front") {
        _cameraDistance = 0.45f;
        _cameraYaw = 0.0f;
        _cameraPitch = 0.0f;
    } else if (preset == "side") {
        _cameraDistance = 0.45f;
        _cameraYaw = -90.0f;
        _cameraPitch = 0.0f;
    } else if (preset == "top") {
        _cameraDistance = 0.45f;
        _cameraYaw = 0.0f;
        _cameraPitch = 89.9f;
    } else if (preset == "back") {
        _cameraDistance = 0.45f;
        _cameraYaw = 180.0f;
        _cameraPitch = 0.0f;
    }
    _panX = 0.0f;
    _panY = 0.02f;
    emit cameraChanged(_cameraYaw, _cameraPitch, _cameraDistance, _panX, _panY);
    update();
}

void UrdfViewerWidget::syncCamera(float yaw, float pitch, float distance,
                                 float panX, float panY)
{
    _cameraYaw = yaw;
    _cameraPitch = pitch;
    _cameraDistance = distance;
    _panX = panX;
    _panY = panY;
    update();
}

void UrdfViewerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.039f, 0.059f, 0.114f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPos[] = {0.6f, 1.2f, 0.8f, 1.0f};
    GLfloat lightAmbient[] = {0.35f, 0.35f, 0.40f, 1.0f};
    GLfloat lightDiffuse[] = {0.85f, 0.85f, 0.85f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    GLfloat fillPos[] = {-0.6f, -0.8f, -0.4f, 1.0f};
    GLfloat fillDiffuse[] = {0.25f, 0.30f, 0.40f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiffuse);

    glShadeModel(GL_SMOOTH);
}

void UrdfViewerWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double aspect = (h > 0) ? (static_cast<double>(w) / static_cast<double>(h)) : 1.0;
    gluPerspective(_fov, aspect, 0.01, 10.0);
    glMatrixMode(GL_MODELVIEW);
}

void UrdfViewerWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Camera transformation matching app/teleop
    glTranslatef(_panX, _panY, -_cameraDistance);
    glRotatef(_cameraPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(_cameraYaw, 0.0f, 1.0f, 0.0f);

    if (_showGrid) {
        drawGrid();
    }

    if (_showAxes) {
        drawAxes(0.12f);
    }

    // 1. Draw Target Planned Robot (Solid)
    drawRobot(_jointAngles, false);

    // 2. Draw Telemetry Echo Robot (Ghost Overlay)
    if (_showGhost && _hasTelemetry) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRobot(_telemetryAngles, true);
        glDisable(GL_BLEND);
    }

    // 3. Balance Indicators (CoM and Support Polygon)
    if (_showCoM || _showSupportPolygon) {
        drawBalanceIndicators(_jointAngles);
    }
}

void UrdfViewerWidget::mousePressEvent(QMouseEvent *event)
{
    _lastMousePos = event->pos();
}

void UrdfViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - _lastMousePos.x();
    int dy = event->y() - _lastMousePos.y();
    _lastMousePos = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        _cameraYaw += dx * 0.5f;
        _cameraPitch += dy * 0.5f;
        if (_cameraPitch > 89.0f) _cameraPitch = 89.0f;
        if (_cameraPitch < -89.0f) _cameraPitch = -89.0f;
        emit cameraChanged(_cameraYaw, _cameraPitch, _cameraDistance, _panX, _panY);
        update();
    } else if (event->buttons() & Qt::RightButton || event->buttons() & Qt::MiddleButton) {
        _panX += dx * 0.001f * _cameraDistance;
        _panY -= dy * 0.001f * _cameraDistance;
        emit cameraChanged(_cameraYaw, _cameraPitch, _cameraDistance, _panX, _panY);
        update();
    }
}

void UrdfViewerWidget::wheelEvent(QWheelEvent *event)
{
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    _cameraDistance -= numSteps * 0.05f;
    if (_cameraDistance < 0.1f) _cameraDistance = 0.1f;
    if (_cameraDistance > 3.0f) _cameraDistance = 3.0f;
    emit cameraChanged(_cameraYaw, _cameraPitch, _cameraDistance, _panX, _panY);
    update();
}

void UrdfViewerWidget::drawGrid()
{
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);

    // Floor sits right beneath the feet at Y = -0.180m (matching app/teleop)
    const float yFloor = -0.180f;
    const float size = 0.40f;
    const float step = 0.04f;

    for (float i = -size; i <= size + 1e-4f; i += step) {
        bool isCenter = std::abs(i) < 1e-3f;
        if (isCenter) {
            glColor4f(0.024f, 0.714f, 0.831f, 0.85f); // Cyan accent (0x06b6d4)
        } else {
            glColor4f(0.12f, 0.16f, 0.23f, 0.55f);     // Slate grid (0x1e293b)
        }

        glVertex3f(i, yFloor, -size);
        glVertex3f(i, yFloor, size);
        glVertex3f(-size, yFloor, i);
        glVertex3f(size, yFloor, i);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void UrdfViewerWidget::drawAxes(float length)
{
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);

    // X: Red (Right)
    glColor3f(1.0f, 0.2f, 0.2f);
    glVertex3f(0.0f, -0.180f, 0.0f);
    glVertex3f(length, -0.180f, 0.0f);

    // Y: Green (Up)
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(0.0f, -0.180f, 0.0f);
    glVertex3f(0.0f, -0.180f + length, 0.0f);

    // Z: Blue (Forward)
    glColor3f(0.2f, 0.5f, 1.0f);
    glVertex3f(0.0f, -0.180f, 0.0f);
    glVertex3f(0.0f, -0.180f, length);

    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

void UrdfViewerWidget::drawBox(float dx, float dy, float dz,
                               float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    float hx = dx * 0.5f;
    float hy = dy * 0.5f;
    float hz = dz * 0.5f;

    glBegin(GL_QUADS);
    // Front (+X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(hx, -hy, -hz); glVertex3f(hx,  hy, -hz);
    glVertex3f(hx,  hy,  hz); glVertex3f(hx, -hy,  hz);
    // Back (-X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-hx,  hy, -hz); glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy,  hz); glVertex3f(-hx,  hy,  hz);
    // Right (+Y)
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f( hx, hy, -hz); glVertex3f(-hx, hy, -hz);
    glVertex3f(-hx, hy,  hz); glVertex3f( hx, hy,  hz);
    // Left (-Y)
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-hx, -hy, -hz); glVertex3f( hx, -hy, -hz);
    glVertex3f( hx, -hy,  hz); glVertex3f(-hx, -hy,  hz);
    // Top (+Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-hx, -hy, hz); glVertex3f( hx, -hy, hz);
    glVertex3f( hx,  hy, hz); glVertex3f(-hx,  hy, hz);
    // Bottom (-Z)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-hx,  hy, -hz); glVertex3f( hx,  hy, -hz);
    glVertex3f( hx, -hy, -hz); glVertex3f(-hx, -hy, -hz);
    glEnd();
}

void UrdfViewerWidget::drawUrdfBox(float sx, float sy, float sz,
                                   float ox, float oy, float oz,
                                   const float color[3], float alpha)
{
    glPushMatrix();
    glTranslatef(ox, oy, oz);
    drawBox(sx, sy, sz, color[0], color[1], color[2], alpha);
    glPopMatrix();
}

void UrdfViewerWidget::drawRobot(const std::array<double, 17> &angles, bool isGhost)
{
    glPushMatrix();

    // Map URDF standard coordinate frame (+X forward, +Y left, +Z up)
    // into the OpenGL camera space (+X right, +Y up, +Z facing viewer)
    // using pure orthogonal rotation matching app/teleop:
    // X_gl = Y_urdf, Y_gl = Z_urdf, Z_gl = X_urdf
    const GLfloat urdfToGl[16] = {
        0.0f, 0.0f, 1.0f, 0.0f,  // 1st column: maps (1,0,0) to (0,0,1)
        1.0f, 0.0f, 0.0f, 0.0f,  // 2nd column: maps (0,1,0) to (1,0,0)
        0.0f, 1.0f, 0.0f, 0.0f,  // 3rd column: maps (0,0,1) to (0,1,0)
        0.0f, 0.0f, 0.0f, 1.0f   // 4th column: translation
    };
    glMultMatrixf(urdfToGl);

    drawBaseLink(isGhost);
    drawHead(angles, isGhost);
    drawLeftArm(angles, isGhost);
    drawRightArm(angles, isGhost);
    drawLeftLeg(angles, isGhost);
    drawRightLeg(angles, isGhost);

    glPopMatrix();
}

void UrdfViewerWidget::drawBaseLink(bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *gold = isGhost ? COLOR_GHOST_GOLD : COLOR_CHEST_GOLD;
    const float *cyan = isGhost ? COLOR_GHOST_CYAN : COLOR_REACTOR_CYAN;
    float a = isGhost ? 0.40f : 1.0f;

    drawUrdfBox(0.054f, 0.076f, 0.084f, 0.0f, 0.0f, 0.0f, red, a);
    drawUrdfBox(0.004f, 0.052f, 0.046f, 0.029f, 0.0f, 0.008f, gold, a);
    drawUrdfBox(0.003f, 0.020f, 0.020f, 0.032f, 0.0f, 0.012f, cyan, a);
}

void UrdfViewerWidget::drawHead(const std::array<double, 17> &angles, bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *gold = isGhost ? COLOR_GHOST_GOLD : COLOR_CHEST_GOLD;
    const float *lens = isGhost ? COLOR_GHOST_CYAN : COLOR_LENS_DARK;
    float a = isGhost ? 0.40f : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.046f);
    glRotatef(static_cast<float>(angles[16] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);

    drawUrdfBox(0.038f, 0.040f, 0.036f, 0.0f, 0.0f, 0.020f, red, a);
    drawUrdfBox(0.003f, 0.024f, 0.024f, 0.020f, 0.0f, 0.022f, gold, a);
    drawUrdfBox(0.002f, 0.016f, 0.016f, 0.022f, 0.0f, 0.022f, lens, a);
    glPopMatrix();
}

void UrdfViewerWidget::drawLeftArm(const std::array<double, 17> &angles, bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *black = isGhost ? COLOR_GHOST_CYAN : COLOR_SERVO_BLACK;
    float a = isGhost ? 0.40f : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, 0.048f, 0.028f);
    glRotatef(static_cast<float>(angles[5] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.024f, 0.018f, 0.024f, 0.0f, 0.008f, 0.0f, black, a);

    glTranslatef(0.0f, 0.018f, 0.0f);
    glRotatef(static_cast<float>(angles[6] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.024f, 0.024f, 0.044f, 0.0f, 0.0f, -0.022f, red, a);

    glTranslatef(0.0f, 0.0f, -0.044f);
    glRotatef(static_cast<float>(angles[7] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.022f, 0.022f, 0.044f, 0.0f, 0.0f, -0.022f, red, a);
    drawUrdfBox(0.016f, 0.020f, 0.014f, 0.0f, 0.0f, -0.048f, black, a);
    glPopMatrix();
}

void UrdfViewerWidget::drawRightArm(const std::array<double, 17> &angles, bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *black = isGhost ? COLOR_GHOST_CYAN : COLOR_SERVO_BLACK;
    float a = isGhost ? 0.40f : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, -0.048f, 0.028f);
    glRotatef(static_cast<float>(angles[10] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.024f, 0.018f, 0.024f, 0.0f, -0.008f, 0.0f, black, a);

    glTranslatef(0.0f, -0.018f, 0.0f);
    glRotatef(static_cast<float>(angles[9] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.024f, 0.024f, 0.044f, 0.0f, 0.0f, -0.022f, red, a);

    glTranslatef(0.0f, 0.0f, -0.044f);
    glRotatef(static_cast<float>(angles[8] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.022f, 0.022f, 0.044f, 0.0f, 0.0f, -0.022f, red, a);
    drawUrdfBox(0.016f, 0.020f, 0.014f, 0.0f, 0.0f, -0.048f, black, a);
    glPopMatrix();
}

void UrdfViewerWidget::drawLeftLeg(const std::array<double, 17> &angles, bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *black = isGhost ? COLOR_GHOST_CYAN : COLOR_SERVO_BLACK;
    float a = isGhost ? 0.40f : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, 0.028f, -0.042f);
    glRotatef(static_cast<float>(angles[4] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.028f, 0.026f, 0.020f, 0.0f, 0.0f, -0.009f, black, a);

    glTranslatef(0.0f, 0.0f, -0.018f);
    glRotatef(static_cast<float>(angles[3] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, black, a);

    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(angles[2] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, red, a);

    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(angles[1] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.028f, 0.026f, 0.018f, 0.0f, 0.0f, -0.008f, black, a);

    glTranslatef(0.0f, 0.0f, -0.016f);
    glRotatef(static_cast<float>(angles[0] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.076f, 0.048f, 0.008f, 0.010f, 0.0f, -0.004f, red, a);
    glPopMatrix();
}

void UrdfViewerWidget::drawRightLeg(const std::array<double, 17> &angles, bool isGhost)
{
    const float *red = isGhost ? COLOR_GHOST_CYAN : COLOR_ARMOR_RED;
    const float *black = isGhost ? COLOR_GHOST_CYAN : COLOR_SERVO_BLACK;
    float a = isGhost ? 0.40f : 1.0f;

    glPushMatrix();
    glTranslatef(0.0f, -0.028f, -0.042f);
    glRotatef(static_cast<float>(angles[11] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.028f, 0.026f, 0.020f, 0.0f, 0.0f, -0.009f, black, a);

    glTranslatef(0.0f, 0.0f, -0.018f);
    glRotatef(static_cast<float>(angles[12] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, black, a);

    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(angles[13] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, red, a);

    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(angles[14] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);
    drawUrdfBox(0.028f, 0.026f, 0.018f, 0.0f, 0.0f, -0.008f, black, a);

    glTranslatef(0.0f, 0.0f, -0.016f);
    glRotatef(static_cast<float>(angles[15] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    drawUrdfBox(0.076f, 0.048f, 0.008f, 0.010f, 0.0f, -0.004f, red, a);
    glPopMatrix();
}

void UrdfViewerWidget::drawBalanceIndicators(const std::array<double, 17> &angles)
{
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);

    glPushMatrix();
    const GLfloat urdfToGl[16] = {
        0.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glMultMatrixf(urdfToGl);

    float groundZ = -0.180f;

    // Approximate Center of Mass (CoM) projection
    float comX = 0.0f;
    float comY = 0.0f;

    // Shift CoM laterally based on hip roll sway
    comY += static_cast<float>(angles[4] * 0.025 - angles[11] * 0.025);

    // Render CoM Projection Dot on Ground
    glColor4f(0.2f, 1.0f, 0.4f, 0.9f);
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    glVertex3f(comX, comY, groundZ + 0.002f);
    glEnd();

    // Vertical line dropping from torso to ground
    glBegin(GL_LINES);
    glColor4f(0.2f, 1.0f, 0.4f, 0.4f);
    glVertex3f(comX, comY, 0.0f);
    glVertex3f(comX, comY, groundZ);
    glEnd();

    // Render Foot Support Polygon outline
    if (_showSupportPolygon) {
        glColor4f(0.2f, 0.8f, 1.0f, 0.7f);
        glBegin(GL_LINE_LOOP);
        // Left foot boundary
        glVertex3f( 0.048f,  0.052f, groundZ + 0.001f);
        glVertex3f(-0.028f,  0.052f, groundZ + 0.001f);
        glVertex3f(-0.028f,  0.004f, groundZ + 0.001f);
        glVertex3f( 0.048f,  0.004f, groundZ + 0.001f);
        glEnd();

        glBegin(GL_LINE_LOOP);
        // Right foot boundary
        glVertex3f( 0.048f, -0.004f, groundZ + 0.001f);
        glVertex3f(-0.028f, -0.004f, groundZ + 0.001f);
        glVertex3f(-0.028f, -0.052f, groundZ + 0.001f);
        glVertex3f( 0.048f, -0.052f, groundZ + 0.001f);
        glEnd();
    }

    glPopMatrix();
    glEnable(GL_LIGHTING);
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
