# RoboHero Joint Calibration & Physical Measurement Guide

This document defines the methodology, measurement tools, anatomical reference datums, mathematical formulations, and clearance protocols for:
1. Calibrating the true physical angular scaling factor ($\text{radPerPwm}$) independently of existing EEPROM trim biases.
2. Measuring and encoding the exact physical rotation limits ($\text{lower}$ and $\text{upper}$ bounds in radians) into the RoboHero URDF model.
3. Discovering the true geometric zero points ($0.0^\circ$) to refine firmware servo trims in EEPROM.

---

## 1. Physical Scale Discrepancy & Trim Offset Independence

### Scale Discrepancy
In the initial 3D viewer model, the angular scale was hardcoded as:

$$\text{radPerPwm} = \frac{\pi}{270} \approx 0.011636\text{ rad/count} \quad (0.667^\circ/\text{count})$$

Physical micro-servos (EMAX ES08MDII) travel their full $180^\circ$ rotation across $\sim 180 - 200\text{ counts}$ ($\sim 0.9^\circ \sim 1.0^\circ/\text{count}$). Consequently, a command of $\Delta\text{PWM} = +40$ rotates the physical robot by $\sim 36^\circ - 40^\circ$, whereas the 3D model only rotates by $26.7^\circ$ (under-rotating by ~30%).

### Handling Imperfect EEPROM Trims (Differential Calibration)
The existing servo trims stored in flash/EEPROM are approximate and may not align links at exact geometric $0.0^\circ$. To ensure measurements are **100% immune to trim inaccuracies**, we use **two-point differential angle measurements**:

$$\text{radPerPwm} = \frac{(\alpha_2^\circ - \alpha_1^\circ) \times (\pi / 180^\circ)}{PWM_2 - PWM_1}$$

Because this formula uses the delta between two measured points $(\Delta\alpha / \Delta PWM)$, it eliminates any constant trim or zero-offset error.

---

## 2. Measurement Tools Required

1. **Clear Plastic Protractor / Goniometer (180° or 360°)**:
   - Placed directly over the servo horn pivot center to measure angular deflection between parent and child link centerlines.
2. **Digital Inclinometer / Angle Gauge** (or Smartphone Clinometer App):
   - Zeroed against the reference parent link, then aligned with the moving child link to read relative angles directly.
3. **RoboHero Host Terminal (`robohero_cli`)**:
   - Used for interactive incremental servo actuation (`pwm <ch> <val>`, `center`, `relax`).

---

## 3. Clearance Posing & Joint Isolation (Avoiding Obstructions)

To prevent adjacent limbs or brackets from artificially blocking the joint under test:
- **Physical Setup**: Suspend the robot by the torso (or support on a riser) so limbs hang freely in mid-air.
- **Clearance Poses**: Before testing a joint, adjacent joints are placed in a dedicated clearance pose:

| Joint Under Test | Potential Obstruction | Clearance Pose Command Before Testing |
|---|---|---|
| **Shoulder Pitch (CH 5, 10)** | Upper arm rubbing against torso ribs | Abduct **Shoulder Roll (CH 6 / 9)** slightly outward ($+15^\circ$) to swing freely. |
| **Elbow (CH 7, 8)** | Forearm hitting chest or hips | Raise **Shoulder Pitch** forward $+30^\circ$ and Shoulder Roll outward $+20^\circ$ for $360^\circ$ clearance. |
| **Hip Pitch (CH 3, 12)** | Leg colliding with the opposite leg | Splay opposite leg outward via **Hip Roll** and bend tested knee $+30^\circ$ to swing freely. |
| **Knee Pitch (CH 2, 13)** | Foot hitting opposite leg | Lift **Hip Pitch** forward $+20^\circ$ so the shin has full backward flexion clearance. |
| **Ankles (CH 0, 1, 14, 15)** | Foot sole clipping table/ground | Suspend robot in air and bend knee $+20^\circ$ so the foot floats freely. |

---

## 4. Phase 1: 3-Joint Scale Factor Calibration ($\text{radPerPwm}$)

