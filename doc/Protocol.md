# RoboHero Communication Protocols Specification

This document specifies the communication protocols utilized by the RoboHero bipedal humanoid robot. RoboHero supports both binary packed wire protocols over MQTT for high-frequency low-latency teleoperation/telemetry, and ASCII/JSON topics for Home Assistant integration.

---

## 1. MQTT Topic Hierarchy

All MQTT topics follow a structured namespace based on the robot's unique identifier (`<robot_id>`, defaults to `robohero` or MAC-derived string).

| Topic | Direction | Format | Description |
| :--- | :--- | :--- | :--- |
| `robot/robohero/<robot_id>/control` | Client &rarr; Robot | Binary (`RHB1`) | Real-time motion control, servo positioning, and program commands |
| `robot/robohero/<robot_id>/status` | Robot &rarr; Client | Binary (`RHB1`) | High-frequency telemetry stream (timestamp, voltage, servo positions) |
| `robot/robohero/<robot_id>/cmd` | Client &rarr; Robot | UTF-8 Text | High-level string commands (e.g. `forward`, `standby`, `wave`, `pm <id>`) |
| `robot/robohero/<robot_id>/head/set` | Client &rarr; Robot | UTF-8 Text | Head pan yaw angle slider in degrees (`-90` to `90`) |
| `robot/robohero/<robot_id>/state` | Robot &rarr; Client | JSON | Low-frequency periodic state & battery telemetry for Home Assistant |
| `robot/robohero/<robot_id>/availability` | Robot &rarr; Broker | UTF-8 Text | MQTT Birth / Last Will & Testament (`online` / `offline`) |

---

## 2. Binary Wire Protocol (`RHB1`)

The binary wire protocol is optimized for low-latency transmission over 802.11 Wi-Fi and MQTT. All multi-byte integers are encoded in **Little-Endian** byte order.

### 2.1 Packet Header (6 Bytes Fixed)

Every binary packet begins with a 6-byte header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Magic ('RHB1' = 0x31424852)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Msg Type    |  Payload Len  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **`magic`** (`uint32_t`, 4 bytes): Magic identifier `0x31424852` (`'R'`, `'H'`, `'B'`, `'1'` in ASCII, Little-Endian).
- **`msg_type`** (`uint8_t`, 1 byte): Message type opcode.
- **`payload_len`** (`uint8_t`, 1 byte): Byte length of the TLV payload sequence following the header (`0` to `249`).

### 2.2 Message Types (`msg_type`)

| Opcode | Identifier | Direction | Payload Description |
| :---: | :--- | :--- | :--- |
| `0x01` | `RH_MSG_STATUS` | Robot &rarr; Client | Telemetry status payload containing timestamp, voltage, and servo channels |
| `0x02` | `RH_MSG_STOP` | Client &rarr; Robot | Emergency stop command (payload length = 0) |
| `0x03` | `RH_MSG_CENTER` | Client &rarr; Robot | Neutral standby pose command (payload length = 0) |
| `0x04` | `RH_MSG_ZERO` | Client &rarr; Robot | Zero calibration pose command (payload length = 0) |
| `0x05` | `RH_MSG_RELAX` | Client &rarr; Robot | Torque-off / relax command (payload length = 0) |
| `0x06` | `RH_MSG_PM` | Client &rarr; Robot | Execute standard motion program (contains `RH_TLV_PROG`) |
| `0x07` | `RH_MSG_PMS` | Client &rarr; Robot | Execute special motion program (contains `RH_TLV_PROG`) |
| `0x08` | `RH_MSG_SET_PWM` | Client &rarr; Robot | Set servo PWM positions (contains 1 or more `RH_TLV_SERVO`) |

---

## 3. Tag-Length-Value (TLV) Structures

The payload consists of zero or more contiguous TLV items:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   TLV Type    |    TLV Len    |         Value Bytes ...       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 3.1 TLV Types

| Type ID | Identifier | Length (`len`) | Value Format | Description |
| :---: | :--- | :---: | :--- | :--- |
| `0x01` | `RH_TLV_TIME` | 4 | `uint32_t` (ms) | Millisecond timestamp (system uptime) |
| `0x02` | `RH_TLV_VOLTAGE` | 2 | `int16_t` (cV) | Battery voltage in centivolts (e.g. `740` = 7.40V) |
| `0x03` | `RH_TLV_SERVO` | 3 | `uint8_t chan`, `int16_t pos` | Servo channel (`0..16`) and target position (`1..270`) |
| `0x04` | `RH_TLV_PROG` | 2 | `int16_t prog_id` | Motion program number / action ID |

#### `RH_TLV_SERVO` Value Layout (3 Bytes):
- `chan` (`uint8_t`, 1 byte): Servo index `0` through `16`.
- `pos` (`int16_t`, 2 bytes LE): Target PWM value in degrees / PCA9685 count.

---

## 4. Single-Servo vs. Variable-Length Multi-Servo Payloads

### 4.1 Legacy Single-Position Payload (11 Bytes)
In legacy single-position control payloads, setting a single servo required publishing an individual 11-byte packet:

