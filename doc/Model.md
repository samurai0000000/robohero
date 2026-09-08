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
  - **Vision Retargeting Scope**: The MediaPipe vision pipeline retargets arm pitch/roll, elbow bends, hip/knee pitch, and head yaw from the operator's webcam stream. Lower extremity roll joints (`left_ankle_roll_joint` / CH 0 and `right_ankle_roll_joint` / CH 15) are **not tracked** and remain stationary at neutral standby ($0.0\text{ rad}$ / $PWM_0$).

### 2.3 Motion Studio (`app/motion`)
- **Build Mechanism**: Qt Resource system (`motion_resources.qrc`) packages `../../model/robohero.urdf` and `../../model/calibration.json`.
- **Runtime Derivation**:
  - Exposes degree sliders strictly bounded to `[degree_range[0], degree_range[1]]` derived from `calibration.json` / `urdf_limits`.
  - Converts user slider angles to PWM counts in real time. Because it is the first companion tool to enforce kinematics limits directly on GUI sliders, any mismatch in `urdf_limits` or `sign` immediately manifests here.

### 2.4 Android Mobile App (`app/android`)
- **Build Mechanism**: `app/android/app/build.gradle.kts` adds `../../model` to `sourceSets["main"].assets.srcDirs`.
- **Runtime Derivation**:
  - `TrimDefinitions.defaultTrims(context)` parses `model/calibration.json` via Android's `AssetManager` and `org.json.JSONObject` solely for the `"trims"` dictionary (`[-125, +125]` EEPROM offsets). It does not compute joint kinematics or limit bounds.

### 2.5 Command Line Interface (`app/cli`)
- **Runtime Operation**: Sends raw PWM command strings (`pwm <ch> <val>`) directly to the controller via serial or TCP/UDP socket without kinematics constraints or limit checks.

### 2.6 Control Paradigms: Raw PWM vs. Degree-Bounded Kinematics
Understanding why different subprojects exhibit or mask limit anomalies is fundamental:

| Application | Control Input | Kinematic Limits Enforced? | Ankle Roll (CH 0) Exercised? | Behavior Under Bad `urdf_limits` |
|---|---|:---:|:---:|---|
| **`app/web`** | Raw PWM counts (`[1, 270]`) | ❌ No (Direct PWM) | ✅ Yes (Sliders 80..180) | Unaffected; user can move PWM freely |
| **`app/cli`** | Raw PWM commands | ❌ No (Bypass) | ✅ Yes | Unaffected; sends raw PWM to firmware |
| **`app/teleop`** | Webcam Pose Tracking | ✅ Yes (MediaPipe $\to$ rad) | ❌ No (Pinned at $0.0\text{ rad}$) | Unaffected; joint is not retargeted |
| **`app/android`**| Trim Offsets (`[-125, +125]`)| ❌ No (EEPROM only) | ❌ No | Unaffected; only edits EEPROM trims |
| **`app/motion`** | Joint Degrees ($\theta^\circ$) | ✅ **Yes** (`urdf_limits`) | ✅ **Yes** (Timeline & Keyframes) | **Fails if limits/signs are incorrect** |

---

## 3. Kinematic Sign Conventions & Bilateral Symmetry

RoboHero is a 17-DOF humanoid robot. To ensure consistent 3D rendering and motion generation across all companion applications, all joint transformations follow standard ROS/URDF right-hand coordinate frames:
- **Sagittal Plane ($Z-X$)**: Pitch joints. Axis is typically $+Y = [0, 1, 0]$.
- **Frontal Plane ($Z-Y$)**: Roll joints. Axis is typically $+X = [1, 0, 0]$.
- **Transverse Plane ($X-Y$)**: Yaw joints. Axis is typically $+Z = [0, 0, 1]$.

Because identical servos on left and right limbs are mounted in mirror symmetry, their physical rotation directions invert relative to the robot's coordinate frame. The `sign` property (`+1.0` or `-1.0`) in `model/calibration.json` reconciles physical servo rotation with the canonical URDF joint axis:

$$\theta = (\text{pwm} - \text{center}) \times \text{rad\_per\_pwm} \times \text{sign}$$

### Canonical 17-Joint Kinematic Sign & Motion Reference Table