To ensure high statistical confidence across all 16 body servos (which share identical PCA9685 driver mapping), we measure 3 long-lever joints across positive and negative steps:

1. **Shoulder Pitch (CH 5)**: Measure angle at $PWM_1 = 120$ and $PWM_2 = 200$.
2. **Knee Pitch (CH 2)**: Measure angle at $PWM_1 = 100$ and $PWM_2 = 180$.
3. **Hip Pitch (CH 12)**: Measure angle at $PWM_1 = 90$ and $PWM_2 = 170$.
4. Compute the average $\text{radPerPwm}$ and update [RobotModel.js](../app/web/src/viewer/RobotModel.js).

---

## 5. Phase 2: Joint-by-Joint Mechanical Limit & True Zero Measurement

### Reference Planes & Datums

| Joint Channel & Name | Plane | Reference Datum ($0.0^\circ$) | Positive ($+^\circ$) Direction | Negative ($-^\circ$) Direction |
|---|---|---|---|---|
| **CH 16**: `head_yaw_joint` | Transverse ($X-Y$) | Chest forward normal ($+X$) | Head turns Left | Head turns Right |
| **CH 5**: `left_shoulder_pitch_joint` | Sagittal ($Z-X$) | Torso vertical side profile | Arm swings Backward | Arm swings Forward / Up |
| **CH 6**: `left_shoulder_roll_joint` | Frontal ($Z-Y$) | Torso lateral side profile | Arm lifts Outward / Up | Arm tucks Inward |
| **CH 7**: `left_elbow_joint` | Frontal ($Z-Y$) | Upper arm centerline | Forearm bends Outward | Forearm bends Inward |
| **CH 8**: `right_elbow_joint` | Frontal ($Z-Y$) | Upper arm centerline | Forearm bends Inward | Forearm bends Outward |
| **CH 9**: `right_shoulder_roll_joint` | Frontal ($Z-Y$) | Torso lateral side profile | Arm lifts Outward / Up | Arm tucks Inward |
| **CH 10**: `right_shoulder_pitch_joint` | Sagittal ($Z-X$) | Torso vertical side profile | Arm swings Forward / Up | Arm swings Backward |
| **CH 11**: `right_hip_roll_joint` | Frontal ($Z-Y$) | Pelvis vertical midline | Leg swings Outward | Leg swings Inward |
| **CH 12**: `right_hip_pitch_joint` | Sagittal ($Z-X$) | Torso vertical side line | Thigh swings Forward | Thigh swings Backward |
| **CH 13**: `right_knee_pitch_joint` | Sagittal ($Z-X$) | Thigh longitudinal axis | Shin bends Backward (flexion) | Shin extends Forward (kick) |
| **CH 14**: `right_ankle_pitch_joint` | Sagittal ($Z-X$) | Shin vertical axis ($90^\circ$ to sole) | Foot pitches Toes Down | Foot pitches Toes Up |
| **CH 15**: `right_ankle_roll_joint` | Frontal ($Z-Y$) | Foot sole ground plane | Inward tilt / Inversion ($+70^\circ$) | Outward tilt / Eversion ($-25^\circ$) |
| **CH 4**: `left_hip_roll_joint` | Frontal ($Z-Y$) | Pelvis vertical midline | Leg swings Inward | Leg swings Outward |
| **CH 3**: `left_hip_pitch_joint` | Sagittal ($Z-X$) | Torso vertical side line | Thigh swings Backward | Thigh swings Forward |
| **CH 2**: `left_knee_pitch_joint` | Sagittal ($Z-X$) | Thigh longitudinal axis | Shin extends Forward (kick) | Shin bends Backward (flexion) |
| **CH 1**: `left_ankle_pitch_joint` | Sagittal ($Z-X$) | Shin vertical axis ($90^\circ$ to sole) | Foot pitches Toes Up | Foot pitches Toes Down |
| **CH 0**: `left_ankle_roll_joint` | Frontal ($Z-Y$) | Foot sole ground plane | Outward tilt / Eversion ($+25^\circ$) | Inward tilt / Inversion ($-80^\circ$) |

