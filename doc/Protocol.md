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

---

## 5. Servo Channel Mapping Reference

| Channel | Joint Name | Minimum | Neutral | Maximum | Controller |
| :---: | :--- | :---: | :---: | :---: | :---: |
| 0 | `left_ankle_roll` | 1 | 135 | 270 | PCA9685 Ch 0 |
| 1 | `left_ankle_pitch` | 1 | 135 | 270 | PCA9685 Ch 1 |
| 2 | `left_knee_pitch` | 1 | 135 | 270 | PCA9685 Ch 2 |
| 3 | `left_hip_pitch` | 1 | 135 | 270 | PCA9685 Ch 3 |
| 4 | `left_hip_roll` | 1 | 135 | 270 | PCA9685 Ch 4 |
| 5 | `left_shoulder_pitch`| 1 | 135 | 270 | PCA9685 Ch 5 |
| 6 | `left_shoulder_roll` | 1 | 135 | 270 | PCA9685 Ch 6 |
| 7 | `left_elbow` | 1 | 135 | 270 | PCA9685 Ch 7 |
| 8 | `right_elbow` | 1 | 135 | 270 | PCA9685 Ch 8 |
| 9 | `right_shoulder_roll`| 1 | 135 | 270 | PCA9685 Ch 9 |
| 10 | `right_shoulder_pitch`| 1 | 135 | 270 | PCA9685 Ch 10 |
| 11 | `right_hip_roll` | 1 | 135 | 270 | PCA9685 Ch 11 |
| 12 | `right_hip_pitch` | 1 | 135 | 270 | PCA9685 Ch 12 |
| 13 | `right_knee_pitch` | 1 | 135 | 270 | PCA9685 Ch 13 |
| 14 | `right_ankle_pitch`| 1 | 135 | 270 | PCA9685 Ch 14 |
| 15 | `right_ankle_roll` | 1 | 135 | 270 | PCA9685 Ch 15 |
| 16 | `head_yaw` | 0 | 90 | 180 | ESP8266 GPIO12 |

---

## 6. ASCII / Home Assistant Command Protocol

For Home Assistant and text-based tools, RoboHero accepts plain ASCII strings on `robot/robohero/<robot_id>/cmd`:

- **Motion Program Triggers**: `forward`, `backward`, `turn_left`, `turn_right`, `move_left`, `move_right`, `get_up`, `get_up_face`, `wave`, `bow`, `dance`, `iron_man`, `clap`, `warmup`, `apache`, `balance`, `goilc`, `auto`.
- **System States**: `standby`, `center`, `relax`, `zero`, `stop`.
- **Direct Program Execution**: `pm <id>` (standard motion ID), `pms <id>` (special motion ID).
