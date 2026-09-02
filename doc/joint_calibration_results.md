# RoboHero Joint Calibration & Empirical Measurement Results

**Date**: 2026-09-02  
**Robot ID**: `TTR-ee40`  
**Host Version**: `v2.1.7` / Project `v2.1.10`  
**Kinematic Model**: `urdf/robohero.urdf`  
**Viewer Mapping**: `app/web/src/viewer/RobotModel.js`  

---

## 1. Calibrated Angular Scale Constants (`radPerPwm`)

The true physical angular scaling factor was calibrated using differential angle measurements ($\Delta\alpha^\circ / \Delta\text{PWM}$) across multiple joints to eliminate EEPROM trim biases:

| Limb Group | Servos | Calibrated Scale | Resolution |
|---|---|---|---|
| **Leg Joints** | Channels 0, 1, 2, 3, 4, 11, 12, 13, 14, 15 | **$1.000^\circ/\text{count}$** | $\text{radPerPwm} = \frac{\pi}{180} \approx 0.017453\text{ rad/count}$ |
| **Arm Joints (Pitch/Roll)** | Channels 5, 6, 9, 10 | **$1.333^\circ/\text{count}$** | $\text{radPerPwm} = \frac{\pi}{135} \approx 0.023271\text{ rad/count}$ |
| **Elbow Joints** | Channels 7, 8 | **$1.000^\circ/\text{count}$** | $\text{radPerPwm} = \frac{\pi}{180} \approx 0.017453\text{ rad/count}$ |
| **Head Yaw** | Channel 16 (GPIO 12) | **$1.000^\circ/\text{count}$** | $\text{radPerPwm} = \frac{\pi}{180} \approx 0.017453\text{ rad/count}$ |

---

## 2. Complete 17-Joint Empirical Limits Table

| Ch | Joint Name | Standby Center | Measured PWM Range | Measured Degree Range | Calibrated URDF Limits (`lower`, `upper`) | Mechanical Stop / Clearance Notes |
|:---:|:---|:---:|:---:|:---:|:---|:---|
| **0** | `left_ankle_roll_joint` | 160 | `[80, 180]` | `[-25.0°, +75.0°]` | `lower="-0.4363" upper="1.3090"` | $+75^\circ$ inward tilt / $-25^\circ$ outward tilt |
| **1** | `left_ankle_pitch_joint` | 161 | `[115, 245]` | `[-45.0°, +90.0°]` | `lower="-0.7854" upper="1.5708"` | $+90^\circ$ toes up / $-45^\circ$ toes down |
| **2** | `left_knee_pitch_joint` | 141 | `[38, 180]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` | $+90^\circ$ backward flexion / $-40^\circ$ forward kick |
| **3** | `left_hip_pitch_joint` | 168 | `[65, 200]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` | $+90^\circ$ forward kick / $-40^\circ$ backward extension |
| **4** | `left_hip_roll_joint` | 158 | `[60, 178]` | `[-20.0°, +92.0°]` | `lower="-0.3491" upper="1.6057"` | $+92^\circ$ outward leg split / $-20^\circ$ inward cross |
| **5** | `left_shoulder_pitch_joint` | 158 | `[5, 245]` | `[-180.0°, +90.0°]` | `lower="-3.1416" upper="1.5708"` | $-180^\circ$ straight overhead / $+90^\circ$ backward swing |
| **6** | `left_shoulder_roll_joint` | 243 | `[70, 250]` | `[-5.0°, +170.0°]` | `lower="-0.0873" upper="2.9671"` | $+170^\circ$ lateral outward lift / $0^\circ$ tucked at ribs |
| **7** | `left_elbow_joint` | 159 | `[85, 245]` | `[-75.0°, +85.0°]` | `lower="-1.3090" upper="1.4835"` | $+85^\circ$ outward bend / $-75^\circ$ inward bend |
| **8** | `right_elbow_joint` | 163 | `[75, 215]` | `[-85.0°, +55.0°]` | `lower="-1.4835" upper="0.9599"` | $-85^\circ$ outward bend / $+55^\circ$ inward bend |
| **9** | `right_shoulder_roll_joint` | 75 | `[65, 245]` | `[-170.0°, +5.0°]` | `lower="-2.9671" upper="0.0873"` | $-170^\circ$ lateral outward lift / $0^\circ$ tucked at ribs |
| **10** | `right_shoulder_pitch_joint` | 156 | `[70, 270]` | `[-90.0°, +115.0°]` | `lower="-1.5708" upper="2.0071"` | $+115^\circ$ forward lift / $-90^\circ$ backward swing |
| **11** | `right_hip_roll_joint` | 161 | `[141, 259]` | `[-100.0°, +20.0°]` | `lower="-1.7453" upper="0.3491"` | $-100^\circ$ outward leg split / $+20^\circ$ inward cross |
| **12** | `right_hip_pitch_joint` | 129 | `[89, 230]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` | $+90^\circ$ forward kick / $-40^\circ$ backward extension |
| **13** | `right_knee_pitch_joint` | 150 | `[110, 240]` | `[-40.0°, +90.0°]` | `lower="-0.6981" upper="1.5708"` | $+90^\circ$ backward flexion / $-40^\circ$ forward kick |
| **14** | `right_ankle_pitch_joint` | 165 | `[65, 210]` | `[-50.0°, +90.0°]` | `lower="-0.8727" upper="1.5708"` | $+90^\circ$ toes up / $-50^\circ$ toes down |
| **15** | `right_ankle_roll_joint` | 162 | `[142, 220]` | `[-25.0°, +70.0°]` | `lower="-0.4363" upper="1.2217"` | $+70^\circ$ inward tilt / $-25^\circ$ outward tilt |
| **16** | `head_yaw_joint` | 90 | `[40, 140]` | `[-80.0°, +80.0°]` | `lower="-1.3963" upper="1.3963"` | $\pm 80^\circ$ left/right neck yaw rotation |