| Ch | URDF Joint Name | Group | Axis | Center | Sign | Plane | Positive Direction ($+\theta$) | Negative Direction ($-\theta$) |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :--- | :--- |
| **0** | `left_ankle_roll_joint` | Left Leg | `1 0 0` | 160 | **+1.0** | Frontal | Outer sole down (Eversion) | Outer sole up (Inversion) |
| **1** | `left_ankle_pitch_joint` | Left Leg | `0 1 0` | 161 | **-1.0** | Sagittal | Toes pitch up (Dorsiflexion) | Toes pitch down (Plantarflexion) |
| **2** | `left_knee_pitch_joint` | Left Leg | `0 1 0` | 141 | **-1.0** | Sagittal | Knee extends forward (Kick) | Knee flexes backward (Bend) |
| **3** | `left_hip_pitch_joint` | Left Leg | `0 1 0` | 168 | **+1.0** | Sagittal | Thigh swings backward | Thigh swings forward |
| **4** | `left_hip_roll_joint` | Left Leg | `1 0 0` | 158 | **-1.0** | Frontal | Leg swings inward (Adduction) | Leg swings outward (Abduction) |
| **5** | `left_shoulder_pitch_joint` | Left Arm | `0 1 0` | 158 | **+1.0** | Sagittal | Arm swings backward | Arm swings forward / up |
| **6** | `left_shoulder_roll_joint` | Left Arm | `1 0 0` | 252 | **-1.0** | Frontal | Arm lifts outward / overhead | Arm tucks inward to torso |
| **7** | `left_elbow_joint` | Left Arm | `1 0 0` | 159 | **+1.0** | Frontal | Forearm bends outward | Forearm bends inward |
| **8** | `right_elbow_joint` | Right Arm | `1 0 0` | 163 | **+1.0** | Frontal | Forearm bends inward | Forearm bends outward |
| **9** | `right_shoulder_roll_joint` | Right Arm | `1 0 0` | 69 | **-1.0** | Frontal | Arm lifts outward / overhead | Arm tucks inward to torso |
| **10**| `right_shoulder_pitch_joint`| Right Arm | `0 1 0` | 163 | **-1.0** | Sagittal | Arm swings forward / up | Arm swings backward |
| **11**| `right_hip_roll_joint` | Right Leg | `1 0 0` | 161 | **-1.0** | Frontal | Leg swings outward (Abduction) | Leg swings inward (Adduction) |
| **12**| `right_hip_pitch_joint` | Right Leg | `0 1 0` | 129 | **-1.0** | Sagittal | Thigh swings forward | Thigh swings backward |
| **13**| `right_knee_pitch_joint` | Right Leg | `0 1 0` | 150 | **+1.0** | Sagittal | Knee flexes backward (Bend) | Knee extends forward (Kick) |
| **14**| `right_ankle_pitch_joint`| Right Leg | `0 1 0` | 165 | **+1.0** | Sagittal | Toes pitch down (Plantarflexion)| Toes pitch up (Dorsiflexion) |
| **15**| `right_ankle_roll_joint` | Right Leg | `1 0 0` | 162 | **+1.0** | Frontal | Outer sole down (Eversion) | Outer sole up (Inversion) |
| **16**| `head_yaw_joint` | Head | `0 0 1` | 90 | **+1.0** | Transverse| Head pans left | Head pans right |

### 3.1 Bilateral Roll Symmetry & Ankle Coordinate Convention

Both `left_ankle_roll_joint` (CH 0) and `right_ankle_roll_joint` (CH 15) define their URDF rotation axis as $+X = [1, 0, 0]$ (pointing forward).

Because both roll joints share the same forward axis vector $+X$, a positive rotation ($+\theta$) tilts the $+Y$ link upward and $-Y$ link downward. However, because the feet sit on opposite sides of the sagittal midline, this produces **opposite anatomical effects**:
- **Right Foot (CH 15, on $-Y$ side of pelvis)**:
  - Positive rotation ($+\theta$): Tilts the sole inward (Inversion), reaching up to $+70.0^\circ$ bracket clearance before collision.
  - Negative rotation ($-\theta$): Tilts the sole outward (Eversion), with only $-25.0^\circ$ clearance.
  - Calibrated range: **`[-25.0°, +70.0°]`** (`lower="-0.4363" upper="1.2217"`).
