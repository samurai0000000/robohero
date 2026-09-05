# RoboHero 17-Joint Calibration & URDF Constraints Walkthrough

**Calibration Date**: September 2, 2026 (2026-09-02)  
**Robot ID**: `TTR-ee40`  
**Host Controller Version**: `v2.1.7`  
**Project Version**: `v2.1.11`  

All 17 revolute joints on RoboHero have been physically measured using a protractor, calibrated for true angular scale ($\text{radPerPwm}$), and encoded into [model/robohero.urdf](../model/robohero.urdf) and [model/calibration.json](../model/calibration.json).

---

## 1. Calibrated Angular Scaling (`radPerPwm`)

The under-rotation issue was resolved by replacing the hardcoded $\frac{\pi}{270}$ ($0.667^\circ/\text{count}$) with differential multi-joint scale factors:

* **Leg Joints (Channels 0, 1, 2, 3, 4, 11, 12, 13, 14, 15)**: **$1.000^\circ/\text{count}$** ($\text{radPerPwm} = \frac{\pi}{180}$)
* **Arm Pitch/Roll Joints (Channels 5, 6, 9, 10)**: **$1.333^\circ/\text{count}$** ($\text{radPerPwm} = \frac{\pi}{135}$)
* **Elbow Joints (Channels 7, 8)**: **$1.000^\circ/\text{count}$** ($\text{radPerPwm} = \frac{\pi}{180}$)
* **Head Yaw (Channel 16)**: **$1.000^\circ/\text{count}$** ($\text{radPerPwm} = \frac{\pi}{180}$)

---

## 2. Master Calibration & Joint Constraints Table

| Ch | Joint Name | Standby Center | Measured PWM Range | Measured Degree Range | Encoded URDF Limits (`lower`, `upper`) |
|:---:|:---|:---:|:---:|:---:|:---|
| **0** | `left_ankle_roll_joint` | 160 | `[80, 180]` | `[-25.0°, +75.0°]` | `lower="-0.4363" upper="1.3090"` |
| **1** | `left_ankle_pitch_joint` | 161 | `[115, 245]` | `[-45.0°, +90.0°]` | `lower="-0.7854" upper="1.5708"` |
| **2** | `left_knee_pitch_joint` | 141 | `[38, 180]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` |
| **3** | `left_hip_pitch_joint` | 168 | `[65, 200]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` |
| **4** | `left_hip_roll_joint` | 158 | `[60, 178]` | `[-20.0°, +92.0°]` | `lower="-0.3491" upper="1.6057"` |
| **5** | `left_shoulder_pitch_joint` | 158 | `[5, 245]` | `[-180.0°, +90.0°]` | `lower="-3.1416" upper="1.5708"` |
| **6** | `left_shoulder_roll_joint` | 243 | `[70, 250]` | `[-5.0°, +170.0°]` | `lower="-0.0873" upper="2.9671"` |
| **7** | `left_elbow_joint` | 159 | `[85, 245]` | `[-75.0°, +85.0°]` | `lower="-1.3090" upper="1.4835"` |
| **8** | `right_elbow_joint` | 163 | `[75, 215]` | `[-85.0°, +55.0°]` | `lower="-1.4835" upper="0.9599"` |
| **9** | `right_shoulder_roll_joint` | 75 | `[65, 245]` | `[-170.0°, +5.0°]` | `lower="-2.9671" upper="0.0873"` |
| **10** | `right_shoulder_pitch_joint` | 156 | `[70, 270]` | `[-90.0°, +115.0°]` | `lower="-1.5708" upper="2.0071"` |
| **11** | `right_hip_roll_joint` | 161 | `[141, 259]` | `[-100.0°, +20.0°]` | `lower="-1.7453" upper="0.3491"` |
| **12** | `right_hip_pitch_joint` | 129 | `[89, 230]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` |
| **13** | `right_knee_pitch_joint` | 150 | `[110, 240]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` |
| **14** | `right_ankle_pitch_joint` | 165 | `[65, 210]` | `[-50.0°, +90.0°]` | `lower="-0.8727" upper="1.5708"` |
| **15** | `right_ankle_roll_joint` | 162 | `[142, 220]` | `[-25.0°, +70.0°]` | `lower="-0.4363" upper="1.2217"` |
| **16** | `head_yaw_joint` | 90 | `[40, 140]` | `[-80.0°, +80.0°]` | `lower="-1.3963" upper="1.3963"` |

---

## 3. Permanent Reference Documentation Created

1. **[joint_calibration_and_measurement_guide.md](joint_calibration_and_measurement_guide.md)**: Full physical measurement protocol, reference planes, and mathematical equations.
2. **[joint_calibration_results.md](joint_calibration_results.md)**: Full empirical results report with recommended EEPROM trims.
3. **[joint_calibration_results.json](joint_calibration_results.json)**: Machine-readable calibration dataset.
4. **[joint_calibration_20260902.md](joint_calibration_20260902.md)**: Calibration summary and execution records.

---

## 4. Automated Verification Results

- `python3 scripts/validate_urdf.py`: **PASSED** (17 channels verified and kinematic hierarchy intact).
- `npm run build` in `app/web`: **PASSED** (Built clean production bundle).