---

## 3. Geometric Zero Points ($PWM_0$), Current Trims & Recommendations

| Ch | Joint Name | Nominal Base Center | Pre-Calibration Trim (Current) | Measured Geometric $PWM_0$ | **Recommended Trim ($\Delta\text{Trim}$)** | Net Change |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|
| 0 | `left_ankle_roll_joint` | 160 | **0** | 155 | **$-5$** | $-5$ |
| 1 | `left_ankle_pitch_joint` | 161 | **0** | 158 | **$-3$** | $-3$ |
| 2 | `left_knee_pitch_joint` | 141 | **0** | 140 | **$-1$** | $-1$ |
| 3 | `left_hip_pitch_joint` | 168 | **0** | 160 | **$-8$** | $-8$ |
| 4 | `left_hip_roll_joint` | 158 | **0** | 157 | **$-1$** | $-1$ |
| 5 | `left_shoulder_pitch_joint` | 158 | **0** | 165 | **$+7$** | $+7$ |
| 6 | `left_shoulder_roll_joint` | 243 | **0** | 245 | **$+2$** | $+2$ |
| 7 | `left_elbow_joint` | 159 | **0** | 160 | **$+1$** | $+1$ |
| 8 | `right_elbow_joint` | 163 | **0** | 160 | **$-3$** | $-3$ |
| 9 | `right_shoulder_roll_joint` | 75 | **0** | 70 | **$-5$** | $-5$ |
| 10 | `right_shoulder_pitch_joint` | 156 | **0** | 158 | **$+2$** | $+2$ |
| 11 | `right_hip_roll_joint` | 161 | **0** | 160 | **$-1$** | $-1$ |
| 12 | `right_hip_pitch_joint` | 129 | **0** | 132 | **$+3$** | $+3$ |
| 13 | `right_knee_pitch_joint` | 150 | **0** | 150 | **$0$** | $0$ |
| 14 | `right_ankle_pitch_joint` | 165 | **0** | 160 | **$-5$** | $-5$ |
| 15 | `right_ankle_roll_joint` | 162 | **0** | 163 | **$+1$** | $+1$ |
| 16 | `head_yaw_joint` | 90 | **0** | 90 | **$0$** | $0$ |

---

## 4. Rollback Reference
To rollback all trims to the pre-calibration baseline at any time, reset all trim values to `0` via the web UI (`calibrate.html`) or send:
`GET /api/trim?save=1&t0=0&t1=0&t2=0&t3=0&t4=0&t5=0&t6=0&t7=0&t8=0&t9=0&t10=0&t11=0&t12=0&t13=0&t14=0&t15=0&t16=0`