- **Left Foot (CH 0, on $+Y$ side of pelvis)**:
  - Positive rotation ($+\theta$): Tilts the sole outward (Eversion), with only $+25.0^\circ$ clearance.
  - Negative rotation ($-\theta$): Tilts the sole inward (Inversion), reaching up to $-80.0^\circ$ bracket clearance before collision.
  - Calibrated range: **`[-80.0°, +25.0°]`** (`lower="-1.3963" upper="0.4363"`).
- **Common Servo Sign**:
  Both channels use `sign: +1.0`, meaning higher PWM commands positive rotation ($+\theta$) and lower PWM commands negative rotation ($-\theta$).

---

## 4. Mathematical Invariants & Validation Rules

Any companion application or data model managing RoboHero joint angles MUST preserve the following mathematical invariants:

### 4.1 Angle $\leftrightarrow$ PWM Conversion Invariants

- **PWM to Radians**:
  $$\theta = (\text{pwm} - \text{center}) \times \text{rad\_per\_pwm} \times \text{sign}$$
- **Radians to PWM**:
  $$\text{pwm} = \text{round}\left(\text{center} + \frac{\text{clamp}(\theta, \text{lower}, \text{upper})}{\text{rad\_per\_pwm} \times \text{sign}}\right)$$

### 4.2 Range Consistency & Reachability Invariant

For every joint channel:
1. Transforming the physical PWM operating bounds $[\text{pwm}_{\min}, \text{pwm}_{\max}]$ through the kinematic conversion formula MUST span the URDF angular limits without clipping:
   $$\min(\theta(\text{pwm}_{\min}), \theta(\text{pwm}_{\max})) \approx \text{urdf\_limits.lower}$$
   $$\max(\theta(\text{pwm}_{\min}), \theta(\text{pwm}_{\max})) \approx \text{urdf\_limits.upper}$$

2. **Pitfall Alert (Sign Inversion & Asymmetric Bounds Bug)**:
   Never invert the kinematic `sign` (e.g. changing `+1.0` to `-1.0`) to force-fit asymmetric limit bounds:
   - On `left_ankle_roll_joint` (CH 0), the physical operating range is $PWM \in [80, 180]$ around center $160$.
   - The large travel of $80$ counts ($PWM 80$) corresponds to $-80.0^\circ$, while the small travel of $20$ counts ($PWM 180$) corresponds to $+25.0^\circ$.
   - An earlier misconfiguration assigned the symmetric-looking bounds `[-25.0°, +75.0°]` (copying the right ankle's positive large travel) and set `sign: -1.0` to reach $PWM 80$ at positive angles. This resulted in two critical failures:
     1. **Reversed Physical Motion**: In the Motion Editor, moving the slider to positive angles commanded lower PWM, causing the physical servo to turn in reverse while the 3D model turned forward.
     2. **Slider Travel Truncation**: Clamping to lower bound $-25.0^\circ$ prevented the user from commanding PWM below $135$, discarding $55^\circ$ ($55$ PWM counts) of valid physical motion.
   - The correct configuration is `sign: +1.0`, `degree_range: [-80.0, 25.0]`, and `urdf_limits: { "lower": -1.3963, "upper": 0.4363 }`.

### 4.3 Degree Range Synchronization
`model/calibration.json` specifies both `degree_range` (in degrees) and `urdf_limits` (in radians). Companion tools must ensure:
$$\text{degree\_range}[0] \equiv \text{round}\left(\text{urdf\_limits.lower} \times \frac{180}{\pi}, 1\right)$$
$$\text{degree\_range}[1] \equiv \text{round}\left(\text{urdf\_limits.upper} \times \frac{180}{\pi}, 1\right)$$

---

## 5. Guidelines for Future Applications

When creating new host tools, simulations (e.g. Gazebo, Isaac Sim, Webots), or control modules:

1. **Do NOT Hard-Code Joint Properties**:
   - Never hard-code channel numbers, center PWMs, scale factors (`rad_per_pwm`), signs, or angle limits in C++, Python, JavaScript, or Kotlin code.
2. **Derive from `model/`**:
   - For build systems with asset packing (Make, CMake, Gradle, Cargo): Add a build step or resource alias pointing to `model/robohero.urdf` and `model/calibration.json`.
   - For ROS/ROS2 nodes: Reference `$(find robohero)/model/robohero.urdf` or `$(find robohero)/model/robohero.xacro`.
3. **Always Run URDF & Calibration Validation**:
   - Execute `python3 scripts/validate_urdf.py` before committing any model or calibration changes.