> [!NOTE]
> **Bilateral Ankle Roll Coordinate Convention & Symmetry**:
> Both `left_ankle_roll_joint` (CH 0) and `right_ankle_roll_joint` (CH 15) define their URDF rotation axis as $+X = [1, 0, 0]$ (pointing forward).
> - Because both roll joints share the same forward $+X$ axis vector, a roll rotation produces **opposite anatomical effects** across the bilateral midline:
>   - For the **Right Ankle (CH 15)**: Positive rotation ($+\theta$) tilts the foot sole inward (inversion), allowing up to $+70^\circ$ bracket clearance before collision; negative rotation ($-\theta$) tilts outward (eversion) with only $-25^\circ$ clearance $\rightarrow$ calibrated range: **`[-25.0°, +70.0°]`** (`lower="-0.4363" upper="1.2217"`).
>   - For the **Left Ankle (CH 0)**: Positive rotation ($+\theta$) tilts the foot sole outward (eversion) with only $+25^\circ$ clearance; negative rotation ($-\theta$) tilts inward (inversion) with large $-80^\circ$ clearance $\rightarrow$ calibrated range: **`[-80.0°, +25.0°]`** (`lower="-1.3963" upper="0.4363"`).
> - Servo sign for both channels is **`+1.0`** (increasing PWM commands positive rotation).

---

### Measurement Protocol for Each Joint

1. **Set Clearance Pose**: Put sibling joints into their clearance orientation.
2. **Identify True Geometric Zero ($PWM_0$)**:
   - Jog PWM until link is physically at $0.0^\circ$ against the reference datum (e.g. shin perfectly straight with thigh).
   - Record $PWM_0$. Recommended EEPROM trim adjustment: $\Delta\text{Trim} = PWM_0 - \text{BaseCenter}$.
3. **Measure Maximum Stop ($\alpha_{\max}^\circ$)**:
   - Jog PWM in steps of $+5$ toward maximum travel, stopping $2^\circ \sim 3^\circ$ before bracket collision.
   - Measure angle $\alpha_{\max}^\circ$ with protractor and record $PWM_{\max}$.
4. **Measure Minimum Stop ($\alpha_{\min}^\circ$)**:
   - Jog PWM in steps of $-5$ toward minimum travel, stopping $2^\circ \sim 3^\circ$ before bracket collision.
   - Measure angle $\alpha_{\min}^\circ$ with protractor and record $PWM_{\min}$.
5. **Compute URDF Limits**:
   $$\text{limit lower} = \frac{\min(\alpha_{\min}, \alpha_{\max}) \times \pi}{180}, \quad \text{limit upper} = \frac{\max(\alpha_{\min}, \alpha_{\max}) \times \pi}{180}$$

---

## 6. Model Updating & Verification

1. **Update URDF & Calibration**: In [robohero.urdf](../model/robohero.urdf) and [calibration.json](../model/calibration.json), edit the `<limit>` tag and calibration properties for each joint.
2. **Update Web Viewer**: In [RobotModel.js](../app/web/src/viewer/RobotModel.js), set the calibrated `radPerPwm` and joint limits.
3. **Validate**:
   ```bash
   python3 scripts/validate_urdf.py
   cd app/web && npm run build
   ```
4. **Visual Inspection**: Open 3D Web Studio (`npm run dev`) and verify that 3D rendering tracks physical robot movement 1:1.

---

## 7. Saving Calibration Results for Future Reference

At the conclusion of the calibration process, all final empirical measurements and computed model limits will be permanently recorded and saved under `doc/` in:
- **`doc/joint_calibration_results.md`**: Human-readable calibration report with complete joint tables, measured physical angles, true $PWM_0$ geometric centers, recommended EEPROM trims, and bracket clearance notes.
- **`doc/joint_calibration_results.json`**: Machine-readable dataset containing the exact `radPerPwm`, center offsets, and `[lower, upper]` bounds for programmatic ingestion across tooling.
