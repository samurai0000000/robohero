# RoboHero Teleoperation Application (`robohero_teleop`)

A cross-platform desktop application (Windows primary, Linux portable) for real-time humanoid robot teleoperation. The operator's movements are captured via webcam, extracted into pose landmarks using YOLOv8, mapped geometrically to RoboHero's 17 degrees of freedom, checked against URDF safety limits, and streamed over MQTT to the robot.

---

## System Architecture

```
                                      +------------------------------------+
                                      |   Webcam Video Stream (QThread)    |
                                      +-----------------+------------------+
                                                        |
                                                        v
                                      +------------------------------------+
                                      |  PoseEstimator (YOLOv8n-pose ONNX) |
                                      |  - Auto GPU: DirectML / CUDA / CPU |
                                      |  - In-memory embedded model (.rdata|
                                      +-----------------+------------------+
                                                        |
                                            17 COCO Keypoints (x, y, conf)
                                                        |
                                                        v
                                      +------------------------------------+
                                      |   PoseRetargeter (Eigen 3D Math)   |
                                      |  - Geometric Direct Angular Mapping|
                                      |  - Exponential Smoothing (alpha)   |
                                      |  - Velocity / Slew Rate Limiting   |
                                      +-----------------+------------------+
                                                        |
                                            Raw Joint Angles (radians)
                                                        |
                                                        v
                                      +------------------------------------+
                                      |     UrdfLimits (pugiXML Parser)    |
                                      |  - Compiled-in :/urdf/robohero.urdf|
                                      |  - Hard stop safety clamp [min,max]|
                                      |  - PWM mapping (SERVOMIN..SERVOMAX)|
                                      +--------+------------------+--------+
                                               |                  |
                       Safety-Clamped Angles   |                  | PCA9685 PWM Commands
                                               v                  v
                 +-------------------------------+     +-------------------------------+
                 |  UrdfViewerWidget (OpenGL 3D) |     |     MqttClient (mosquitto)    |
                 |  - Real-time posture preview  |     |  - Binary rh_msg_pwm stream   |
                 |  - Interactive orbit/pan/zoom |     |  - Auto-reconnect & watchdog  |
                 +-------------------------------+     +---------------+---------------+
                                                                       |
                                                                       v
                                                        +-----------------------------+
                                                        |   RoboHero Bipedal Robot    |
                                                        +-----------------------------+
```

---

## Key Design Decisions & Architectural Specifications

### 1. Embedded Assets (Zero-Disk Dependency)
- **Robot Kinematics (`urdf/robohero.urdf`)**: Compiled directly into the binary via Qt's Resource System (`:/urdf/robohero.urdf`). Non-selectable in UI to prevent kinematic desynchronization.
- **YOLOv8 Pose Model (`assets/yolov8n-pose.onnx`)**: Compiled directly into the binary (`:/models/yolov8n-pose.onnx`). Loaded directly from memory into `Ort::Session` using raw byte pointers without creating temporary files on disk.

### 2. Runtime Platform & Hardware Acceleration Detection
- At startup, `PoseEstimator` queries `Ort::GetAvailableProviders()`:
  - **Windows**: Attempts initialization with **DirectML** (`OrtSessionOptionsAppendExecutionProvider_DML`). On ASUS NUC workstations with Intel Iris Xe or Intel Arc graphics, tensor calculations are offloaded via DirectX 12 to the GPU, yielding 80–120+ FPS.
  - **Linux**: Attempts **CUDA** / **TensorRT** / **OpenVINO**, falling back gracefully to **CPU**.
  - **CPU Fallback**: Optimized with AVX2 vector extensions; runs comfortably at 45–60 FPS on modern Intel Core processors.

### 3. Motion Retargeting Strategy: Geometric (Direct Angular) Mapping
- **Flexion & Abduction**: Computed using vector dot-products and `atan2` on adjacent COCO keypoint triples (e.g. wrist-elbow-shoulder for elbow flexion; elbow-shoulder-hip for shoulder roll).
- **Sagittal Reaching**: Inferred from limb foreshortening ratios against calibrated neutral bone lengths.
- **Exponential Moving-Average Filter**:
  $$\theta_{\text{filtered}}^{(t)} = \alpha \theta_{\text{raw}}^{(t)} + (1 - \alpha) \theta_{\text{filtered}}^{(t-1)}$$
- **Deadband**: Angular changes below a configurable threshold (e.g. $1.0^\circ$) are suppressed when stationary to eliminate servo micro-jitter and prevent coil heating.

### 4. Servo Channel & URDF Joint Mapping

