# RoboHero Motion & Gait Studio (`robohero_motion`)

A standalone, cross-platform C++ desktop application built with **Qt** (Qt6 / Qt5) and **OpenGL** for designing, generating, visualizing, and streaming bipedal walking gaits and sequenced multi-joint motions for the **RoboHero** humanoid robot.

---

## 1. System Architecture

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              RoboHero Motion & Gait Studio                             │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│   ┌────────────────────────┐  ┌─────────────────────────┐  ┌────────────────────────┐  │
│   │ Parametric Gait Engine │  │ Keyframe Choreographer  │  │ Balance & CoM Engine   │  │
│   │ (Analytical 5-DOF IK)  │  │ (Cubic Splines / Dope)  │  │ (Support Polygon Check)│  │
│   └───────────┬────────────┘  └────────────┬────────────┘  └───────────┬────────────┘  │
│               │                            │                           │               │
│               └──────────────────────┬─────┴───────────────────────────┘               │
│                                      ▼                                                 │
│                    ┌───────────────────────────────────┐                               │
│                    │    UrdfLimits & Safety Clamping   │                               │
│                    │ (Compiled-in :/model/* resources) │                               │
│                    └─────────────────┬─────────────────┘                               │
│                                      │                                                 │
│                     Discrete 50 Hz Sampled Joint Trajectories                          │
│                                      │                                                 │
│               ┌──────────────────────┴──────────────────────┐                          │
│               ▼                                             ▼                          │
│   ┌───────────────────────┐                     ┌───────────────────────┐              │
│   │ UrdfViewerWidget (3D) │                     │ MqttClient (mosquitto)│              │
│   │ • OpenGL URDF Digital │                     │ • Binary RHB1 stream  │              │
│   │   Twin Visualizer     │                     │ • RH_MSG_SET_PWM      │              │
│   │ • CoM / Support Poly  │                     │ • Auto-reconnect      │              │
│   │ • Treadmill/World View│                     │ • Low-latency Wi-Fi   │              │
│   └───────────────────────┘                     └───────────┬───────────┘              │
│                                                             │                          │
└─────────────────────────────────────────────────────────────┼──────────────────────────┘
                                                              │ MQTT over Wi-Fi
                                                              ▼
                                                 ┌────────────────────────┐
                                                 │ RoboHero Humanoid Robot│
                                                 │ (ESP8266 + PCA9685)    │
                                                 └────────────────────────┘
```

---

## 2. Kinematic Structure & Model Architecture

### 2.1 Subproject Integration & Calibration Single Source of Truth (SOT)

Following the canonical architecture established in `doc/Model.md` and proven in `app/teleop`:

```
                       ┌──────────────────────────────┐
                       │ model/                       │
                       │  ├── robohero.urdf           │
                       │  └── calibration.json        │
                       └──────────────┬───────────────┘
                                      │
          ┌───────────────────────────┼───────────────────────────┐
          │ (Makefile Rule)           │ (Qt AUTORCC)              │ (Qt AUTORCC)
          ▼                           ▼                           ▼
   ┌──────────────┐            ┌──────────────┐            ┌──────────────┐
   │ app/web      │            │ app/teleop   │            │ app/motion   │
   │ public/model/│            │ :/model/*    │            │ :/model/*    │
   └──────────────┘            └──────────────┘            └──────────────┘
```

1. **Zero Hard-Coding**:
   * Channel indices, center PWMs, scale constants (`rad_per_pwm`), direction signs ($\pm 1.0$), and joint angle limits are **never hard-coded** in `app/motion` source files.
2. **Build-Time Dependency Tracking (`CMakeLists.txt`)**:
   * `motion_resources.qrc` packages `../../model/robohero.urdf` as `:/model/robohero.urdf` and `../../model/calibration.json` as `:/model/calibration.json`.
   * CMake declares strict rebuild dependency tracking:
     ```cmake
     set_property(SOURCE motion_resources.qrc PROPERTY OBJECT_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/../../model/robohero.urdf"
         "${CMAKE_CURRENT_SOURCE_DIR}/../../model/calibration.json"
     )
     ```
     Whenever `model/calibration.json` is updated by physical robot calibration, CMake automatically triggers `rcc` re-compilation, eliminating stale kinematics bugs.
3. **Runtime Derivation via `UrdfLimits`**:
   * On startup, `UrdfLimits::instance().init(":/model/robohero.urdf", ":/model/calibration.json")` parses `calibration.json` via Qt's `QJsonDocument` and validates the XML joints via `pugixml`.
   * Computes precise bidirectional conversion between radians and PCA9685 PWM counts:
     $$\text{pwm} = \text{round}\left(\text{center} + \frac{\theta}{\text{radPerPwm} \times \text{sign}}\right)$$
     $$\theta = (\text{pwm} - \text{center}) \times \text{radPerPwm} \times \text{sign}$$
4. **Safety Clamping with Configurable Margin**:
   * `UrdfLimits::clamp(channel, angleRad, safetyMarginDeg)` enforces strict physical stops before sending any target angle to the OpenGL renderer or MQTT streamer.
5. **Firmware Header Sharing**:
   * Shares `firmware/include` (`robohero/msg.h` and `robohero/protocol.h`) to ensure exact byte-level wire compatibility for `RH_MSG_SET_PWM` and `RH_TLV_SERVO`.
6. **Zero-Disk Runtime Dependency**:
   * The compiled `robohero_motion` binary has no external file path dependency on the robot model; it runs as a completely standalone executable on Windows or Linux with model assets embedded in `.rdata`.

### 2.2 Complete 17-Servo DOF Breakdown

RoboHero features 17 degrees of freedom organized into five limb groups:

| Limb Group | Servos | Hardware Channels | Kinematic Function |
| :--- | :---: | :---: | :--- |
| **Left Leg** | 5 | Ch 0: Ankle Roll<br>Ch 1: Ankle Pitch<br>Ch 2: Knee Pitch<br>Ch 3: Hip Pitch<br>Ch 4: Hip Roll | Sagittal stepping, knee flexion, and lateral weight transfer |
| **Right Leg** | 5 | Ch 11: Hip Roll<br>Ch 12: Hip Pitch<br>Ch 13: Knee Pitch<br>Ch 14: Ankle Pitch<br>Ch 15: Ankle Roll | Sagittal stepping, knee flexion, and lateral weight transfer |
| **Left Arm** | 3 | Ch 5: Shoulder Pitch<br>Ch 6: Shoulder Roll<br>Ch 7: Elbow | Sagittal swing, lateral raise, and forearm reaching/flexion |
| **Right Arm** | 3 | Ch 8: Elbow<br>Ch 9: Shoulder Roll<br>Ch 10: Shoulder Pitch | Forearm reaching/flexion, lateral raise, and sagittal swing |
| **Head** | 1 | Ch 16: Neck Yaw | Horizontal gaze tracking and anticipatory turning pan |

> [!NOTE]
> **No Hip Yaw Joint**:
> Unlike 6-DOF humanoid legs, RoboHero does not possess an explicit pelvis yaw servo. Turning is achieved through **lateral weight transfer** (shifting the Center of Mass over the stance foot using hip and ankle roll), unweighting the swing foot, and applying differential pitch/roll foot rotations.

### 2.3 Viewport Visualization Modes (Ghost Overlay & Split View)

The 3D URDF model viewer is **completely local-first and operates offline without requiring network or robot hardware**. When a robot is connected via MQTT, the viewer provides two dual-state visualization modes to monitor planned motion versus physical robot telemetry:

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    Dual Visualization Modes: Plan vs. Reality                   │
├────────────────────────────────────────┬────────────────────────────────────────┤
│ Mode 1: Ghost Overlay (Default)        │ Mode 2: Synchronized Split View        │
├────────────────────────────────────────┼────────────────────────────────────────┤
│ • Single unified 3D viewport           │ • Two side-by-side viewports           │
│ • Solid Model: Planned target motion   │ • Left Viewport: Planned target motion │
│ • Translucent Cyan Ghost: Live robot   │ • Right Viewport: Live robot telemetry │
│ • Instant visual deviation & lag check │ • Synchronized orbital camera rotation │
└────────────────────────────────────────┴────────────────────────────────────────┘
```

1. **Local-First Offline Rendering**:
   * Dragging the playhead or scrubbing evaluates splines at 50 Hz and directly calls `UrdfViewerWidget::setJointAngles()`.
   * Sliders and parametric generators animate the solid digital twin with zero latency.
2. **Mode 1: Telemetry Ghost / Shadow Overlay (Default)**:
   * The planned motion renders in full solid materials.
   * When connected to MQTT, incoming `RH_MSG_STATUS` packets render a **semi-transparent cyan ghost model** superimposed over the solid model in the exact same 3D coordinate space.
   * If the robot tracks the trajectory perfectly, the ghost and solid model overlap identically.
   * Any Wi-Fi packet latency, mechanical slew delay, or tracking discrepancy appears as an immediate visible divergence between the limbs.
3. **Mode 2: Synchronized Split-Screen View (`[ ◫ Split View ]`)**:
   * A toolbar button splits the center viewport into two synchronized side-by-side viewports:
     * **Left: Planned Trajectory** (Local offline generator / timeline)
     * **Right: Physical Robot Feedback** (Live telemetry from MQTT `0x01` status packets)
   * Camera orbiting, panning, and zooming are linked between both viewports.

---

## 3. Motion Generation Paradigms & Mathematics

### 3.1 Parametric Biped Walking Gait Generator (Analytical 5-DOF IK)

Rather than hand-animating complex leg movements frame-by-frame, the parametric gait generator synthesizes continuous walking, side-stepping, and turning gaits directly from mathematical equations:

#### 1. Analytical Closed-Form Leg Inverse Kinematics (IK)
Given a target foot position $(x_f, y_f, z_f)$ relative to the hip and a flat-foot orientation constraint ($\phi_{\text{roll}} = \phi_{\text{target}}, \theta_{\text{pitch}} = \theta_{\text{target}}$):
* **Ankle Offset & Lateral Decoupling**:
  $$x_a = x_f - (-H_{\text{ankle}} \sin\theta_{\text{pitch}}), \quad y_a = y_f - (-H_{\text{ankle}} \sin\phi_{\text{roll}}), \quad z_a = z_f - (-H_{\text{ankle}} \cos\theta_{\text{pitch}} \cos\phi_{\text{roll}})$$
* **Hip Roll & Ankle Roll** (axis $[1, 0, 0]$):
  $$\theta_{\text{hip\_roll}} = \text{atan2}(y_a, -z_a), \quad \theta_{\text{ankle\_roll}} = \phi_{\text{roll}} - \theta_{\text{hip\_roll}}$$
  Coupled laterally such that $\theta_{\text{ankle\_roll}} = -\theta_{\text{hip\_roll}}$ for horizontal soles, maintaining flat foot contact while the pelvis sways.
* **Leg Extension Distance in Sagittal Plane**:
  $$z' = -\sqrt{y_a^2 + z_a^2}, \quad x' = x_a, \quad D = \text{clamp}\left(\sqrt{x'^2 + z'^2}, |L_1 - L_2| + \epsilon, (L_1 + L_2) \cdot 0.999\right)$$
* **Knee Pitch Angle** (axis $[0, 1, 0]$, Law of Cosines):
  $$\cos\alpha = \frac{L_1^2 + L_2^2 - D^2}{2 L_1 L_2}, \quad \theta_{\text{knee}} = \max(0.0, \pi - \alpha)$$
  Enforces strictly non-negative knee flexion ($\theta_{\text{knee}} \ge 0$), eliminating unphysical knee hyper-extension (forward bending).
* **Sagittal Hip Pitch Angle** (axis $[0, 1, 0]$):
  $$\gamma = \text{atan2}(x', -z'), \quad \cos\beta = \frac{L_1^2 + D^2 - L_2^2}{2 L_1 D}, \quad \theta_{\text{hip}} = -(\gamma + \beta)$$
  Because the URDF joint axis is $[0, 1, 0]$, by the right-hand rule forward rotation ($+X$) requires a **negative angle** ($\theta_{\text{hip}} < 0$).
* **Closed-Loop Ankle Pitch Compensation**:
  $$\theta_{\text{ankle}} = \theta_{\text{pitch}} - \theta_{\text{hip}} - \theta_{\text{knee}}$$
  Guarantees the foot sole remains completely parallel to the ground during stance, and level in the air during swing.

#### 2. Cycloid Foot Trajectory Generation
* **Swing Phase ($0 \le t < T_{\text{swing}}$)**:
  $$z(t) = H_{\text{step}} \sin\left(\frac{\pi t}{T_{\text{swing}}}\right)$$
  $$x(t) = -\frac{L_{\text{stride}}}{2} + L_{\text{stride}} \left( \frac{t}{T_{\text{swing}}} - \frac{1}{2\pi}\sin\left(\frac{2\pi t}{T_{\text{swing}}}\right) \right)$$
* **Stance Phase ($T_{\text{swing}} \le t < T_{\text{cycle}}$)**:
  The foot remains planted firmly on the floor, translating backwards relative to the pelvis at constant speed $-v_{\text{stride}}$.

#### 3. Lateral Pelvis Sway (Weight Transfer)
To allow the swing foot to lift off the ground without tipping the robot, the pelvis shifts laterally over the stance foot:
$$y_{\text{pelvis}}(t) = A_{\text{sway}} \sin\left(\frac{2\pi t}{T_{\text{cycle}}}\right)$$

#### 4. Upper Body Dynamic Counter-Balancing
* **Shoulder Pitch (Ch 5 & 10)**: Left arm oscillates in anti-phase with the left leg (matching the right leg), canceling the vertical-axis body yaw angular momentum ($L_z = \sum (\mathbf{r}_i \times m_i \mathbf{v}_i)_z$).
* **Elbow Flexion (Ch 7 & 8)**: Adjustable arm tuck ($15^\circ$ to $40^\circ$) reduces arm rotational inertia during fast strides.
* **Shoulder Roll (Ch 6 & 9)**: Provides lateral clearance ($5^\circ$ to $10^\circ$) to avoid collisions with the hips during sway.
* **Head Yaw (Ch 16)**: Anticipatory gaze panning into the turning direction before foot rotation.

---

### 3.2 Upper Body Kinematics & Gesture Generation (Arms & Head)

For discrete gestures and choreographic tasks:
* **Analytical 3-DOF Arm IK**: Given a 3D hand target $(x_h, y_h, z_h)$, the app computes Shoulder Roll, Shoulder Pitch, and Elbow flexion.
* **Built-in Archetype Generators**:
  * **Waving**: Lateral shoulder raise ($90^\circ$), forward shoulder tilt ($20^\circ$), elbow sinusoidal oscillation ($1.5\,\text{Hz}$), and head gaze tracking.
  * **Bowing**: Forward hip/torso bending coupled with arms tucking respectfully straight and head nodding down.
  * **Clapping**: Bilateral symmetrical arm sweep bringing hands together in rhythm.
  * **Martial Arts / Guard**: Guard stance with elbows bent and fists raised while legs walk.

---

### 3.3 Interactive Multi-Track Keyframe Choreographer

* **Dope Sheet & Timeline Curve Editor**: 17 separate joint tracks with keyframes placed at arbitrary timestamps.
* **Continuous Spline Interpolation**: **Cubic Hermite Splines** and **Cubic Bezier Easing** ensure $C^1$ velocity continuity, preventing abrupt jerks that strip servo gears.
* **Mirror & Phase-Offset ($\pi$)**: One-click mirroring of left-side motion to the right side with inverted roll signs and half-period phase shift.
* **Onion-Skinning in 3D**: Semi-transparent ghost rendering of previous and upcoming poses to evaluate motion arcs.

---

### 3.4 Static & Dynamic Balance Verification (CoM & Support Polygon)

* **Center of Mass (CoM)**: Calculated dynamically using URDF link masses:
  $$\mathbf{r}_{\text{CoM}} = \frac{\sum m_i \mathbf{r}_i}{\sum m_i}$$
* **Support Polygon**:
  * Double-Support: Convex hull enclosing both foot soles.
  * Single-Support: Bounding box of the stance foot sole.
* **Real-Time Visual Stability Indicator**:
  * 🟢 **Green**: CoM is safely within the support polygon (statically stable).
  * 🟡 **Yellow**: CoM approaching polygon boundary (marginal stability).
  * 🔴 **Red**: CoM outside support polygon (unstable; warning alert triggered).

---

## 4. Input Parameters & Motion Targets

When starting with an empty library, the robot defaults to the **Canonical Standby Pose**. Users specify high-level target parameters:

### Locomotion Gait Targets

| Target Parameter | Units | Typical Range | Description / Purpose |
| :--- | :---: | :---: | :--- |
| **Locomotion Mode** | Enum | `Forward`, `Backward`, `Turn Left`, `Turn Right`, `Side-step` | High-level direction of travel |
| **Stride Length ($L_x$)** | mm | `-30` to `+50` mm | Step-level forward/backward swing displacement per step |
| **Step Height ($H_z$)** | mm | `10` to `35` mm | Peak ground clearance height of the swing foot |
| **Lateral Sway ($A_{\text{sway}}$)** | mm | `10` to `25` mm | Pelvis lateral shift to center CoM over stance foot |
| **Turn Angle per Step ($\Delta\theta$)** | degrees | `5°` to `15°` | In-place rotation angle achieved during unweighted phase |
| **Step Duration ($T_{\text{step}}$)** | ms | `400` to `1000` ms | Stepping cadence / speed |
| **Double-Support Duty Cycle** | % | `10%` to `30%` | Overlap phase when both feet are on the ground |
| **Torso Pitch Lean** | degrees | `0°` to `8°` | Forward tilt to counteract leg momentum and gravity |
| **Arm Swing Amplitude** | degrees | `0°` to `30°` | Counter-swing of shoulders to cancel body yaw inertia |
| **Arm Stance Mode** | Enum | `Natural Swing`, `Guard`, `Hands on Hips`, `Custom` | Upper body pose configuration during walking |

### Coordinate Frame Distinction: Local Step vs. Global World Odometry

* **Local Kinematic Frame (Pelvis / Foot Frame)**:
  $L_x$, $H_z$, and $A_{\text{sway}}$ represent relative foot-to-pelvis displacements used by Inverse Kinematics to compute servo angles.
* **Global World Displacement (Odometry / Path Result)**:
  Because the stance foot is pinned to the floor by friction, each local stride translates the entire robot across the world:
  $$\Delta X_{\text{global}} = N_{\text{steps}} \times L_x, \quad \Delta \Theta_{\text{global}} = N_{\text{steps}} \times \Delta\theta$$
  Global forward velocity: $v_{\text{global}} = \frac{L_x}{T_{\text{step}}}$. For example, $L_x = 40\,\text{mm}$ at $T_{\text{step}} = 0.5\,\text{s}$ yields $80\,\text{mm/s}$ ($4.8\,\text{m/min}$).
* **Viewport Viewing Modes**:
  1. **Treadmill Mode**: Robot remains centered at origin $(0, 0, 0)$ while the floor grid slides underneath (ideal for inspecting joint motion and clearance).
  2. **World Navigation Mode**: Robot physically traverses the 3D ground plane along its global path.

---

## 5. Motion File Format Specification (`.rhm.json`)

Motions are stored in the filesystem as structured JSON files:

```json
{
  "version": "1.0",
  "name": "Forward Walk Standard",
  "description": "5-DOF parametric walking gait with lateral sway and arm balance",
  "generator": {
    "type": "parametric_biped",
    "parameters": {
      "stride_length_mm": 40.0,
      "step_height_mm": 25.0,
      "sway_amplitude_mm": 18.0,
      "cycle_duration_ms": 1600,
      "torso_pitch_deg": 3.0,
      "arm_swing_deg": 20.0,
      "arm_mode": "natural_swing"
    }
  },
  "duration_ms": 1600,
  "loop": true,
  "fps": 50,
  "keyframes": [
    {
      "time_ms": 0,
      "easing": "cubic_in_out",
      "channels": {
        "0": 160, "1": 161, "2": 141, "3": 168, "4": 158,
        "5": 158, "6": 243, "7": 159, "8": 163, "9": 75,
        "10": 156, "11": 161, "12": 129, "13": 150, "14": 165,
        "15": 162, "16": 90
      }
    },
    {
      "time_ms": 400,
      "easing": "cubic_in_out",
      "channels": { ... }
    }
  ]
}
```

### 5.1 Filesystem Storage, Library Discovery & Persistence Across App Restarts

To ensure all motions persist across application restarts and can be managed seamlessly:

1. **Storage Directories**:
   * **Bundled Default Motions**: Located in `app/motion/motions/` (e.g., `walk_forward.rhm.json`, `walk_turn_left.rhm.json`, `bow.rhm.json`, `wave.rhm.json`).
   * **User Custom Motions**: Automatically saved to standard OS user application data directories:
     * **Linux**: `~/.local/share/robohero/motions/` (or `~/.config/robohero/motions/`)
     * **Windows**: `%APPDATA%\robohero\motions\`
   * **Custom Workspace Folder**: Users can select any custom folder via the Settings dialog; the path is stored in `QSettings` (`robohero_motion.ini` / Registry) and remembered across sessions.
2. **Library Scanner on Startup**:
   * On startup, `MotionLibrary` scans all configured motion directories for `.rhm.json` files.
   * Discovered motions populate the **Motion Library Browser** dock panel with:
     * Motion Name & Category (Locomotion, Gesture, Custom)
     * Total Duration & Step Count
     * Loop indicator (`🔁 Loopable`)
     * Instant action buttons: `[ Load ]` and `[ ▶ Play on Robot ]`
3. **Session Restoration**:
   * `QSettings` tracks the last active motion, timeline zoom, and playback settings so the user's workspace is automatically restored when the app restarts.

---

## 6. Communication Protocol & Robot Playback Execution

The application provides two complementary ways to execute motions on the robot:

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           Trajectory Playback Pipeline                          │
├───────────────────────────────────────┬─────────────────────────────────────────┤
│ 1. Virtual Digital Twin Preview       │ 2. Physical Robot Execution (MQTT)      │
├───────────────────────────────────────┼─────────────────────────────────────────┤
│ • UrdfViewerWidget (OpenGL)           │ • MotionPlayer (50 Hz / 20 ms QTimer)   │
│ • Sampled spline interpolation (50Hz) │ • Pack 17 channels -> RH_MSG_SET_PWM    │
│ • Real-time CoM & support polygon     │ • Low-latency MQTT publish over Wi-Fi   │
│ • Visual ground contact feedback      │ • Seamless loop & smooth Standby return │
└───────────────────────────────────────┴─────────────────────────────────────────┘
```

### 6.1 Real-Time Robot Playback Engine (`MotionPlayer`)

For any motion found on the filesystem, clicking **"Play on Robot"** initiates real-time trajectory streaming:

1. **High-Resolution Sampling Timer (50 Hz / $\Delta t = 20\,\text{ms}$)**:
   * Evaluates the motion's Hermite splines / Bezier easing curves at the current playhead time $t$.
   * Clamps all 17 joint angles through `UrdfLimits` to guarantee mechanical safety limits.
   * Converts joint angles to PCA9685 PWM values using `calibration.json` scale factors and trim offsets.
2. **Binary Wire Protocol Transmission (`RHB1`)**:
   * All 17 channels are batched into a single 57-byte `RH_MSG_SET_PWM` packet (opcode `0x08`) as specified in `doc/Protocol.md`.
   * Transmitted over MQTT to `robot/robohero/<robot_id>/control`.
3. **Playback Controls & Safety Features**:
   * **Play / Pause / Stop**: Immediate transport control.
   * **Loop Playback**: Repeated cycles for walking gaits without stopping between steps.
   * **Smooth Return to Standby**: When stopping or completing a non-looping motion, the player generates a 300 ms linear blend back to the canonical Standby pose to prevent the robot from toppling or dropping abruptly.
   * **Live Scrubbing**: Dragging the timeline slider with "Sync to Robot" enabled sends the current pose to the robot in real time.

---

## 7. Build Instructions

### Linux Build

Prerequisites:
* Qt6 (or Qt5) Widgets, OpenGL, Core, Gui
* `libmosquitto-dev`, `libconfig++-dev`
* `pkg-config`, `cmake` (>= 3.20), `make`, `g++` (C++17)

Build commands:
```bash
cd app/motion
make
# or:
cmake -B build/linux -S .
cmake --build build/linux -j$(nproc)
```

Run application:
```bash
make run
# or:
./build/linux/robohero_motion
```

### Windows Build (MSVC)

See [doc/Compile-on-Windows.md](doc/Compile-on-Windows.md) for complete setup instructions using Visual Studio 2022, CMake presets, and vcpkg.

```powershell
cd app/motion
cmake --build build/windows-msvc --config Release
# or:
cmake --build --preset windows-msvc
```

---

## 8. Directory & File Structure

```
app/motion/
├── CMakeLists.txt              # Cross-platform CMake build configuration
├── CMakePresets.json           # Presets for Linux and Windows MSVC builds
├── Makefile                    # Makefile wrapper (build, clean, distclean, run)
├── motion_resources.qrc        # Qt resource bundle (model URDF, calibration, icons)
├── robohero_motion.rc          # Windows executable application icon resource
├── assets/                     # Application icons (shared with app/teleop)
│   ├── robohero_icon.png       # Window icon (PNG)
│   ├── robohero_head.png       # Viewport avatar/branding
│   └── robohero_icon.ico       # Windows executable icon (ICO)
├── doc/                        # Subproject documentation
│   ├── README.md               # Complete architecture, kinematics, and protocol guide
│   └── Compile-on-Windows.md   # Windows MSVC and vcpkg compilation instructions
├── motions/                    # Bundled default motion sequences (.rhm.json)
│   ├── walk_forward.rhm.json
│   ├── walk_turn_left.rhm.json
│   ├── bow.rhm.json
│   └── wave.rhm.json
└── src/                        # C++ source code (BSD style, 4-space indent)
    ├── main.cxx                # Application entry point
    ├── MainWindow.cxx/.hxx     # Main window, dock layouts, menu actions
    ├── UrdfViewerWidget.cxx/.hxx # OpenGL 3D viewer (Ghost Overlay & Split View)
    ├── UrdfLimits.cxx/.hxx     # Kinematic constraints and PWM translation
    ├── MqttClient.cxx/.hxx     # Low-latency binary MQTT client (RHB1)
    ├── MotionSequence.cxx/.hxx # Data container for keyframes, easing, and tracks
    ├── MotionLibrary.cxx/.hxx  # Filesystem scanner and session persistence
    ├── MotionLibraryWidget.cxx/.hxx # Motion library browser dock panel
    ├── MotionPlayer.cxx/.hxx   # 50 Hz trajectory sampling & robot streaming engine
    ├── ParametricGait.cxx/.hxx # Analytical 5-DOF IK and biped gait synthesizer
    ├── TimelineWidget.cxx/.hxx # Multi-track timeline slider, dope sheet, and scrub
    └── SettingsDialog.cxx/.hxx # MQTT connection and workspace folder settings
```

