# RoboHero Home Assistant Integration Guide

This guide provides step-by-step instructions for integrating your **RoboHero 17 DOF Biped Robot** into **Home Assistant** via MQTT.

---

## Architecture Overview

RoboHero integrates natively with Home Assistant using **MQTT Auto-Discovery**:
- **Direct Wi-Fi Connection**: The robot connects directly to your Home Assistant Mosquitto MQTT broker. No external gateways, bridge scripts, or companion containers are required.
- **Auto-Discovery**: On boot and MQTT connection, the robot automatically advertises its sensors, buttons, number slider, and select dropdown to Home Assistant (`homeassistant/.../config`).
- **Telemetry & State**: Live battery voltage, battery percentage, low-voltage lockout alerts, and motion states are published to `robot/robohero/<id>/state`.
- **Availability (LWT)**: Home Assistant automatically tracks whether the robot is online or offline via MQTT Last Will and Testament.

---

## Step 1: Configure RoboHero Wi-Fi and MQTT

To connect RoboHero to your Home Assistant MQTT broker:

### Method A: Via Web UI
1. Power on RoboHero. If not connected to your home Wi-Fi, it will broadcast a fallback AP SSID named `TTR-XXXX` (password `12345678`).
2. Connect your computer/phone to the `TTR-XXXX` Wi-Fi network and navigate in your browser to:
   ```
   http://192.168.4.1/
   ```
3. Enter your home Wi-Fi network SSID and Password.
4. Set the **MQTT Host** to your Home Assistant IP address (e.g. `192.168.1.100`) and **MQTT Port** to `1883`.
5. If your Mosquitto broker requires authentication, fill in the **MQTT User** and **MQTT Password**.
6. Save and reboot.

### Method B: Via Serial Shell (USB Console)
Connect a USB-to-UART cable at 115200 baud and issue:
```text
config set wifi.mode 0
config set wifi.sta.ssid "YourHomeWiFi"
config set wifi.sta.pass "YourWiFiPassword"
config set mqtt.host "192.168.1.100"
config set mqtt.port 1883
config set mqtt.user "homeassistant"
config set mqtt.pass "YourMqttPassword"
config save
reboot
```

---

## Step 2: Verify Auto-Discovery in Home Assistant

1. In Home Assistant, open **Settings** &rarr; **Devices & Services**.
2. Select the **MQTT** integration card.
3. You will see **RoboHero** listed under **Devices**.
4. Click on **RoboHero** to inspect the discovered entities:
    - **Switches & Controls**:
      - `switch.robohero_power` (RoboHero Power: ON stands up to Standby, OFF relaxes servos to rest)
      - `number.robohero_head_pan_angle` (Head Pan Angle slider: -90° to +90°)
      - `select.robohero_select_motion_program` (Select Motion Program dropdown)
    - **Sensors**:
      - `sensor.robohero_battery_voltage` (Battery Voltage in V)
      - `sensor.robohero_battery_level` (Battery Level in %)
      - `sensor.robohero_motion_status` (Motion Status: `idle`, `moving`, `relaxed`, `voltage_low`)
    - **Binary Sensors (Diagnostics)**:
      - `binary_sensor.robohero_connectivity` (Connectivity: `online` / `offline` via LWT)
      - `binary_sensor.robohero_low_voltage_alert` (Low Voltage Alert: `problem` when ≤ 5.9V)
    - **Buttons (22 Total)**:
      - **Safety & Pose**:
        - `button.robohero_emergency_stop` (Emergency Stop)
        - `button.robohero_standby` (Standby)
        - `button.robohero_relax` (Relax)
        - `button.robohero_zero_pose` (Zero Pose)
      - **Locomotion**:
        - `button.robohero_forward` (Forward)
        - `button.robohero_backward` (Backward)
        - `button.robohero_turn_left` (Turn Left)
        - `button.robohero_turn_right` (Turn Right)
        - `button.robohero_move_left` (Move Left)
        - `button.robohero_move_right` (Move Right)
      - **Recovery**:
        - `button.robohero_get_up_back` (Get Up Back)
        - `button.robohero_face_down_get_up` (Face-Down Get Up)
      - **Gestures & Routines**:
        - `button.robohero_dance` (Dance)
        - `button.robohero_bow` (Bow)
        - `button.robohero_wave` (Wave)
        - `button.robohero_iron_man` (Iron Man)
        - `button.robohero_apache` (Apache)
        - `button.robohero_balance` (Balance)
        - `button.robohero_warm_up` (Warm-Up)
        - `button.robohero_clap` (Clap)
        - `button.robohero_goilc` (GOILC)
        - `button.robohero_auto_demo_loop` (Auto Demo Loop)

---

## Step 3: Add the Lovelace Dashboard Card

A pre-styled, modular Lovelace dashboard card is provided in [dashboard_card.yaml](dashboard_card.yaml).

