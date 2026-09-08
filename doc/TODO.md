# RoboHero Architecture TODO

This document tracks upcoming architectural improvements and refactoring tasks across the RoboHero project.

---

## 1. Shared C++ Companion Application Core Library (`librobohero_client` / `librobohero_gui`)

### Motivation
Both `app/teleop` (Teleoperation Dashboard) and `app/motion` (Motion & Gait Studio) independently implement and duplicate several core modules:
- URDF limit verification and calibration parsing (`UrdfLimits.cxx/.hxx`)
- RHB1 binary MQTT client and packet serialization (`MqttClient.cxx/.hxx`)
- 3D OpenGL URDF robot visualization and ghost rendering (`UrdfViewerWidget.cxx/.hxx`)
- Telemetry buffer state persistence and cross-thread Qt signal handling

Duplicating these components increases maintenance overhead and leads to recurring bugs when creating new desktop tools (e.g. subtle differences in `validMask` telemetry handling, missing Qt meta-type registrations, or divergent clamping logic).

### Proposed Structure

Create a shared library (e.g., in `libs/robohero_client` or `libs/robohero_gui`) that companion desktop applications can link against via CMake:

```
libs/
└── robohero_client/
    ├── CMakeLists.txt
    ├── include/
    │   ├── RoboHero/
    │   │   ├── UrdfLimits.hxx           # Canonical URDF limits & angle <-> PWM conversion
    │   │   ├── MqttClient.hxx           # Thread-safe RHB1 binary MQTT client
    │   │   ├── TelemetryState.hxx       # Persistent 17-channel telemetry buffer
    │   │   ├── UrdfViewerWidget.hxx     # Reusable Qt/OpenGL 3D robot viewer
    │   │   └── AppInit.hxx              # Standard Qt meta-type & surface format bootstrap
    │   └── ...
    └── src/
        ├── UrdfLimits.cxx
        ├── MqttClient.cxx
        ├── TelemetryState.cxx
        ├── UrdfViewerWidget.cxx
        └── AppInit.cxx
```

### Key Modules to Unify

1. **`UrdfLimits` & Kinematics Engine**:
   - Single implementation for loading `model/calibration.json` and `model/robohero.urdf`.
   - Unified `pwmToAngle`, `angleToPwm`, and `clamp` routines adhering to documented mathematical invariants.
   - Automated startup sanity assertions verifying that limits and signs are self-consistent.

2. **`MqttClient` & Protocol Handler**:
   - Encapsulates `libmosquitto` loop on a dedicated worker thread.
   - Encapsulates binary RHB1 packet serialization (`RH_MSG_SET_PWM`, `RH_MSG_STATUS`, `RH_MSG_CENTER`, etc.).
   - Built-in `TelemetryState` buffer that automatically maintains persistent channel positions and only updates channels with `validMask[ch] == true`.

3. **`UrdfViewerWidget` (Qt 3D OpenGL Viewer)**:
   - High-performance OpenGL rendering for RoboHero links and joints.
   - Dual-pose support: solid editable pose and ghost pose (reference stance or live telemetry).
   - Standard orbit, pan, zoom mouse controls, ground grid, and dark theme palette.

4. **Application Bootstrap Helper (`AppInit`)**:
   - Standard function `RoboHero::initApplication(QApplication &app)` to register all required Qt meta-types (`std::array<int, 17>`, `std::array<bool, 17>`, `std::array<double, 17>`) and configure default OpenGL surface formats.

5. **Rate-Limited Transmission Engine (`PoseBroadcaster`)**:
   - Built-in transmission throttling to decouple high-frequency UI/kinematics loops (50–60 Hz) from network transmission ($\le 25\text{ Hz}$).
   - Configurable transmit rate (default 20 Hz / 50 ms) to prevent saturating the robot's ESP8266 controller.
   - Built-in trailing-edge debouncer for interactive controls (sliders, joysticks, sensors) to prevent event flood.
