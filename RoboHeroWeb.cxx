/*
 * RoboHeroWeb.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "RoboHeroWeb.hxx"
#include "RoboHeroApp.hxx"
#include "RoboHeroEeprom.hxx"

static const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RoboHero Controller</title>
<style>
:root {
  --bg-primary: #0f172a;
  --bg-card: rgba(30, 41, 59, 0.75);
  --bg-card-hover: rgba(51, 65, 85, 0.85);
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --accent-cyan: #06b6d4;
  --accent-blue: #3b82f6;
  --accent-pink: #ec4899;
  --accent-green: #10b981;
  --accent-amber: #f59e0b;
  --border: rgba(255, 255, 255, 0.1);
  --radius: 12px;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: radial-gradient(circle at top, #1e293b 0%, #0f172a 100%);
  color: var(--text-main);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
  min-height: 100vh;
  padding: 16px;
  display: flex;
  flex-direction: column;
  align-items: center;
}
.navbar {
  width: 100%;
  max-width: 600px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 18px;
  background: var(--bg-card);
  backdrop-filter: blur(12px);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin-bottom: 20px;
}
.brand {
  font-size: 1.25rem;
  font-weight: 700;
  background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  display: flex;
  align-items: center;
  gap: 8px;
}
.nav-link {
  color: var(--text-main);
  background: linear-gradient(135deg, rgba(6, 182, 212, 0.2), rgba(59, 130, 246, 0.2));
  border: 1px solid var(--accent-cyan);
  padding: 8px 14px;
  border-radius: 8px;
  text-decoration: none;
  font-size: 0.9rem;
  font-weight: 600;
  transition: all 0.2s ease;
}
.nav-link:hover {
  background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
  color: #fff;
  box-shadow: 0 0 15px rgba(6, 182, 212, 0.5);
}
.container {
  width: 100%;
  max-width: 600px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}
.card {
  background: var(--bg-card);
  backdrop-filter: blur(12px);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 18px;
}
.card-title {
  font-size: 0.95rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--text-muted);
  margin-bottom: 12px;
  font-weight: 600;
}
.dpad-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 10px;
}
.btn {
  background: var(--bg-card);
  border: 1px solid var(--border);
  color: var(--text-main);
  padding: 14px 10px;
  font-size: 0.95rem;
  font-weight: 600;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.15s ease;
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  user-select: none;
}
.btn:hover { background: var(--bg-card-hover); border-color: rgba(255, 255, 255, 0.2); }
.btn:active { transform: scale(0.97); }
.btn-primary { background: linear-gradient(135deg, #2563eb, #1d4ed8); border: none; }
.btn-primary:hover { background: linear-gradient(135deg, #3b82f6, #2563eb); }
.btn-standby { background: linear-gradient(135deg, #db2777, #be185d); border: none; font-weight: 700; }
.btn-standby:hover { background: linear-gradient(135deg, #ec4899, #db2777); }
.btn-action { background: linear-gradient(135deg, rgba(245, 158, 11, 0.2), rgba(217, 119, 6, 0.2)); border-color: var(--accent-amber); color: #fde68a; }
.btn-action:hover { background: linear-gradient(135deg, var(--accent-amber), #d97706); color: #000; }
.btn-auto { background: linear-gradient(135deg, #059669, #047857); border: none; font-weight: 700; grid-column: span 2; }
.btn-auto:hover { background: linear-gradient(135deg, #10b981, #059669); }
.actions-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 10px;
}
.toast {
  position: fixed;
  bottom: 20px;
  background: rgba(16, 185, 129, 0.9);
  color: #fff;
  padding: 10px 20px;
  border-radius: 8px;
  font-weight: 600;
  opacity: 0;
  transform: translateY(20px);
  transition: all 0.3s ease;
  pointer-events: none;
}
.toast.show { opacity: 1; transform: translateY(0); }
</style>
</head>
<body>
<div class="navbar">
  <div class="brand">
    <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
    RoboHero
  </div>
  <a href="/calibrate" class="nav-link" id="nav-calibrate">PWM Calibration</a>
</div>
<div class="container">
  <div class="card">
    <div class="card-title">Locomotion</div>
    <div class="dpad-grid">
      <button class="btn btn-primary" id="btn-turn-left" onclick="sendCmd('pm', 3)">Turn Left</button>
      <button class="btn btn-primary" id="btn-forward" onclick="sendCmd('pm', 1)">Forward</button>
      <button class="btn btn-primary" id="btn-turn-right" onclick="sendCmd('pm', 4)">Turn Right</button>
      <button class="btn btn-primary" id="btn-move-left" onclick="sendCmd('pm', 5)">Move Left</button>
      <button class="btn btn-standby" id="btn-standby" onclick="sendCmd('pm', 99)">STANDBY</button>
      <button class="btn btn-primary" id="btn-move-right" onclick="sendCmd('pm', 6)">Move Right</button>
      <div></div>
      <button class="btn btn-primary" id="btn-backward" onclick="sendCmd('pm', 2)">Backward</button>
      <div></div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">Recovery</div>
    <div class="actions-grid">
      <button class="btn btn-primary" id="btn-get-up" onclick="sendCmd('pm', 11)">Get Up</button>
      <button class="btn btn-primary" id="btn-get-up-face" onclick="sendCmd('pm', 12)">Face-Down Get Up</button>
    </div>
  </div>

  <div class="card">
    <div class="card-title">Actions & Gestures</div>
    <div class="actions-grid">
      <button class="btn btn-action" id="btn-bow" onclick="sendCmd('pms', 1)">Bow</button>
      <button class="btn btn-action" id="btn-apache" onclick="sendCmd('pms', 4)">Apache</button>
      <button class="btn btn-action" id="btn-waving" onclick="sendCmd('pms', 2)">Waving</button>
      <button class="btn btn-action" id="btn-balance" onclick="sendCmd('pms', 5)">Balance</button>
      <button class="btn btn-action" id="btn-ironman" onclick="sendCmd('pms', 3)">Iron Man</button>
      <button class="btn btn-action" id="btn-warmup" onclick="sendCmd('pms', 6)">Warm-Up</button>
      <button class="btn btn-action" id="btn-clap" onclick="sendCmd('pms', 7)">Clap Hands</button>
      <button class="btn btn-action" id="btn-goilc" onclick="sendCmd('pms', 8)">GOILC</button>
      <button class="btn btn-action" id="btn-dance" onclick="sendCmd('pms', 9)">Dance</button>
      <button class="btn btn-auto" id="btn-auto" onclick="sendCmd('pms', 99)">Auto Demo Loop</button>
    </div>
  </div>
</div>
<div id="toast" class="toast">Command sent</div>
<script>
function sendCmd(key, val) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/?' + key + '=' + val + '&_t=' + Date.now(), true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      showToast(xhr.status === 200 ? 'Command executed' : 'Command sent');
    }
  };
  xhr.send();
}
function showToast(msg) {
  var t = document.getElementById('toast');
  t.innerText = msg;
  t.classList.add('show');
  setTimeout(function() { t.classList.remove('show'); }, 1500);
}
</script>
</body>
</html>
)rawliteral";

static const char PAGE_CALIBRATE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<title>RoboHero PWM Calibration</title>
<style>
:root {
  --bg-primary: #0f172a;
  --bg-card: rgba(30, 41, 59, 0.75);
  --bg-card-hover: rgba(51, 65, 85, 0.85);
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --accent-cyan: #06b6d4;
  --accent-blue: #3b82f6;
  --accent-emerald: #10b981;
  --accent-pink: #ec4899;
  --accent-amber: #f59e0b;
  --border: rgba(255, 255, 255, 0.1);
  --radius: 12px;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: radial-gradient(circle at top, #1e293b 0%, #0f172a 100%);
  color: var(--text-main);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
  min-height: 100vh;
  padding: 16px;
  display: flex;
  flex-direction: column;
  align-items: center;
}
.navbar {
  width: 100%;
  max-width: 900px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 18px;
  background: var(--bg-card);
  backdrop-filter: blur(12px);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin-bottom: 20px;
  position: sticky;
  top: 16px;
  z-index: 100;
}
.brand {
  font-size: 1.25rem;
  font-weight: 700;
  background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.nav-actions { display: flex; gap: 10px; align-items: center; }
.nav-link {
  color: var(--text-muted);
  text-decoration: none;
  font-size: 0.9rem;
  font-weight: 600;
  padding: 8px 12px;
  border-radius: 8px;
  transition: all 0.2s ease;
}
.nav-link:hover { color: var(--text-main); background: rgba(255, 255, 255, 0.05); }
.btn-save {
  background: linear-gradient(135deg, #10b981, #059669);
  color: #fff;
  border: none;
  padding: 9px 18px;
  border-radius: 8px;
  font-weight: 700;
  font-size: 0.95rem;
  cursor: pointer;
  box-shadow: 0 0 15px rgba(16, 185, 129, 0.4);
  transition: all 0.2s ease;
  display: flex;
  align-items: center;
  gap: 6px;
}
.btn-save:hover {
  background: linear-gradient(135deg, #34d399, #10b981);
  box-shadow: 0 0 20px rgba(16, 185, 129, 0.7);
  transform: translateY(-1px);
}
.btn-save:active { transform: translateY(1px); }
.container {
  width: 100%;
  max-width: 900px;
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}
@media (max-width: 768px) {
  .container { grid-template-columns: 1fr; }
}
.card {
  background: var(--bg-card);
  backdrop-filter: blur(12px);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
}
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid var(--border);
  padding-bottom: 8px;
  margin-bottom: 12px;
}
.card-title {
  font-size: 0.95rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--accent-cyan);
}
.pose-bar {
  grid-column: 1 / -1;
  display: flex;
  gap: 10px;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
}
.btn-pose {
  background: rgba(255, 255, 255, 0.08);
  border: 1px solid var(--border);
  color: var(--text-main);
  padding: 8px 14px;
  border-radius: 8px;
  font-size: 0.85rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
}
.btn-pose:hover { background: rgba(255, 255, 255, 0.15); border-color: rgba(255, 255, 255, 0.3); }
.slider-group {
  display: flex;
  flex-direction: column;
  gap: 14px;
}
.slider-row {
  display: flex;
  flex-direction: column;
  gap: 6px;
  background: rgba(15, 23, 42, 0.4);
  padding: 10px;
  border-radius: 8px;
  border: 1px solid rgba(255, 255, 255, 0.03);
}
.slider-meta {
  display: flex;
  justify-content: space-between;
  font-size: 0.85rem;
}
.servo-name { font-weight: 600; color: #e2e8f0; }
.servo-val {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-weight: 700;
  padding: 2px 6px;
  border-radius: 4px;
  background: rgba(6, 182, 212, 0.15);
  color: var(--accent-cyan);
  min-width: 44px;
  text-align: right;
}
.slider-controls {
  display: flex;
  align-items: center;
  gap: 8px;
}
.btn-step {
  background: rgba(255, 255, 255, 0.08);
  border: 1px solid var(--border);
  color: var(--text-main);
  width: 28px;
  height: 28px;
  border-radius: 6px;
  font-size: 1rem;
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
}
.btn-step:hover { background: rgba(255, 255, 255, 0.2); }
.btn-zero {
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid var(--border);
  color: var(--text-muted);
  padding: 0 6px;
  height: 28px;
  border-radius: 6px;
  font-size: 0.75rem;
  font-weight: 600;
  cursor: pointer;
}
.btn-zero:hover { color: #fff; background: rgba(236, 72, 153, 0.3); border-color: var(--accent-pink); }
input[type="range"] {
  -webkit-appearance: none;
  appearance: none;
  flex: 1;
  height: 6px;
  background: #334155;
  border-radius: 3px;
  outline: none;
}
input[type="range"]::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: var(--accent-cyan);
  cursor: pointer;
  box-shadow: 0 0 8px var(--accent-cyan);
  transition: all 0.15s ease;
}
input[type="range"]::-webkit-slider-thumb:hover {
  transform: scale(1.2);
  background: #fff;
}
.footer-bar {
  grid-column: 1 / -1;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px;
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin-top: 10px;
}
.toast {
  position: fixed;
  bottom: 20px;
  background: rgba(16, 185, 129, 0.95);
  color: #fff;
  padding: 12px 24px;
  border-radius: 8px;
  font-weight: 700;
  box-shadow: 0 5px 20px rgba(0,0,0,0.5);
  opacity: 0;
  transform: translateY(20px);
  transition: all 0.3s ease;
  z-index: 1000;
}
.toast.show { opacity: 1; transform: translateY(0); }
</style>
</head>
<body>
<div class="navbar">
  <div class="brand">PWM Trim Calibration</div>
  <div class="nav-actions">
    <a href="/" class="nav-link">Controller</a>
    <button id="btn-top-save" class="btn-save" onclick="saveToEeprom()">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"/></svg>
      Save to EEPROM
    </button>
  </div>
</div>

<div class="container">
  <div class="card pose-bar">
    <span style="font-weight:600; color:var(--text-muted); font-size:0.9rem;">Quick Pose Check:</span>
    <div style="display:flex; gap:8px;">
      <button class="btn-pose" id="btn-pose-center" onclick="previewPose('center')">Standby Pose</button>
      <button class="btn-pose" id="btn-pose-zero" onclick="previewPose('zero')">Zero Alignment Pose</button>
      <button class="btn-pose" id="btn-reset-all" style="color:#f43f5e;" onclick="resetAllZero()">Reset All Trims to 0</button>
      <button class="btn-pose" id="btn-reload" onclick="loadTrims()">Reload EEPROM</button>
    </div>
  </div>

  <!-- Right Arm -->
  <div class="card">
    <div class="card-header">
      <span class="card-title">Right Arm</span>
    </div>
    <div class="slider-group">
      <div class="slider-row" id="row_10">
        <div class="slider-meta"><span class="servo-name">Servo 10 - Shoulder Pitch</span><span class="servo-val" id="val_10">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(10, -1)">-</button>
          <input type="range" id="trim_10" min="-125" max="125" value="0" oninput="updateVal(10, this.value)">
          <button class="btn-step" onclick="stepVal(10, 1)">+</button>
          <button class="btn-zero" onclick="setZero(10)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_9">
        <div class="slider-meta"><span class="servo-name">Servo 9 - Shoulder Roll</span><span class="servo-val" id="val_9">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(9, -1)">-</button>
          <input type="range" id="trim_9" min="-125" max="125" value="0" oninput="updateVal(9, this.value)">
          <button class="btn-step" onclick="stepVal(9, 1)">+</button>
          <button class="btn-zero" onclick="setZero(9)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_8">
        <div class="slider-meta"><span class="servo-name">Servo 8 - Elbow</span><span class="servo-val" id="val_8">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(8, -1)">-</button>
          <input type="range" id="trim_8" min="-125" max="125" value="0" oninput="updateVal(8, this.value)">
          <button class="btn-step" onclick="stepVal(8, 1)">+</button>
          <button class="btn-zero" onclick="setZero(8)">0</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Left Arm -->
  <div class="card">
    <div class="card-header">
      <span class="card-title">Left Arm</span>
    </div>
    <div class="slider-group">
      <div class="slider-row" id="row_5">
        <div class="slider-meta"><span class="servo-name">Servo 5 - Shoulder Pitch</span><span class="servo-val" id="val_5">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(5, -1)">-</button>
          <input type="range" id="trim_5" min="-125" max="125" value="0" oninput="updateVal(5, this.value)">
          <button class="btn-step" onclick="stepVal(5, 1)">+</button>
          <button class="btn-zero" onclick="setZero(5)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_6">
        <div class="slider-meta"><span class="servo-name">Servo 6 - Shoulder Roll</span><span class="servo-val" id="val_6">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(6, -1)">-</button>
          <input type="range" id="trim_6" min="-125" max="125" value="0" oninput="updateVal(6, this.value)">
          <button class="btn-step" onclick="stepVal(6, 1)">+</button>
          <button class="btn-zero" onclick="setZero(6)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_7">
        <div class="slider-meta"><span class="servo-name">Servo 7 - Elbow</span><span class="servo-val" id="val_7">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(7, -1)">-</button>
          <input type="range" id="trim_7" min="-125" max="125" value="0" oninput="updateVal(7, this.value)">
          <button class="btn-step" onclick="stepVal(7, 1)">+</button>
          <button class="btn-zero" onclick="setZero(7)">0</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Right Leg -->
  <div class="card">
    <div class="card-header">
      <span class="card-title">Right Leg</span>
    </div>
    <div class="slider-group">
      <div class="slider-row" id="row_11">
        <div class="slider-meta"><span class="servo-name">Servo 11 - Hip Roll</span><span class="servo-val" id="val_11">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(11, -1)">-</button>
          <input type="range" id="trim_11" min="-125" max="125" value="0" oninput="updateVal(11, this.value)">
          <button class="btn-step" onclick="stepVal(11, 1)">+</button>
          <button class="btn-zero" onclick="setZero(11)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_12">
        <div class="slider-meta"><span class="servo-name">Servo 12 - Hip Pitch</span><span class="servo-val" id="val_12">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(12, -1)">-</button>
          <input type="range" id="trim_12" min="-125" max="125" value="0" oninput="updateVal(12, this.value)">
          <button class="btn-step" onclick="stepVal(12, 1)">+</button>
          <button class="btn-zero" onclick="setZero(12)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_13">
        <div class="slider-meta"><span class="servo-name">Servo 13 - Knee</span><span class="servo-val" id="val_13">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(13, -1)">-</button>
          <input type="range" id="trim_13" min="-125" max="125" value="0" oninput="updateVal(13, this.value)">
          <button class="btn-step" onclick="stepVal(13, 1)">+</button>
          <button class="btn-zero" onclick="setZero(13)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_14">
        <div class="slider-meta"><span class="servo-name">Servo 14 - Ankle Pitch</span><span class="servo-val" id="val_14">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(14, -1)">-</button>
          <input type="range" id="trim_14" min="-125" max="125" value="0" oninput="updateVal(14, this.value)">
          <button class="btn-step" onclick="stepVal(14, 1)">+</button>
          <button class="btn-zero" onclick="setZero(14)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_15">
        <div class="slider-meta"><span class="servo-name">Servo 15 - Ankle Roll</span><span class="servo-val" id="val_15">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(15, -1)">-</button>
          <input type="range" id="trim_15" min="-125" max="125" value="0" oninput="updateVal(15, this.value)">
          <button class="btn-step" onclick="stepVal(15, 1)">+</button>
          <button class="btn-zero" onclick="setZero(15)">0</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Left Leg -->
  <div class="card">
    <div class="card-header">
      <span class="card-title">Left Leg</span>
    </div>
    <div class="slider-group">
      <div class="slider-row" id="row_4">
        <div class="slider-meta"><span class="servo-name">Servo 4 - Hip Roll</span><span class="servo-val" id="val_4">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(4, -1)">-</button>
          <input type="range" id="trim_4" min="-125" max="125" value="0" oninput="updateVal(4, this.value)">
          <button class="btn-step" onclick="stepVal(4, 1)">+</button>
          <button class="btn-zero" onclick="setZero(4)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_3">
        <div class="slider-meta"><span class="servo-name">Servo 3 - Hip Pitch</span><span class="servo-val" id="val_3">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(3, -1)">-</button>
          <input type="range" id="trim_3" min="-125" max="125" value="0" oninput="updateVal(3, this.value)">
          <button class="btn-step" onclick="stepVal(3, 1)">+</button>
          <button class="btn-zero" onclick="setZero(3)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_2">
        <div class="slider-meta"><span class="servo-name">Servo 2 - Knee</span><span class="servo-val" id="val_2">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(2, -1)">-</button>
          <input type="range" id="trim_2" min="-125" max="125" value="0" oninput="updateVal(2, this.value)">
          <button class="btn-step" onclick="stepVal(2, 1)">+</button>
          <button class="btn-zero" onclick="setZero(2)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_1">
        <div class="slider-meta"><span class="servo-name">Servo 1 - Ankle Pitch</span><span class="servo-val" id="val_1">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(1, -1)">-</button>
          <input type="range" id="trim_1" min="-125" max="125" value="0" oninput="updateVal(1, this.value)">
          <button class="btn-step" onclick="stepVal(1, 1)">+</button>
          <button class="btn-zero" onclick="setZero(1)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_0">
        <div class="slider-meta"><span class="servo-name">Servo 0 - Ankle Roll</span><span class="servo-val" id="val_0">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(0, -1)">-</button>
          <input type="range" id="trim_0" min="-125" max="125" value="0" oninput="updateVal(0, this.value)">
          <button class="btn-step" onclick="stepVal(0, 1)">+</button>
          <button class="btn-zero" onclick="setZero(0)">0</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Head & System Tuning -->
  <div class="card" style="grid-column: 1 / -1;">
    <div class="card-header">
      <span class="card-title">Head & System Parameters</span>
    </div>
    <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap:12px;">
      <div class="slider-row" id="row_16">
        <div class="slider-meta"><span class="servo-name">GPIO 12 Head Servo (16)</span><span class="servo-val" id="val_16">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(16, -1)">-</button>
          <input type="range" id="trim_16" min="-125" max="125" value="0" oninput="updateVal(16, this.value)">
          <button class="btn-step" onclick="stepVal(16, 1)">+</button>
          <button class="btn-zero" onclick="setZero(16)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_18">
        <div class="slider-meta"><span class="servo-name">PWM Frequency Trim (18)</span><span class="servo-val" id="val_18">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(18, -1)">-</button>
          <input type="range" id="trim_18" min="-125" max="125" value="0" oninput="updateVal(18, this.value)">
          <button class="btn-step" onclick="stepVal(18, 1)">+</button>
          <button class="btn-zero" onclick="setZero(18)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_17">
        <div class="slider-meta"><span class="servo-name">Delay Time Trim (17)</span><span class="servo-val" id="val_17">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(17, -1)">-</button>
          <input type="range" id="trim_17" min="-125" max="125" value="0" oninput="updateVal(17, this.value)">
          <button class="btn-step" onclick="stepVal(17, 1)">+</button>
          <button class="btn-zero" onclick="setZero(17)">0</button>
        </div>
      </div>
      <div class="slider-row" id="row_19">
        <div class="slider-meta"><span class="servo-name">Voltage Cal Offset (19)</span><span class="servo-val" id="val_19">0</span></div>
        <div class="slider-controls">
          <button class="btn-step" onclick="stepVal(19, -1)">-</button>
          <input type="range" id="trim_19" min="-125" max="125" value="0" oninput="updateVal(19, this.value)">
          <button class="btn-step" onclick="stepVal(19, 1)">+</button>
          <button class="btn-zero" onclick="setZero(19)">0</button>
        </div>
      </div>
    </div>
  </div>

  <div class="footer-bar">
    <span style="color:var(--text-muted); font-size:0.85rem;">Adjustments apply immediately in real-time. Click Save to write to EEPROM.</span>
    <button id="btn-bottom-save" class="btn-save" onclick="saveToEeprom()">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"/></svg>
      Save to EEPROM
    </button>
  </div>
</div>
<div id="toast" class="toast">Saved to EEPROM</div>

<script>
var liveTimer = null;
var lastSentVal = {};

function updateVal(id, val) {
  var el = document.getElementById('val_' + id);
  if (el) el.innerText = (val > 0 ? '+' : '') + val;

  if (lastSentVal[id] !== val) {
    if (liveTimer) clearTimeout(liveTimer);
    liveTimer = setTimeout(function() {
      lastSentVal[id] = val;
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/calibrate?apply=1&key=' + id + '&val=' + val + '&_t=' + Date.now(), true);
      xhr.send();
    }, 25);
  }
}

function stepVal(id, delta) {
  var input = document.getElementById('trim_' + id);
  if (input) {
    var v = parseInt(input.value) + delta;
    if (v < -125) v = -125;
    if (v > 125) v = 125;
    input.value = v;
    updateVal(id, v);
  }
}

function setZero(id) {
  var input = document.getElementById('trim_' + id);
  if (input) {
    input.value = 0;
    updateVal(id, 0);
  }
}

function resetAllZero() {
  if (confirm('Reset all trims to 0?')) {
    for (var i = 0; i <= 19; i++) {
      setZero(i);
    }
    showToast('All sliders reset to 0.');
  }
}

function applyTrims(trims) {
  if (trims && trims.length) {
    trims.forEach(function(val, id) {
      var input = document.getElementById('trim_' + id);
      if (input) {
        input.value = val;
        lastSentVal[id] = val;
        var el = document.getElementById('val_' + id);
        if (el) el.innerText = (val > 0 ? '+' : '') + val;
      }
    });
  }
}

function loadTrims() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/calibrate?json=1&_t=' + Date.now(), true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4 && xhr.status === 200) {
      try {
        var data = JSON.parse(xhr.responseText);
        if (data && data.trims) {
          applyTrims(data.trims);
        }
      } catch (e) {}
    }
  };
  xhr.send();
}

function saveToEeprom() {
  var qs = 'save=1';
  for (var i = 0; i <= 19; i++) {
    var input = document.getElementById('trim_' + i);
    qs += '&t' + i + '=' + (input ? encodeURIComponent(input.value) : '0');
  }

  var btnTop = document.getElementById('btn-top-save');
  var btnBottom = document.getElementById('btn-bottom-save');
  if (btnTop) btnTop.innerText = 'Saving...';
  if (btnBottom) btnBottom.innerText = 'Saving...';

  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/calibrate?' + qs + '&_t=' + Date.now(), true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        showToast('Saved to EEPROM!');
      } else {
        showToast('Save failed HTTP ' + xhr.status);
      }
      if (btnTop) btnTop.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"/></svg> Save to EEPROM';
      if (btnBottom) btnBottom.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"/></svg> Save to EEPROM';
    }
  };
  xhr.send();
}

function previewPose(pose) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/calibrate?pose=' + pose + '&_t=' + Date.now(), true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      showToast('Pose ' + pose + ' executed');
    }
  };
  xhr.send();
}

function showToast(msg) {
  var t = document.getElementById('toast');
  t.innerText = msg;
  t.classList.add('show');
  setTimeout(function() { t.classList.remove('show'); }, 2000);
}

window.addEventListener('DOMContentLoaded', loadTrims);
</script>
</body>
</html>
)rawliteral";

RoboHeroWeb::RoboHeroWeb(RoboHeroServo &servo, RoboHeroEeprom &eeprom, RoboHeroApp &app)
    : _server(80)
    , _servo(servo)
    , _eeprom(eeprom)
    , _app(app)
{
}

void RoboHeroWeb::begin()
{
    // Register routes for ALL HTTP methods
    _server.on("/", [this]() { handleIndex(); });
    _server.on("/calibrate", [this]() { handleCalibrate(); });

    _server.begin();
}

void RoboHeroWeb::handleClient()
{
    _server.handleClient();
}

void RoboHeroWeb::handleIndex()
{
    // Handle AJAX motion/locomotion actions on "/"
    if (_server.hasArg("pm")) {
        int pm = _server.arg("pm").toInt();
        _app.setServoProgram(pm);
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", "{\"status\":\"ok\",\"pm\":" + String(pm) + "}");
        return;
    }

    if (_server.hasArg("pms")) {
        int pms = _server.arg("pms").toInt();
        _app.setServoProgramStack(pms);
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", "{\"status\":\"ok\",\"pms\":" + String(pms) + "}");
        return;
    }

    _server.send_P(200, "text/html", PAGE_INDEX);
}

void RoboHeroWeb::handleCalibrate()
{
    // 1. Live trim adjustment without writing to EEPROM
    if (_server.hasArg("apply") && _server.hasArg("key") && _server.hasArg("val")) {
        int key = _server.arg("key").toInt();
        int val = _server.arg("val").toInt();
        if (val < -125) val = -125;
        if (val > 125) val = 125;

        _servo.applyTrim(key, (int8_t) val);
        if (key == EEPROM_KEY_VOLTAGE_CAL) {
            _app.resetLowVoltage();
        }

        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", "{\"status\":\"ok\",\"applied\":true,\"key\":" + String(key) + ",\"val\":" + String(val) + "}");
        return;
    }

    // 2. Save all trims to EEPROM
    if (_server.hasArg("save")) {
        for (int i = 0; i <= 19; i++) {
            String argName = "t" + String(i);
            if (_server.hasArg(argName)) {
                int val = _server.arg(argName).toInt();
                if (val < -125) val = -125;
                if (val > 125) val = 125;

                _servo.applyTrim(i, (int8_t) val);
            }
        }
        if (_server.hasArg("t19")) {
            _app.resetLowVoltage();
        }

        bool saved = _eeprom.save();
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", saved ? "{\"status\":\"ok\",\"msg\":\"Saved to EEPROM!\"}" : "{\"status\":\"error\",\"msg\":\"Failed to save EEPROM\"}");
        return;
    }

    // 3. Return current JSON trims
    if (_server.hasArg("json")) {
        String json = "{\"trims\":[";
        for (int i = 0; i <= 19; i++) {
            if (i > 0) {
                json += ",";
            }
            json += String((int) _eeprom.readKeyValue(i));
        }
        json += "]}";
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", json);
        return;
    }

    // 4. Quick pose preview command
    if (_server.hasArg("pose")) {
        String pose = _server.arg("pose");
        if (pose == "zero") {
            _servo.programZero();
        } else if (pose == "center") {
            _servo.programCenter();
        }
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json", "{\"status\":\"ok\",\"pose\":\"" + pose + "\"}");
        return;
    }

    // 5. Serve HTML calibration page directly from PROGMEM (zero RAM allocation)
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, "text/html", PAGE_CALIBRATE);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
