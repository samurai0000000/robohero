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
   - **Sensors**:
     - `sensor.robohero_battery_voltage` (Battery Voltage in V)
     - `sensor.robohero_battery_level` (Battery % state of charge)
     - `sensor.robohero_motion_status` (Motion status: `idle`, `moving`, `relaxed`, `voltage_low`)
   - **Binary Sensors**:
     - `binary_sensor.robohero_low_voltage_alert` (Problem alert if voltage ≤ 5.9V)
     - `binary_sensor.robohero_connectivity` (Online / Offline LWT status)
   - **Switches**:
     - `switch.robohero_power` (Controls power: ON stands up to Standby, OFF relaxes servos to rest)
   - **Buttons**:
     - Locomotion: Forward, Backward, Turn Left, Turn Right, Sidestep Left, Sidestep Right
     - Recovery: Get Up (Back), Face-Down Get Up
     - Gestures (All 10): Wave, Bow, Dance, Iron Man, Clap Hands, Warm-Up, Apache, Balance, GOILC, Auto Demo Loop
     - Safety/State: Emergency Stop, Standby / Center, Relax, Zero
   - **Controls**:
     - `select.robohero_select_motion_program` (Dropdown to trigger any motion program)
     - `number.robohero_head_pan_angle` (Head yaw angle slider: -90° to +90°)

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

## Step 4: Voice Control via Home Assistant Assist

Because all actions are standard Home Assistant `button` entities, you can immediately control RoboHero using Home Assistant Assist voice commands:

- *"Press RoboHero wave"*
- *"Press RoboHero dance"*
- *"Press RoboHero standby"*
- *"Press RoboHero relax"*
- *"What is RoboHero's battery level?"*

---

## Step 5: Voice Control with Google Home & Nest Audio

To control RoboHero using a **Google Nest Audio**, **Nest Mini**, or the **Google Assistant** app on your phone (e.g. *"Hey Google, tell robot to dance"*), follow these steps:

### 1. Create a Helper Script in Home Assistant
Because Home Assistant `button` entities are momentary actions rather than stateful switches, you create a **Script** to press the button. Home Assistant then automatically exports this script to Google Assistant as a voice **Scene**:

**Method A: Via the Visual (GUI) Editor**
1. In Home Assistant, go to **Settings** &rarr; **Automations & Scenes** &rarr; **Scripts** tab.
2. Click **+ Add Script**.
3. Name it **`Robot Dance`** (using `Robot Dance` instead of `RoboHero Dance` avoids naming conflicts with the `RoboHero` power switch).
4. Under **Sequence**, click **+ Add Action** &rarr; **Perform Action** (Call Service).
5. Choose **Button: Press** (`button.press`).
6. Select **RoboHero Dance** (`button.robohero_dance`) as the target.
7. Click **Save**.

**Method B: Via the UI YAML Editor**
1. When creating or editing a script, click the **Three Dots (`⋮`)** in the top-right corner &rarr; **Edit in YAML**.
2. Paste the following YAML:
   ```yaml
   alias: Robot Dance
   icon: mdi:music
   sequence:
     - action: button.press
       target:
         entity_id: button.robohero_dance
   ```
   *(Note: Do not include a top-level `robot_dance:` key in the UI editor, as Home Assistant manages the ID automatically).*
3. Click **Save**.

**Method C: If editing raw `scripts.yaml` directly on disk**
```yaml
robot_dance:
  alias: Robot Dance
  icon: mdi:music
  sequence:
    - action: button.press
      target:
        entity_id: button.robohero_dance
```

---

### 2. Mandatory Step: Expose the Script as a Scene to Google Assistant
> [!IMPORTANT]
> **Why Google Says "Can't find scene called Robot Dance"**:
> Newly created scripts and scenes in Home Assistant are **NOT exposed to Google Assistant by default**. Google will not know the scene exists until you turn on its exposure toggle!

1. In Home Assistant, go to **Settings** &rarr; **Voice assistants**.
2. Click the **Expose** tab at the very top of the page.
3. Search for: **`Robot Dance`** (or `dance`).
4. Find the row for `script.robot_dance` and toggle the switch under the **Google Assistant** column to **ON (Blue)**.
5. In the **Voice name** column, ensure it says **`Robot Dance`**.
6. Force Google to download the new scene by saying to your Nest Audio:
   > **"Hey Google, sync my devices"**
7. Google will chime: *"Syncing devices for Home Assistant..."*

Once synced, Google Assistant officially registers `Robot Dance` as an active scene!

> [!TIP]
> **RoboHero As a Native Device in Google Home**:
> The robot firmware automatically exposes a native Power switch (`switch.robohero_power`). In Google Home, it appears as a dedicated device named **RoboHero**!
> Once linked, you can say directly to your Nest Audio:
> - *"Hey Google, turn on RoboHero"* (Powers on and stands up to ready stance)
> - *"Hey Google, turn off RoboHero"* (Powers down and relaxes servos to rest)
>
> **Direct Voice Command for Gestures**:
> Home Assistant scripts are treated by Google Assistant as scenes! You can say to your Nest Audio:
> - *"Hey Google, activate RoboHero Dance"*
> - *"Hey Google, start RoboHero Dance"*

---

### 3. Create a Custom Phrase in Google Home (Optional)
If you want to use natural phrasing like *"Hey Google, tell robot to dance"*:

1. Open the **Google Home** app on your phone.
2. Tap the **Automations** tab at the bottom &rarr; tap the **+ Add** (or floating `+`) button.
3. If prompted to choose a type, select **Household** (or **Personal**).
4. Under **Starters** (or *"When..."*):
   - Tap **Add starter** &rarr; select **Voice command** (or **Voice**).
   - Enter your preferred phrases:
     - `tell robot to dance`
     - `make robot dance`
     - `robot dance`
5. Under **Actions** (or *"Then..."*):
   - Tap **Add action** &rarr; select **Adjust Home Devices** &rarr; choose **RoboHero Dance** (or choose **Try adding your own** / **Custom action** and enter: `activate RoboHero Dance`).
6. Tap **Save**.

Now you can say directly to your Nest Audio:
> **"Hey Google, tell robot to dance"**

Google Assistant will trigger the script in Home Assistant, which dispatches the MQTT command to RoboHero to dance!