```
[Header: 6B] [RH_TLV_SERVO: 5B (type=3, len=3, chan, pos_low, pos_high)] = 11 Bytes
```

When transmitting a full 17-channel posture update, the legacy approach required publishing **17 distinct MQTT messages** (187 bytes of payload + 17x IP/TCP/MQTT header overheads), causing unnecessary network congestion and packet jitter.

### 4.2 Efficient Variable-Length Batched Payload (Up to 91 Bytes)
With variable-length payload support, multiple `RH_TLV_SERVO` items are packed into a single `RH_MSG_SET_PWM` packet:

```
+----------------+----------------+----------------+-----+----------------+
|  Header (6B)   | Servo TLV 0    | Servo TLV 1    | ... | Servo TLV N-1  |
|  (Type=8, Len) | (Type=3,Len=3) | (Type=3,Len=3) | ... | (Type=3,Len=3) |
+----------------+----------------+----------------+-----+----------------+
```

- For all 17 servos: $6 + 17 \times 5 = 91\text{ bytes}$ total in a **single** MQTT packet.
- Senders can pack any subset of channels (1 to 17 servos) whose positions have changed.

### 4.3 Control Ingestion & Non-Control TLVs
When processing incoming packets on `robot/robohero/<robot_id>/control`:
- Firmware iterates across all TLVs until `off >= payload_len`.
- All `RH_TLV_SERVO` items inside `RH_MSG_SET_PWM` (or `RH_MSG_STATUS`) are applied to the PCA9685 / GPIO12 controllers.
- Telemetry TLVs such as `RH_TLV_VOLTAGE` or `RH_TLV_TIME` are ignored cleanly without interrupting the parsing loop.

### 4.4 Transmission Rate Budgeting & Flow Control
To guarantee responsive robot performance without packet loss or jitter, clients MUST adhere to the following transmission rate constraints:

1. **Recommended Publish Rate**: **15 Hz – 25 Hz** (default **20 Hz / 50 ms**).
2. **Maximum Safe Burst Rate**: $\le 30\text{ Hz}$. Clients transmitting above 30 Hz risk overwhelming the ESP8266 LwIP socket buffers.
3. **The "Pause-and-Go" Phenomenon**:
   - The robot controller outputs servo pulses via PCA9685 at **54 Hz**.
   - The ESP8266 drives the PCA9685 over software bit-banged I2C and automatically transmits `RH_MSG_STATUS` telemetry whenever a servo position changes.
   - When clients flood incoming commands at 50–100+ Hz, bidirectional TCP traffic exhausts the ESP8266's small packet pool. LwIP triggers TCP zero-window flow control and retransmissions, creating periodic stalls ("pauses") followed by burst flushes ("go").
4. **Decouple UI Evaluation from Network Transmission**:
   - Local 3D rendering engines (Three.js, Qt OpenGL) should evaluate kinematics at 50–60 Hz for fluid user experience.
   - Network transmission should run on an independent timer/throttle capped at $\le 25\text{ Hz}$.
5. **Interactive UI Throttling (Trailing-Edge Pattern)**:
   - Interactive UI sliders, joysticks, or gesture trackers must never publish unthrottled on raw event callbacks.
   - Clients must implement a single-shot trailing-edge throttle timer:
     - On first event: transmit immediately and arm timer for $T = 1000 / \text{rate\_hz}$ ms.
     - On subsequent events while timer is active: mark update as pending.
     - On timer timeout: if an update is pending, transmit the latest pose and re-arm timer.

---

## 5. Servo Channel Mapping & Calibration Reference

The following table lists the 17 servo channels, their mapping to physical hardware controllers, calibrated standby centers, and physical operating ranges. The authoritative Single Source of Truth is `model/calibration.json`.