Home Assistant uses the modern **Sections Layout**. Follow these exact steps to add the card:

### In Your New "RoboHero" Dashboard:
1. In the left sidebar, click on **RoboHero**.
2. Click the **Pencil icon (`✎`)** in the top-right corner to enter Edit Mode (as shown in your screenshot).
3. Under the box labeled **"New section"**, click the dashed box with the **`+`** icon inside it.
4. The **Card Picker** dialog will pop up. Scroll all the way down to the bottom and select **Manual**.
5. Delete any template text in the editor, paste the full contents of [dashboard_card.yaml](dashboard_card.yaml), and click **Save**.
6. Click the blue **Done** button in the top-right corner to exit Edit Mode.

---

### Alternative: Raw Configuration Editor
If you prefer pasting the entire dashboard view directly:
1. While in Edit Mode (pencil icon active), click the **Three Dots (`⋮`)** in the top-right corner (next to the `?` button and `Done` button).
2. Select **Raw configuration editor**.
3. You can paste or inspect your dashboard views directly.
4. Click **Save** and close the editor.

---

### Alternative Method: Add RoboHero to the Overview Page
If you prefer adding RoboHero to your existing Overview page:
1. In the **Edit Overview page** dialog (shown when clicking the pencil icon on Overview):
   - Under **Favorite entities**, click **+ Add favorite**.
   - Search for `RoboHero` and select the buttons and sensors you wish to access quickly on your home screen.
   - Click **Save**.

The dashboard card gives you:
- Live battery gauge with colored alert levels.
- Full 3x3 locomotion D-pad for walking and steering.
- One-click gesture buttons (Wave, Bow, Dance, Iron Man, Clap, Warm-Up).
- Self-righting recovery buttons.
- Emergency STOP button.
- Head pan slider for looking left and right.

---

## Step 4: Adding All Gesture Routines to Home Assistant

Home Assistant `button` entities are momentary actions. To control routines by voice or through automations, they are exposed as Home Assistant **Scripts**.

You can add all gestures at once using either the **Home Assistant UI (Single Script with all gestures)** or by pasting into **`scripts.yaml`**.

---

### Method A: Single Script via Home Assistant UI (Latest Version)

This adds **one script** that contains all 12 routines in a dropdown selector directly through the Home Assistant interface (no file editing required).

#### 1. Open the Script Editor in Home Assistant
1. In the left sidebar, click **Settings**.
2. Click **Automations & scenes**.
3. At the top of the page, click the **Scripts** tab.
4. Click the blue **+ Add script** button in the bottom-right corner.

#### 2. Switch to YAML and Paste the Script
1. In the top-right corner of the editor, click the **Three Vertical Dots (`⋮`)** menu.
2. Select **Edit in YAML**.
3. Replace all existing text in the editor with this complete script containing every gesture:

```yaml
alias: RoboHero Routines
icon: mdi:robot
description: "Execute any RoboHero gesture or recovery routine"
fields:
  routine:
    name: Routine
    description: "Choose routine to perform"
    required: true
    selector:
      select:
        options:
          - label: "Dance"
            value: "button.robohero_dance"
          - label: "Bow"
            value: "button.robohero_bow"
          - label: "Wave"
            value: "button.robohero_wave"
          - label: "Iron Man"
            value: "button.robohero_iron_man"
          - label: "Apache"
            value: "button.robohero_apache"
          - label: "Balance"
            value: "button.robohero_balance"
          - label: "Warm-Up"
            value: "button.robohero_warm_up"
          - label: "Clap"
            value: "button.robohero_clap"
          - label: "GOILC"
            value: "button.robohero_goilc"
          - label: "Auto Demo Loop"
            value: "button.robohero_auto_demo_loop"
          - label: "Get Up (Back)"
            value: "button.robohero_get_up_back"
          - label: "Face-Down Get Up"
            value: "button.robohero_face_down_get_up"
sequence:
  - action: button.press
    target:
      entity_id: "{{ routine }}"
```

4. Click the blue **Save** button in the bottom-right corner.
5. To test it immediately: Click the **Three Vertical Dots (`⋮`)** in the top-right corner (next to **Traces**) &rarr; click **Run**. A dialog will open where you can select any routine from the dropdown and watch RoboHero execute it!

---

### Method B: All Individual Routine Scripts via `scripts.yaml`

If you want direct individual voice commands in Google Assistant for each routine (e.g. *"activate Robot Apache"*, *"activate Robot Bow"*), add this block containing all gestures to your `scripts.yaml`:

#### 1. Open `scripts.yaml` in Home Assistant OS 2026
In Home Assistant OS, system files are edited using the **File Editor** add-on:
1. Go to **Settings** &rarr; **System** &rarr; **Add-ons**.
   *(Direct URL: `http://<your-ha-host>:8123/hassio/dashboard`)*.
