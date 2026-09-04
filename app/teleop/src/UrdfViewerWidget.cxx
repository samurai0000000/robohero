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

// Color palette matching robohero.urdf materials
static const float COLOR_ARMOR_RED[3]    = {0.84f, 0.23f, 0.18f};
static const float COLOR_CHEST_GOLD[3]   = {0.86f, 0.68f, 0.48f};
static const float COLOR_SERVO_BLACK[3]  = {0.16f, 0.17f, 0.20f};
static const float COLOR_REACTOR_CYAN[3] = {0.18f, 0.82f, 1.00f};
static const float COLOR_LENS_DARK[3]    = {0.08f, 0.09f, 0.11f};
static const float COLOR_METAL_GRAY[3]   = {0.35f, 0.37f, 0.40f};

UrdfViewerWidget::UrdfViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent)
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
    _cameraDistance = 0.45f;
    _cameraYaw = 20.0f;
    _cameraPitch = 15.0f;
    _panX = 0.0f;
    _panY = 0.02f;
    update();
}

void UrdfViewerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Dark sci-fi background matching Three.js SceneManager (0x0a0f1d)
    glClearColor(0.039f, 0.059f, 0.114f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Key light (Directional)
    GLfloat lightPos[] = {0.6f, 1.2f, 0.8f, 1.0f};
    GLfloat lightAmbient[] = {0.35f, 0.35f, 0.40f, 1.0f};
    GLfloat lightDiffuse[] = {0.85f, 0.85f, 0.85f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    // Cyan fill light matching Three.js SceneManager
    GLfloat fillPos[] = {-0.8f, 0.5f, -0.6f, 1.0f};
    GLfloat fillDiffuse[] = {0.024f, 0.45f, 0.65f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiffuse);

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
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Camera transformation
    glTranslatef(_panX, _panY, -_cameraDistance);
    glRotatef(_cameraPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(_cameraYaw, 0.0f, 1.0f, 0.0f);

    if (_showGrid) {
        drawGrid();
    }

    if (_showAxes) {
        drawAxes(0.12f);
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

    // Floor sits right beneath the feet at Y = -0.180m
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
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);

    // Y: Green (Up)
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);

    // Z: Blue (Forward)
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
    // Front (+Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-hx, -hy,  hz);
    glVertex3f( hx, -hy,  hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f(-hx,  hy,  hz);
    // Back (-Z)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx,  hy, -hz);
    glVertex3f( hx,  hy, -hz);
    glVertex3f( hx, -hy, -hz);
    // Top (+Y)
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-hx,  hy, -hz);
    glVertex3f(-hx,  hy,  hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f( hx,  hy, -hz);
    // Bottom (-Y)
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f( hx, -hy, -hz);
    glVertex3f( hx, -hy,  hz);
    glVertex3f(-hx, -hy,  hz);
    // Right (+X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f( hx, -hy, -hz);
    glVertex3f( hx,  hy, -hz);
    glVertex3f( hx,  hy,  hz);
    glVertex3f( hx, -hy,  hz);
    // Left (-X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy,  hz);
    glVertex3f(-hx,  hy,  hz);
    glVertex3f(-hx,  hy, -hz);
    glEnd();
}

void UrdfViewerWidget::drawUrdfBox(float sx, float sy, float sz,
                                   float ox, float oy, float oz,
                                   const float color[3])
{
    glPushMatrix();
    glTranslatef(ox, oy, oz);
    drawBox(sx, sy, sz, color[0], color[1], color[2]);
    glPopMatrix();
}

void UrdfViewerWidget::drawBaseLink()
{
    // 1. Torso Base Link (armor_red)
    drawUrdfBox(0.055f, 0.076f, 0.080f, 0.0f, 0.0f, 0.0f, COLOR_ARMOR_RED);

    // 2. Gold Chest Armor Plate (chest_gold)
    drawUrdfBox(0.004f, 0.052f, 0.046f, 0.029f, 0.0f, 0.008f, COLOR_CHEST_GOLD);

    // 3. Arc Reactor Core (glowing cyan)
    drawUrdfBox(0.003f, 0.020f, 0.020f, 0.032f, 0.0f, 0.012f, COLOR_REACTOR_CYAN);
}

void UrdfViewerWidget::drawHead()
{
    // Joint: head_yaw_joint (channel 16)
    // origin xyz="0 0 0.046", axis xyz="0 0 1"
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.046f);
    glRotatef(static_cast<float>(_jointAngles[16] * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);

    // Red Helmet Dome
    drawUrdfBox(0.038f, 0.040f, 0.036f, 0.0f, 0.0f, 0.020f, COLOR_ARMOR_RED);

    // Gold Eye Bezel
    drawUrdfBox(0.003f, 0.024f, 0.024f, 0.020f, 0.0f, 0.022f, COLOR_CHEST_GOLD);

    // Dark Eye Lens
    drawUrdfBox(0.002f, 0.016f, 0.016f, 0.022f, 0.0f, 0.022f, COLOR_LENS_DARK);

    glPopMatrix();
}

void UrdfViewerWidget::drawLeftArm()
{
    glPushMatrix();
    // Joint: left_shoulder_pitch_joint (channel 5)
    // origin xyz="0 0.048 0.028", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.048f, 0.028f);
    glRotatef(static_cast<float>(_jointAngles[5] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // left_shoulder_pitch_link (servo bracket)
    drawUrdfBox(0.024f, 0.018f, 0.024f, 0.0f, 0.008f, 0.0f, COLOR_SERVO_BLACK);

    // Joint: left_shoulder_roll_joint (channel 6)
    // origin xyz="0 0.018 0", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.018f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[6] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // left_upper_arm_link (red armor)
    drawUrdfBox(0.024f, 0.024f, 0.044f, 0.0f, 0.0f, -0.022f, COLOR_ARMOR_RED);

    // Joint: left_elbow_joint (channel 7)
    // origin xyz="0 0 -0.044", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.0f, -0.044f);
    glRotatef(static_cast<float>(_jointAngles[7] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // left_forearm_link (red armor)
    drawUrdfBox(0.022f, 0.022f, 0.044f, 0.0f, 0.0f, -0.022f, COLOR_ARMOR_RED);

    // Left Hand (servo bracket)
    drawUrdfBox(0.016f, 0.020f, 0.014f, 0.0f, 0.0f, -0.048f, COLOR_SERVO_BLACK);

    glPopMatrix();
}

void UrdfViewerWidget::drawRightArm()
{
    glPushMatrix();
    // Joint: right_shoulder_pitch_joint (channel 10)
    // origin xyz="0 -0.048 0.028", axis xyz="0 1 0"
    glTranslatef(0.0f, -0.048f, 0.028f);
    glRotatef(static_cast<float>(_jointAngles[10] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // right_shoulder_pitch_link (servo bracket)
    drawUrdfBox(0.024f, 0.018f, 0.024f, 0.0f, -0.008f, 0.0f, COLOR_SERVO_BLACK);

    // Joint: right_shoulder_roll_joint (channel 9)
    // origin xyz="0 -0.018 0", axis xyz="1 0 0"
    glTranslatef(0.0f, -0.018f, 0.0f);
    glRotatef(static_cast<float>(_jointAngles[9] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // right_upper_arm_link (red armor)
    drawUrdfBox(0.024f, 0.024f, 0.044f, 0.0f, 0.0f, -0.022f, COLOR_ARMOR_RED);

    // Joint: right_elbow_joint (channel 8)
    // origin xyz="0 0 -0.044", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.0f, -0.044f);
    glRotatef(static_cast<float>(_jointAngles[8] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // right_forearm_link (red armor)
    drawUrdfBox(0.022f, 0.022f, 0.044f, 0.0f, 0.0f, -0.022f, COLOR_ARMOR_RED);

    // Right Hand (servo bracket)
    drawUrdfBox(0.016f, 0.020f, 0.014f, 0.0f, 0.0f, -0.048f, COLOR_SERVO_BLACK);

    glPopMatrix();
}

void UrdfViewerWidget::drawLeftLeg()
{
    glPushMatrix();
    // Joint: left_hip_roll_joint (channel 4)
    // origin xyz="0 0.028 -0.042", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.028f, -0.042f);
    glRotatef(static_cast<float>(_jointAngles[4] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // left_hip_roll_link (servo bracket)
    drawUrdfBox(0.028f, 0.026f, 0.020f, 0.0f, 0.0f, -0.009f, COLOR_SERVO_BLACK);

    // Joint: left_hip_pitch_joint (channel 3)
    // origin xyz="0 0 -0.018", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.018f);
    glRotatef(static_cast<float>(_jointAngles[3] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // left_thigh_link (servo bracket)
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, COLOR_SERVO_BLACK);

    // Joint: left_knee_pitch_joint (channel 2)
    // origin xyz="0 0 -0.048", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(_jointAngles[2] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // left_shin_link (red armor)
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, COLOR_ARMOR_RED);

    // Joint: left_ankle_pitch_joint (channel 1)
    // origin xyz="0 0 -0.048", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(_jointAngles[1] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // left_ankle_pitch_link (servo bracket)
    drawUrdfBox(0.028f, 0.026f, 0.018f, 0.0f, 0.0f, -0.008f, COLOR_SERVO_BLACK);

    // Joint: left_ankle_roll_joint (channel 0)
    // origin xyz="0 0 -0.016", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.0f, -0.016f);
    glRotatef(static_cast<float>(_jointAngles[0] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // left_foot_link (red armor foot plate)
    drawUrdfBox(0.076f, 0.048f, 0.008f, 0.010f, 0.0f, -0.004f, COLOR_ARMOR_RED);

    glPopMatrix();
}

void UrdfViewerWidget::drawRightLeg()
{
    glPushMatrix();
    // Joint: right_hip_roll_joint (channel 11)
    // origin xyz="0 -0.028 -0.042", axis xyz="1 0 0"
    glTranslatef(0.0f, -0.028f, -0.042f);
    glRotatef(static_cast<float>(_jointAngles[11] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // right_hip_roll_link (servo bracket)
    drawUrdfBox(0.028f, 0.026f, 0.020f, 0.0f, 0.0f, -0.009f, COLOR_SERVO_BLACK);

    // Joint: right_hip_pitch_joint (channel 12)
    // origin xyz="0 0 -0.018", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.018f);
    glRotatef(static_cast<float>(_jointAngles[12] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // right_thigh_link (servo bracket)
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, COLOR_SERVO_BLACK);

    // Joint: right_knee_pitch_joint (channel 13)
    // origin xyz="0 0 -0.048", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(_jointAngles[13] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // right_shin_link (red armor)
    drawUrdfBox(0.026f, 0.028f, 0.048f, 0.0f, 0.0f, -0.024f, COLOR_ARMOR_RED);

    // Joint: right_ankle_pitch_joint (channel 14)
    // origin xyz="0 0 -0.048", axis xyz="0 1 0"
    glTranslatef(0.0f, 0.0f, -0.048f);
    glRotatef(static_cast<float>(_jointAngles[14] * 180.0 / M_PI), 0.0f, 1.0f, 0.0f);

    // right_ankle_pitch_link (servo bracket)
    drawUrdfBox(0.028f, 0.026f, 0.018f, 0.0f, 0.0f, -0.008f, COLOR_SERVO_BLACK);

    // Joint: right_ankle_roll_joint (channel 15)
    // origin xyz="0 0 -0.016", axis xyz="1 0 0"
    glTranslatef(0.0f, 0.0f, -0.016f);
    glRotatef(static_cast<float>(_jointAngles[15] * 180.0 / M_PI), 1.0f, 0.0f, 0.0f);

    // right_foot_link (red armor foot plate)
    drawUrdfBox(0.076f, 0.048f, 0.008f, 0.010f, 0.0f, -0.004f, COLOR_ARMOR_RED);

    glPopMatrix();
}

void UrdfViewerWidget::drawRobot()
{
    glPushMatrix();

    // Map URDF standard coordinate frame (+X forward, +Y left, +Z up)
    // into the OpenGL camera space (+X right, +Y up, +Z facing viewer)
    // using pure orthogonal rotation:
    // X_gl = Y_urdf, Y_gl = Z_urdf, Z_gl = X_urdf
    const GLfloat urdfToGl[16] = {
        0.0f, 0.0f, 1.0f, 0.0f,  // 1st column: maps (1,0,0) to (0,0,1)
        1.0f, 0.0f, 0.0f, 0.0f,  // 2nd column: maps (0,1,0) to (1,0,0)
        0.0f, 1.0f, 0.0f, 0.0f,  // 3rd column: maps (0,0,1) to (0,1,0)
        0.0f, 0.0f, 0.0f, 1.0f   // 4th column: translation
    };
    glMultMatrixf(urdfToGl);

    drawBaseLink();
    drawHead();
    drawLeftArm();
    drawRightArm();
    drawLeftLeg();
    drawRightLeg();

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