| Channel | URDF Joint Name | Label | Group | Center PWM | PWM Range | Hardware Controller |
| :---: | :--- | :--- | :---: | :---: | :---: | :---: |
| **0** | `left_ankle_roll_joint` | Ankle Roll | Left Leg | 160 | [80, 180] | PCA9685 Ch 0 |
| **1** | `left_ankle_pitch_joint` | Ankle Pitch | Left Leg | 161 | [115, 245] | PCA9685 Ch 1 |
| **2** | `left_knee_pitch_joint` | Knee Pitch | Left Leg | 141 | [38, 180] | PCA9685 Ch 2 |
| **3** | `left_hip_pitch_joint` | Hip Pitch | Left Leg | 168 | [65, 200] | PCA9685 Ch 3 |
| **4** | `left_hip_roll_joint` | Hip Roll | Left Leg | 158 | [60, 178] | PCA9685 Ch 4 |
| **5** | `left_shoulder_pitch_joint` | Shoulder Pitch | Left Arm | 158 | [5, 245] | PCA9685 Ch 5 |
| **6** | `left_shoulder_roll_joint` | Shoulder Roll | Left Arm | 252 | [70, 252] | PCA9685 Ch 6 |
| **7** | `left_elbow_joint` | Elbow | Left Arm | 159 | [85, 245] | PCA9685 Ch 7 |
| **8** | `right_elbow_joint` | Elbow | Right Arm | 163 | [75, 215] | PCA9685 Ch 8 |
| **9** | `right_shoulder_roll_joint` | Shoulder Roll | Right Arm | 69 | [69, 250] | PCA9685 Ch 9 |
| **10**| `right_shoulder_pitch_joint`| Shoulder Pitch | Right Arm | 163 | [70, 270] | PCA9685 Ch 10 |
| **11**| `right_hip_roll_joint` | Hip Roll | Right Leg | 161 | [141, 259] | PCA9685 Ch 11 |
| **12**| `right_hip_pitch_joint` | Hip Pitch | Right Leg | 129 | [89, 230] | PCA9685 Ch 12 |
| **13**| `right_knee_pitch_joint` | Knee Pitch | Right Leg | 150 | [110, 240] | PCA9685 Ch 13 |
| **14**| `right_ankle_pitch_joint`| Ankle Pitch | Right Leg | 165 | [65, 210] | PCA9685 Ch 14 |
| **15**| `right_ankle_roll_joint` | Ankle Roll | Right Leg | 162 | [142, 220] | PCA9685 Ch 15 |
| **16**| `head_yaw_joint` | Head Yaw | Head | 90 | [40, 140] | ESP8266 GPIO12 |

---

## 6. Client Telemetry Ingestion & Threading Contract

Companion applications (`app/teleop`, `app/motion`, web apps) consuming robot telemetry must implement the following state management and threading patterns:

### 6.1 Sparse / Delta Telemetry Contract
`RH_MSG_STATUS` packets may carry updates for an arbitrary subset of channels (1 to 17 servos) rather than full-state sweeps. The client MQTT parser yields:
- `pwmValues`: Array of 17 integer PWM values.
- `validMask`: Array of 17 boolean flags indicating which channels were present in the received packet.

#### Persistent State Invariant
Clients **must maintain persistent joint state buffers** initialized to standby center angles. When processing a telemetry packet:
1. Only mutate channels where `validMask[ch] == true`.
2. Leave all unmasked channels untouched at their previous known positions.

```cpp
// Correct Client Telemetry Handler (Persistent Array Pattern)
void MainWindow::onTelemetryReceived(const std::array<int, 17> &pwmValues,
                                     const std::array<bool, 17> &validMask)
{
    bool updated = false;
    const auto &limits = UrdfLimits::instance();
    for (int ch = 0; ch < 17; ++ch) {
        if (validMask[ch]) {
            _telemPwm[ch] = pwmValues[ch];
            _telemAngles[ch] = limits.pwmToAngle(ch, pwmValues[ch]);
            updated = true;
        }
    }

    if (updated) {
        _urdfViewer->setJointAngles(_telemAngles);
        _urdfViewer->setJointPwm(_telemPwm);
    }
}
```

> [!CAUTION]
> **Anti-Pattern Warning**: Do NOT create a temporary angle array and compute `pwmToAngle(i, pwmValues[i])` for all 17 channels without checking `validMask[i]`. Inactive channels have `pwmValues[i] == 0`, which maps to $(0 - \text{center}) \times \dots = -160^\circ$. This slams un-updated joints to extreme mechanical limits and severely distorts 3D ghost visualization.

### 6.2 Threading & Qt Meta-Type Registration Contract
Client MQTT implementations (such as `MqttClient` based on `libmosquitto`) run network I/O and message ingestion on a background worker thread. When passing telemetry data across threads to the Qt GUI event loop:

1. **Meta-Type Registration**: Qt requires non-primitive parameter types passed via signals across threads to be registered at startup. In `main.cxx`, before `app.exec()`:
   ```cpp
   qRegisterMetaType<std::array<int, 17>>("std::array<int, 17>");
   qRegisterMetaType<std::array<bool, 17>>("std::array<bool, 17>");
   qRegisterMetaType<std::array<double, 17>>("std::array<double, 17>");
   ```
2. **Queued Connection**: Cross-thread connections must be queued so slot invocations execute safely on the GUI thread:
   ```cpp
   connect(_mqttClient, &MqttClient::telemetryReceived,
           this, &MainWindow::onTelemetryReceived,
           Qt::QueuedConnection);
   ```

---

## 7. ASCII / Home Assistant Command Protocol

For Home Assistant and text-based tools, RoboHero accepts plain ASCII strings on `robot/robohero/<robot_id>/cmd`:

- **Motion Program Triggers**: `forward`, `backward`, `turn_left`, `turn_right`, `move_left`, `move_right`, `get_up`, `get_up_face`, `wave`, `bow`, `dance`, `iron_man`, `clap`, `warmup`, `apache`, `balance`, `goilc`, `auto`.
- **System States**: `standby`, `center`, `relax`, `zero`, `stop`.
- **Direct Program Execution**: `pm <id>` (standard motion ID), `pms <id>` (special motion ID).