2. Click the blue **Add-on Store** button in the bottom-right corner.
3. Search for **File editor** &rarr; click **INSTALL** &rarr; turn ON **Show in sidebar** &rarr; click **START**.
4. Click **File editor** in your left sidebar &rarr; click the **Folder icon (`📁`)** in the top toolbar &rarr; open **`scripts.yaml`**.
5. Paste the complete block below at the end of the file and click the **Save icon (`💾`)**:

```yaml
# ==============================================================================
# RoboHero Gesture & Action Routines (All Gestures)
# ==============================================================================

robot_dance:
  alias: Robot Dance
  icon: mdi:music
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_dance

robot_bow:
  alias: Robot Bow
  icon: mdi:human-greeting
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_bow

robot_wave:
  alias: Robot Wave
  icon: mdi:hand-wave
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_wave

robot_iron_man:
  alias: Robot Iron Man
  icon: mdi:shield-star
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_iron_man

robot_apache:
  alias: Robot Apache
  icon: mdi:karate
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_apache

robot_balance:
  alias: Robot Balance
  icon: mdi:scale-balance
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_balance

robot_warmup:
  alias: Robot Warmup
  icon: mdi:run
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_warm_up

robot_clap:
  alias: Robot Clap
  icon: mdi:hand-clap
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_clap

robot_goilc:
  alias: Robot Goilc
  icon: mdi:robot-happy
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_goilc

robot_auto_demo:
  alias: Robot Auto Demo
  icon: mdi:play-circle-outline
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_auto_demo_loop

robot_get_up_back:
  alias: Robot Get Up
  icon: mdi:human-handsup
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_get_up_back

robot_get_up_front:
  alias: Robot Face Up
  icon: mdi:human-handsdown
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_face_down_get_up
```

#### 2. Reload Scripts in Home Assistant 2026:
1. In the left sidebar, click **Tools** (the **`>_`** icon at `/config/tools/yaml`).
2. On the **YAML** tab, under **YAML configuration reloading**:
   - Click **All YAML configuration** (or scroll down and click **Scripts**).
3. All routines will immediately appear in **Settings** &rarr; **Automations & scenes** &rarr; **Scripts**.

---

## Step 5: Expose to Google Assistant / Nest Audio

To control the routines with your Google Nest Audio or Google Assistant:

> [!IMPORTANT]
> **Why New Scripts Do Not Appear in the Expose Table Automatically**:
> The table under **Settings &rarr; Voice assistants &rarr; Expose** (`/config/voice-assistants/expose`) only lists entities that have *already* been exposed. Newly created scripts are unexposed by default and will **not** appear in that table until you explicitly add them using the **`+ Expose entity`** button.

### How to Expose Your Routine Scripts:
1. In the left sidebar, click **Settings** &rarr; **Voice assistants**.
2. Click the **Expose** tab at the top.
3. Look at the bottom-right corner and click the blue **`+ Expose entity`** button.
4. In the search box that pops up, type **`Robot`** to list all routine scripts.
5. Select the scripts (`Robot Dance`, `Robot Apache`, `Robot Bow`, `Robot Wave`, etc.) and click **Expose**.
6. Ensure the toggle under the **Google Assistant** column is switched **ON** (blue).
7. Say to your Nest Audio:
   > **"Hey Google, sync my devices"**

Google will chime *"Syncing devices for Home Assistant..."* and confirm all exposed routines are ready.

---

## Voice Commands Reference

| Voice Command | Action Triggered |
| :--- | :--- |
| *"Hey Google, turn on RoboHero"* | Powers up robot and stands in ready stance (`standby`) |
| *"Hey Google, turn off RoboHero"* | Relaxes servos to rest/sleep (`relax`) |
| *"Hey Google, activate Robot Dance"* | Executes Dance routine |
| *"Hey Google, activate Robot Bow"* | Executes Bow greeting |
| *"Hey Google, activate Robot Wave"* | Executes Waving gesture |
| *"Hey Google, activate Robot Iron Man"* | Executes Iron Man combat pose |
| *"Hey Google, activate Robot Apache"* | Executes Apache martial arts routine |
| *"Hey Google, activate Robot Balance"* | Executes Balance stunt |
| *"Hey Google, activate Robot Warmup"* | Executes Warm-Up stretching routine |
| *"Hey Google, activate Robot Clap"* | Executes Hand Clapping routine |
| *"Hey Google, activate Robot Goilc"* | Executes GOILC dynamic routine |
| *"Hey Google, activate Robot Auto Demo"* | Starts continuous auto demo showcase |
| *"Hey Google, activate Robot Get Up"* | Self-rights from back fall |
| *"Hey Google, activate Robot Face Up"* | Self-rights from stomach fall |