| Channel | Joint Name | COCO Keypoints Used | URDF Limit Range (rad) |
| :---: | :--- | :--- | :---: |
| 0 | `left_ankle_roll` | left_ankle -> left_knee (lateral) | [-0.4363, +1.3090] |
| 1 | `left_ankle_pitch` | left_ankle -> left_knee (sagittal) | [-0.7854, +1.5708] |
| 2 | `left_knee_pitch` | left_hip -> left_knee -> left_ankle | [-0.6981, +1.5708] |
| 3 | `left_hip_pitch` | left_shoulder -> left_hip -> left_knee | [-0.6981, +1.5708] |
| 4 | `left_hip_roll` | left_hip -> right_hip (lateral) | [-0.3491, +1.6057] |
| 5 | `left_shoulder_pitch` | left_elbow -> left_shoulder -> left_hip | [-1.8117, +1.3055] |
| 6 | `left_shoulder_roll` | left_wrist -> left_elbow -> left_shoulder | [0.0000, +4.7298] |
| 7 | `left_elbow` | left_shoulder -> left_elbow -> left_wrist | [-1.3090, +1.4835] |
| 8 | `right_elbow` | right_shoulder -> right_elbow -> right_wrist | [-1.4835, +0.9599] |
| 9 | `right_shoulder_roll` | right_wrist -> right_elbow -> right_shoulder | [-4.3284, 0.0000] |
| 10 | `right_shoulder_pitch`| right_elbow -> right_shoulder -> right_hip | [-2.4906, +2.0054] |
| 11 | `right_hip_roll` | right_hip -> left_hip (lateral) | [-1.7453, +0.3491] |
| 12 | `right_hip_pitch` | right_shoulder -> right_hip -> right_knee | [-0.6981, +1.5708] |
| 13 | `right_knee_pitch` | right_hip -> right_knee -> right_ankle | [-0.6981, +1.5708] |
| 14 | `right_ankle_pitch` | right_ankle -> right_knee (sagittal) | [-0.8727, +1.5708] |
| 15 | `right_ankle_roll` | right_ankle -> right_knee (lateral) | [-0.4363, +1.2217] |
| 16 | `head_yaw` | nose offset relative to ear midpoint | [-0.9756, +0.9233] |

**PWM Formula**:
$$\text{pos} = \text{SERVOMIN} + \frac{\theta - \text{lower}}{\text{upper} - \text{lower}} \cdot (\text{SERVOMAX} - \text{SERVOMIN})$$
where $\text{SERVOMIN} = 104$, $\text{SERVOMAX} = 512$ (PCA9685 12-bit counts).

---

## User Configuration & Settings Dialog

The application features a tabbed `SettingsDialog` (`Ctrl+,`) that persists preferences into `teleop.cfg` using `libconfig`:

1. **Camera & Video**: Device index, resolution (640x480 / 720p / 1080p), target FPS, selfie mirror mode (horizontal flip).
2. **AI & Detection**: Model source (Embedded Default vs Custom File), provider override (`Auto`, `DirectML`, `CUDA`, `CPU`), confidence thresholds, operator selection mode.
3. **Motion Retargeting**: Exponential smoothing factor $\alpha$, deadband angle threshold, joint slew rate limit, teleop mode (`Upper Body Only` vs `Full Body`), stance presets.
4. **Robot Safety**: Safety margin backoff angle from URDF limits, command TX rate limit ($30\text{ Hz}$), watchdog timeout ($500\text{ ms}$).
5. **MQTT Network**: Broker host/port, robot ID prefix, keepalive, auto-connect on startup (default: `true`).
6. **3D URDF View**: Visual mesh rendering toggle, joint frame axes, ground plane grid, dark viewport theme.

---

## Directory Structure

```
app/teleop/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── doc/
│   └── README.md
├── assets/
│   ├── yolov8n-pose.onnx
│   ├── robohero_icon.png
│   └── robohero_head.png
├── src/
│   ├── main.cxx
│   ├── MainWindow.cxx / MainWindow.hxx
│   ├── SettingsDialog.cxx / SettingsDialog.hxx
│   ├── CameraThread.cxx / CameraThread.hxx
│   ├── PoseEstimator.cxx / PoseEstimator.hxx
│   ├── PoseRetargeter.cxx / PoseRetargeter.hxx
│   ├── UrdfLimits.cxx / UrdfLimits.hxx
│   ├── UrdfViewerWidget.cxx / UrdfViewerWidget.hxx
│   ├── MqttClient.cxx / MqttClient.hxx
│   └── TeleopConfig.cxx / TeleopConfig.hxx
└── ui/
    ├── MainWindow.ui
    └── SettingsDialog.ui
```

> **Note on Assets**: The application icons (`robohero_icon.png` and `robohero_head.png`) are derived directly from the official RoboHero Android assets (`app/android/app/src/main/res/mipmap-xxxhdpi/ic_launcher*.png`), capturing the authentic design: red camera-eye head, gold chest armor with illuminated glowing cyan arc reactor, orange robotic arms, and black servo brackets.

---

## Build Instructions

### Windows

See [Compile-on-Windows.md](Compile-on-Windows.md) for prerequisites and
first-time source build steps (VS 2026, official Qt/OpenCV prebuilts).

### Linux (Ubuntu/Debian)
```bash
sudo apt install qt6-base-dev qt6-opengl-dev libopencv-dev \
    libmosquitto-dev libconfig++-dev cmake ninja-build

git submodule update --init --recursive
cd app/teleop
cmake --preset linux-release
cmake --build build/linux-release
```
