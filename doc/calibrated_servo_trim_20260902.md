# RoboHero Calibrated Servo Trim & Rollback Guide

**Calibration Date**: September 2, 2026 (2026-09-02)  
**Robot ID**: `TTR-ee40`  
**Host Controller Version**: `v2.1.7`  
**Firmware Architecture**: ESP8266 + PCA9685 (54 Hz)  

---

## 1. Overview & Verification Result

In the RoboHero firmware (`Servo.cxx` and `Motions.cxx`), the effective servo pulse commanded for any pose or motion keyframe is defined as:

$$\text{Effective PWM} = \text{Base PWM} + \text{EEPROM Trim}$$

**Standing Verification Result**:
Following physical testing on September 2, 2026, the **Pre-Calibration Baseline ($Trim = 0$)** was confirmed to provide the optimal physical standing balance and stability for the robot assembly. The baseline trims ($0$) are retained as the active operational configuration.


---

## 2. Comparison & Trim Table (Current vs. Recommended)

| Ch | Joint Name | Group | Nominal Base Center | Pre-Calibration Trim (Current) | Calibrated True Zero $PWM_0$ | **Recommended Trim ($\Delta\text{Trim}$)** | Net Trim Change | Physical Alignment Rationale |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **0** | `left_ankle_roll_joint` | Left Leg | 160 | **0** | 155 | **$-5$** | $-5$ | Levels foot sole horizontally (removes inward tilt bias) |
| **1** | `left_ankle_pitch_joint` | Left Leg | 161 | **0** | 158 | **$-3$** | $-3$ | Aligns foot sole perfectly $90^\circ$ perpendicular to shin |
| **2** | `left_knee_pitch_joint` | Left Leg | 141 | **0** | 140 | **$-1$** | $-1$ | Aligns shin collinear with thigh (straight leg) |
| **3** | `left_hip_pitch_joint` | Left Leg | 168 | **0** | 160 | **$-8$** | $-8$ | Aligns thigh vertically with torso (eliminates forward lean) |
| **4** | `left_hip_roll_joint` | Left Leg | 158 | **0** | 157 | **$-1$** | $-1$ | Aligns leg vertically in frontal plane |
| **5** | `left_shoulder_pitch_joint` | Left Arm | 158 | **0** | 165 | **$+7$** | $+7$ | Aligns upper arm hanging vertically along torso side seam |
| **6** | `left_shoulder_roll_joint` | Left Arm | 243 | **0** | 245 | **$+2$** | $+2$ | Tucks upper arm flush against ribs in resting stance |
| **7** | `left_elbow_joint` | Left Arm | 159 | **0** | 160 | **$+1$** | $+1$ | Aligns forearm collinear with upper arm (straight arm) |
| **8** | `right_elbow_joint` | Right Arm | 163 | **0** | 160 | **$-3$** | $-3$ | Aligns forearm collinear with upper arm (straight arm) |
| **9** | `right_shoulder_roll_joint` | Right Arm | 75 | **0** | 70 | **$-5$** | $-5$ | Tucks upper arm flush against ribs in resting stance |
| **10** | `right_shoulder_pitch_joint` | Right Arm | 156 | **0** | 158 | **$+2$** | $+2$ | Aligns upper arm hanging vertically along torso side seam |
| **11** | `right_hip_roll_joint` | Right Leg | 161 | **0** | 160 | **$-1$** | $-1$ | Aligns leg vertically in frontal plane |
| **12** | `right_hip_pitch_joint` | Right Leg | 129 | **0** | 132 | **$+3$** | $+3$ | Aligns thigh vertically with torso |
| **13** | `right_knee_pitch_joint` | Right Leg | 150 | **0** | 150 | **$0$** | $0$ | Aligns shin collinear with thigh (already perfectly centered) |
| **14** | `right_ankle_pitch_joint` | Right Leg | 165 | **0** | 160 | **$-5$** | $-5$ | Aligns foot sole perfectly $90^\circ$ perpendicular to shin |
| **15** | `right_ankle_roll_joint` | Right Leg | 162 | **0** | 163 | **$+1$** | $+1$ | Levels foot sole horizontally |
| **16** | `head_yaw_joint` | Head | 90 | **0** | 90 | **$0$** | $0$ | Points face parallel to chest forward normal |

---

## 3. How to Apply the New Calibrated Trims

### Via Embedded Web API / Interface
Open `http://<robot_ip>/calibrate.html` or submit HTTP request:
```http
GET /api/trim?save=1&t0=-5&t1=-3&t2=-1&t3=-8&t4=-1&t5=7&t6=2&t7=1&t8=-3&t9=-5&t10=2&t11=-1&t12=3&t13=0&t14=-5&t15=1&t16=0
```

---

## 4. Rollback Procedure (Revert to Baseline)

If you wish to rollback to the pre-calibration state at any time, submit:
```http
GET /api/trim?save=1&t0=0&t1=0&t2=0&t3=0&t4=0&t5=0&t6=0&t7=0&t8=0&t9=0&t10=0&t11=0&t12=0&t13=0&t14=0&t15=0&t16=0
```
Or in the web calibration UI (`calibrate.html`), click **"Reset All Trims to 0"** and then **"Save to Flash"**.
