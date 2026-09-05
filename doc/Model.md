# RoboHero Robot Model & Calibration Architecture

This document describes the canonical robot model files managed under `model/*`, how existing companion applications derive kinematics and joint limits from this directory, and guidelines for future applications to integrate with the model architecture.

---

## 1. Directory Structure & Managed Files

All robot model geometry, kinematics topology, servo channel calibrations, and angular limits reside exclusively in the top-level `model/` directory:

```
model/
├── robohero.urdf        # Canonical 17-DOF URDF kinematic tree and link geometry
├── robohero.xacro       # ROS/XACRO wrapper with transmission definitions
└── calibration.json     # Machine-readable calibration parameters and PWM limits
```

### File Descriptions

1. **`model/robohero.urdf`**:
   - Standard ROS-compliant Universal Robot Description Format (URDF) XML.
   - Defines the 17 revolute joints, link inertial matrices, visual boxes/geometries, material colors, and `<limit lower="..." upper="..." />` angular boundaries in radians.
   - Root link: `base_link` (torso).

2. **`model/robohero.xacro`**:
   - Parametric XACRO model including `model/robohero.urdf`.
   - Attaches `PositionJointInterface` hardware transmissions to all 17 joint channels.

3. **`model/calibration.json`**:
   - Machine-readable Single Source of Truth (SOT) for runtime kinematics and firmware calibration.
   - Contains:
     - `channels`: Map of hardware channels (`0`..`16`) with:
       - `name`: URDF joint name (e.g., `left_ankle_roll_joint`).
       - `label`: Human-readable label (e.g., `Ankle Roll`).
       - `group`: Kinematic subgroup (`left_leg`, `right_leg`, `left_arm`, `right_arm`, `head`).
       - `center`: Physical standby center PWM (e.g., `160`).
       - `sign`: Kinematic direction multiplier (`+1.0` or `-1.0`).
       - `rad_per_pwm`: Angular scale factor in radians per PWM step.
       - `deg_per_pwm`: Angular scale factor in degrees per PWM step.
       - `pwm_range`: Physical hardware operating range `[min, max]`.
       - `degree_range`: Physical joint range `[min, max]`.
       - `urdf_limits`: Canonical lower and upper limits in radians (`lower`, `upper`).
       - `baseline_trim`: Factory baseline trim.
       - `recommended_trim`: Calibrated trim offset for physical robot alignment.
     - `trims`: Servo and system trim definitions (`0`..`19`), including motion delay, PWM frequency, and voltage calibration.

---

## 2. Subproject Integration Patterns

All companion applications derive their models and kinematics parameters from `model/*` via dependency tracking rather than hard-coding values in their respective source trees.

```
                  ┌──────────────────────┐
                  │ model/               │
                  │  ├── robohero.urdf   │
                  │  └── calibration.json│
                  └──────────┬───────────┘
                             │
       ┌─────────────────────┼─────────────────────┐
       │ (Makefile Rule)     │ (Qt AUTORCC)        │ (Gradle assets.srcDirs)
       ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│ app/web      │      │ app/teleop   │      │ app/android  │
│ public/model/│      │ :/model/*    │      │ assets/model/│
└──────────────┘      └──────────────┘      └──────────────┘
```

### 2.1 Web Application (`app/web`)
- **Build Mechanism**: `app/web/Makefile` defines pattern rules:
  ```makefile
  $(PUBLIC_MODEL_DIR)/%: $(MODEL_DIR)/%
      @mkdir -p $(PUBLIC_MODEL_DIR)
      cp $< $@
  ```
- **Runtime Derivation**:
  - `app/web/src/viewer/RobotModel.js` imports `model/calibration.json` dynamically to construct its `CHANNEL_MAP` and loads `./model/robohero.urdf` into Three.js with `urdf-loader`.

### 2.2 Desktop Teleoperation (`app/teleop`)
- **Build Mechanism**: Qt Resource system (`teleop_resources.qrc`) packages `../../model/robohero.urdf` as `:/model/robohero.urdf` and `../../model/calibration.json` as `:/model/calibration.json`.
- **CMake Dependency**: `CMakeLists.txt` declares `OBJECT_DEPENDS` on `model/*` so modifying model files automatically triggers `rcc` re-compilation.
- **Runtime Derivation**:
  - `UrdfLimits::instance().init(":/model/robohero.urdf", ":/model/calibration.json")` parses `calibration.json` via Qt's `QJsonDocument` and verifies XML joint limits dynamically.

### 2.3 Android Mobile App (`app/android`)
- **Build Mechanism**: `app/android/app/build.gradle.kts` adds `../../model` to `sourceSets["main"].assets.srcDirs`.
- **Runtime Derivation**:
  - `TrimDefinitions.defaultTrims(context)` parses `model/calibration.json` via Android's `AssetManager` and `org.json.JSONObject`.

---

## 3. Guidelines for Future Applications

When creating new host tools, simulations (e.g. Gazebo, Isaac Sim, Webots), or control modules:

1. **Do NOT Hard-Code Joint Properties**:
   - Never hard-code channel numbers, center PWMs, scale factors (`rad_per_pwm`), signs, or angle limits in C++, Python, JavaScript, or Kotlin code.
2. **Derive from `model/`**:
   - For build systems with asset packing (Make, CMake, Gradle, Cargo): Add a build step or resource alias pointing to `model/robohero.urdf` and `model/calibration.json`.
   - For ROS/ROS2 nodes: Reference `$(find robohero)/model/robohero.urdf` or `$(find robohero)/model/robohero.xacro`.
3. **Use the Kinematics Equations**:
   - Conversion from PWM to Radians:
     $$\theta = (\text{pwm} - \text{center}) \times \text{rad\_per\_pwm} \times \text{sign}$$
   - Conversion from Radians to PWM:
     $$\text{pwm} = \text{round}\left(\text{center} + \frac{\text{clamp}(\theta, \text{lower}, \text{upper})}{\text{rad\_per\_pwm} \times \text{sign}}\right)$$
4. **Validation**:
   - Always run `python3 scripts/validate_urdf.py` after updating `model/robohero.urdf` or `model/calibration.json` to verify consistency before building applications.
