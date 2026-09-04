/*
 * UrdfViewerWidget.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "UrdfViewerWidget.hxx"
#include <QMouseEvent>
#include <QWheelEvent>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <iostream>

UrdfViewerWidget::UrdfViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , _showGrid(true)
    , _showAxes(true)
    , _showJointLabels(false)
    , _meshMode("boxes")
    , _fov(45.0)
    , _cameraDistance(0.48f)
    , _cameraYaw(25.0f)
    , _cameraPitch(20.0f)
    , _panX(0.0f)
    , _panY(0.04f)
{
    _jointAngles.fill(0.0);
    _jointPwm.fill(1500);
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
    _cameraDistance = 0.48f;
    _cameraYaw = 25.0f;
    _cameraPitch = 20.0f;
    _panX = 0.0f;
    _panY = 0.04f;
    update();
}

void UrdfViewerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPos[] = {1.0f, 2.0f, 2.0f, 1.0f};
    GLfloat lightAmbient[] = {0.3f, 0.3f, 0.35f, 1.0f};
    GLfloat lightDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void UrdfViewerWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (h > 0) ? (static_cast<float>(w) / h) : 1.0f;
    gluPerspective(_fov, aspect, 0.05, 10.0);

    glMatrixMode(GL_MODELVIEW);
}

void UrdfViewerWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera transformation
    glTranslatef(_panX, _panY, -_cameraDistance);
    glRotatef(_cameraPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(_cameraYaw, 0.0f, 1.0f, 0.0f);

    if (_showGrid) {
        drawGrid();
    }

    if (_showAxes) {
        drawAxes(0.15f);
    }

    drawRobot();
}

void UrdfViewerWidget::mousePressEvent(QMouseEvent *event)
{
    _lastMousePos = event->pos();
}

void UrdfViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - _lastMousePos.x();
    int dy = event->y() - _lastMousePos.y();

    if (event->buttons() & Qt::LeftButton) {
        _cameraYaw += dx * 0.5f;
        _cameraPitch += dy * 0.5f;
        if (_cameraPitch > 89.0f) _cameraPitch = 89.0f;
        if (_cameraPitch < -89.0f) _cameraPitch = -89.0f;
        update();
    } else if (event->buttons() & Qt::RightButton || event->buttons() & Qt::MiddleButton) {
        _panX += dx * 0.001f * _cameraDistance;
        _panY -= dy * 0.001f * _cameraDistance;
        update();
    }

    _lastMousePos = event->pos();
}

void UrdfViewerWidget::wheelEvent(QWheelEvent *event)
{
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    _cameraDistance -= numSteps * 0.05f;
    if (_cameraDistance < 0.1f) _cameraDistance = 0.1f;
    if (_cameraDistance > 3.0f) _cameraDistance = 3.0f;
    update();
}

void UrdfViewerWidget::drawGrid()
{
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor4f(0.25f, 0.25f, 0.30f, 0.5f);

    const float size = 0.4f;
    const float step = 0.04f;

    for (float i = -size; i <= size + 1e-4f; i += step) {
        glVertex3f(i, -0.22f, -size);
        glVertex3f(i, -0.22f, size);
        glVertex3f(-size, -0.22f, i);
        glVertex3f(size, -0.22f, i);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void UrdfViewerWidget::drawAxes(float length)
{
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glBegin(GL_LINES);

    // X: Red
    glColor3f(1.0f, 0.2f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);

    // Y: Green (Up)
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);

    // Z: Blue
    glColor3f(0.2f, 0.5f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, length);

    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

void UrdfViewerWidget::drawBox(float dx, float dy, float dz, float r, float g, float b)
{
    glColor3f(r, g, b);
    float hx = dx * 0.5f;
    float hy = dy * 0.5f;
    float hz = dz * 0.5f;

    glBegin(GL_QUADS);
    // Front
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-hx, -hy,  hz);
    glVertex3f( hx, -hy,  hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f(-hx,  hy,  hz);
    // Back
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx,  hy, -hz);
    glVertex3f( hx,  hy, -hz);
    glVertex3f( hx, -hx, -hz);
    // Top
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-hx,  hy, -hz);
    glVertex3f(-hx,  hy,  hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f( hx,  hy, -hz);
    // Bottom
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f( hx, -hy, -hz);
    glVertex3f( hx, -hy,  hz);
    glVertex3f(-hx, -hy,  hz);
    // Right
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f( hx, -hy, -hz);
    glVertex3f( hx,  hy, -hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f( hx, -hy,  hz);
    // Left
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy,  hz);
    glVertex3f(-hx,  hy,  hz);
    glVertex3f(-hx,  hy, -hz);
    glEnd();
}

void UrdfViewerWidget::drawRobot()
{
    glPushMatrix();

    // 1. Torso / Pelvis
    drawBox(0.09f, 0.11f, 0.05f, 0.35f, 0.55f, 0.85f); // RoboHero blue torso

    // 2. Head
    glPushMatrix();
    glTranslatef(0.0f, 0.08f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[16] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f); // head_yaw
    drawBox(0.06f, 0.05f, 0.05f, 0.95f, 0.85f, 0.2f); // RoboHero yellow head
    glPopMatrix();

    // 3. Left Arm
    glPushMatrix();
    glTranslatef(0.065f, 0.045f, 0.0f);
    // Shoulder Roll & Pitch
    glRotatef(static_cast<float>(_jointAngles[6] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
    glRotatef(static_cast<float>(_jointAngles[5] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.025f, 0.06f, 0.025f, 0.3f, 0.7f, 0.4f); // Upper arm

    // Elbow
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[7] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.03f, 0.0f);
    drawBox(0.022f, 0.055f, 0.022f, 0.3f, 0.8f, 0.4f); // Forearm
    glPopMatrix();

    // 4. Right Arm
    glPushMatrix();
    glTranslatef(-0.065f, 0.045f, 0.0f);
    // Shoulder Roll & Pitch
    glRotatef(static_cast<float>(_jointAngles[9] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
    glRotatef(static_cast<float>(_jointAngles[10] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.025f, 0.06f, 0.025f, 0.8f, 0.4f, 0.3f); // Upper arm

    // Elbow
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[8] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.03f, 0.0f);
    drawBox(0.022f, 0.055f, 0.022f, 0.9f, 0.4f, 0.3f); // Forearm
    glPopMatrix();

    // 5. Left Leg
    glPushMatrix();
    glTranslatef(0.035f, -0.06f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[4] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f); // Hip roll
    glRotatef(static_cast<float>(_jointAngles[3] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Hip pitch
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.03f, 0.065f, 0.03f, 0.4f, 0.6f, 0.8f); // Thigh

    // Knee
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[2] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Knee pitch
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.028f, 0.065f, 0.028f, 0.4f, 0.7f, 0.9f); // Shin

    // Ankle & Foot
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[1] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Ankle pitch
    glRotatef(static_cast<float>(_jointAngles[0] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f); // Ankle roll
    glTranslatef(0.0f, -0.01f, 0.015f);
    drawBox(0.04f, 0.015f, 0.07f, 0.3f, 0.3f, 0.35f); // Foot
    glPopMatrix();

    // 6. Right Leg
    glPushMatrix();
    glTranslatef(-0.035f, -0.06f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[11] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f); // Hip roll
    glRotatef(static_cast<float>(_jointAngles[12] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Hip pitch
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.03f, 0.065f, 0.03f, 0.4f, 0.6f, 0.8f); // Thigh

    // Knee
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[13] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Knee pitch
    glTranslatef(0.0f, -0.035f, 0.0f);
    drawBox(0.028f, 0.065f, 0.028f, 0.4f, 0.7f, 0.9f); // Shin

    // Ankle & Foot
    glTranslatef(0.0f, -0.035f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[14] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f); // Ankle pitch
    glRotatef(static_cast<float>(_jointAngles[15] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f); // Ankle roll
    glTranslatef(0.0f, -0.01f, 0.015f);
    drawBox(0.04f, 0.015f, 0.07f, 0.3f, 0.3f, 0.35f); // Foot
    glPopMatrix();

    glPopMatrix();
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
