/*
 * main.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * RoboHero 3D Web & Desktop Application Bootstrapper.
 */

import { SceneManager } from './viewer/SceneManager.js';
import { RobotModel } from './viewer/RobotModel.js';
import { LimbControls } from './ui/LimbControls.js';
import { MqttBridge, RH_MSG_CENTER, RH_MSG_ZERO, RH_MSG_RELAX } from './telemetry/MqttBridge.js';
import { PRESET_POSES } from './studio/Presets.js';
import { PoseSequencer } from './studio/PoseSequencer.js';

class RoboHeroApp {
  constructor() {
    this.canvasContainer = document.getElementById('canvas-3d');
    this.loadingOverlay = document.getElementById('loading-overlay');
    this.toastEl = document.getElementById('toast');

    this.sceneManager = null;
    this.robotModel = null;
    this.limbControls = null;
    this.mqttBridge = new MqttBridge();
    this.sequencer = null;

    this.init();
  }

  async init() {
    // 1. Initialize Three.js Scene
    this.sceneManager = new SceneManager(this.canvasContainer, (clickedObject) => {
      this.handle3DObjectClick(clickedObject);
    });

    // 2. Initialize Robot Model
    this.robotModel = new RobotModel(this.sceneManager.scene);

    // 3. Initialize Limb Controls UI
    const controlsContainer = document.getElementById('joint-controls-container');
    this.limbControls = new LimbControls(controlsContainer, (chan, val) => {
      this.handleSliderChange(chan, val);
    });

    // 4. Initialize Pose Sequencer
    this.sequencer = new PoseSequencer(this.robotModel, (pwm) => {
      this.applyPoseToAll(pwm);
    });

    // 5. Load URDF Model
    try {
      await this.robotModel.load('./urdf/robohero.urdf');
      this.limbControls.setAllValues(this.robotModel.getAllPwm());
      if (this.loadingOverlay) {
        this.loadingOverlay.classList.add('hidden');
      }
    } catch (err) {
      console.error('Failed to load URDF model:', err);
      if (this.loadingOverlay) {
        this.loadingOverlay.querySelector('.loading-text').textContent =
          'Failed to load robohero.urdf. Check file paths.';
      }
    }

    // 6. Setup Event Listeners
    this.setupViewControls();
    this.setupPresetButtons();
    this.setupSequencerUI();
    this.setupMqttBridge();
    this.setupSettingsModal();
    this.setupExportModal();

    // 7. Load Configuration & Auto-connect if enabled
    await this.loadInitialConfig();
  }

  async loadInitialConfig() {
    try {
      const res = await fetch('./config.json');
      if (res.ok) {
        const defaults = await res.json();
        const cfg = this.mqttBridge.loadConfig();
        this.mqttBridge.saveConfig({ ...defaults, ...cfg });
      }
    } catch (_) {
      this.mqttBridge.loadConfig();
    }

    if (this.mqttBridge.config.autoConnect) {
      this.mqttBridge.connect();
    }
  }

  handleSliderChange(chan, val) {
    this.robotModel.setServoPwm(chan, val);

    const liveSend = document.getElementById('chk-live-send')?.checked;
    if (liveSend && this.mqttBridge.isConnected) {
      this.mqttBridge.sendSetPwm(chan, val);
    }
  }

  handle3DObjectClick(obj) {
    if (!obj) return;
    let name = obj.name || '';
    let chan = this.robotModel.getChannelByJointName(name);

    if (chan === -1 && obj.parent) {
      chan = this.robotModel.getChannelByJointName(obj.parent.name);
    }

    if (chan !== -1) {
      this.limbControls.highlightChannel(chan);
      this.showToast(`Selected Joint: CH ${chan} (${obj.name})`);
    }
  }

  applyPoseToAll(pwmArray) {
    this.robotModel.applyAllPwm(pwmArray);
    this.limbControls.setAllValues(pwmArray);

    const liveSend = document.getElementById('chk-live-send')?.checked;
    if (liveSend && this.mqttBridge.isConnected) {
      for (let i = 0; i < 17 && i < pwmArray.length; i++) {
        this.mqttBridge.sendSetPwm(i, pwmArray[i]);
      }
    }
  }

  setupViewControls() {
    const views = ['persp', 'front', 'side', 'top'];
    views.forEach((v) => {
      const btn = document.getElementById(`btn-view-${v}`);
      if (btn) {
        btn.onclick = () => {
          views.forEach((other) =>
            document.getElementById(`btn-view-${other}`)?.classList.remove('active')
          );
          btn.classList.add('active');
          this.sceneManager.setCameraPreset(v);
        };
      }
    });

    const btnReset = document.getElementById('btn-reset-cam');
    if (btnReset) {
      btnReset.onclick = () => this.sceneManager.setCameraPreset('persp');
    }
  }

  setupPresetButtons() {
    const bindPreset = (btnId, poseKey, cmdType = null) => {
      const btn = document.getElementById(btnId);
      if (!btn) return;
      btn.onclick = () => {
        if (PRESET_POSES[poseKey]) {
          this.applyPoseToAll(PRESET_POSES[poseKey].pwm);
          this.showToast(`Applied ${PRESET_POSES[poseKey].name}`);
        } else if (poseKey === 'relax') {
          this.showToast('Relax (PWM Stopped)');
        }
        if (cmdType !== null && this.mqttBridge.isConnected) {
          this.mqttBridge.sendSimpleCmd(cmdType);
        }
      };
    };

    bindPreset('preset-standby', 'standby', RH_MSG_CENTER);
    bindPreset('preset-zero', 'zero', RH_MSG_ZERO);
    bindPreset('preset-relax', 'relax', RH_MSG_RELAX);
    bindPreset('preset-bow', 'bow');
    bindPreset('preset-wave', 'wave');
    bindPreset('preset-ironman', 'ironman');
    bindPreset('preset-apache', 'apache');
  }

  setupSequencerUI() {
    const btnAdd = document.getElementById('btn-seq-add');
    const btnPlay = document.getElementById('btn-seq-play');
    const btnClear = document.getElementById('btn-seq-clear');
    const btnExport = document.getElementById('btn-seq-export');
    const timeline = document.getElementById('keyframe-timeline');
    const countBadge = document.getElementById('keyframe-count');
    const emptyMsg = document.getElementById('timeline-empty-msg');

    const updateTimelineDOM = () => {
      const n = this.sequencer.count();
      if (countBadge) countBadge.textContent = `${n} Keyframes`;
      if (emptyMsg) emptyMsg.style.display = n === 0 ? 'block' : 'none';

      timeline.querySelectorAll('.keyframe-card').forEach((el) => el.remove());

      for (let i = 0; i < n; i++) {
        const kf = this.sequencer.getKeyframe(i);
        const card = document.createElement('div');
        card.className = 'keyframe-card';
        card.id = `kf-card-${i}`;
        card.innerHTML = `
          <span class="kf-num">#${i + 1}</span>
          <span class="kf-time">${kf.durationMs} ms</span>
          <button class="kf-del-btn" title="Delete Keyframe">&times;</button>
        `;

        card.onclick = (e) => {
          if (e.target.classList.contains('kf-del-btn')) return;
          timeline.querySelectorAll('.keyframe-card').forEach((c) => c.classList.remove('active'));
          card.classList.add('active');
          this.applyPoseToAll(kf.pwm);
        };

        const delBtn = card.querySelector('.kf-del-btn');
        delBtn.onclick = (e) => {
          e.stopPropagation();
          this.sequencer.removeKeyframe(i);
          updateTimelineDOM();
        };

        timeline.appendChild(card);
      }
    };

    if (btnAdd) {
      btnAdd.onclick = () => {
        this.sequencer.addKeyframe(null, 500);
        updateTimelineDOM();
        this.showToast('Keyframe captured');
      };
    }

    if (btnPlay) {
      btnPlay.onclick = () => {
        if (this.sequencer.isPlaying) {
          this.sequencer.stop();
          btnPlay.innerHTML = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg> Play Motion`;
          return;
        }

        btnPlay.innerHTML = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><rect x="6" y="4" width="4" height="16"></rect><rect x="14" y="4" width="4" height="16"></rect></svg> Stop`;

        this.sequencer.play((activeIdx) => {
          timeline.querySelectorAll('.keyframe-card').forEach((c, idx) => {
            if (idx === activeIdx) c.classList.add('active');
            else c.classList.remove('active');
          });
          if (activeIdx === -1) {
            btnPlay.innerHTML = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg> Play Motion`;
          }
        });
      };
    }

    if (btnClear) {
      btnClear.onclick = () => {
        this.sequencer.clear();
        updateTimelineDOM();
      };
    }

    if (btnExport) {
      btnExport.onclick = () => {
        const code = this.sequencer.generateCppCode();
        const codeBox = document.getElementById('txt-export-code');
        if (codeBox) codeBox.textContent = code;
        document.getElementById('modal-export')?.classList.add('show');
      };
    }
  }

  setupMqttBridge() {
    const btnConnect = document.getElementById('btn-mqtt-connect');
    const txtStatus = document.getElementById('txt-mqtt-status');
    const valVoltage = document.getElementById('val-voltage');
    const hudStatus = document.getElementById('hud-sync-status');

    this.mqttBridge.onStatusChange = (status) => {
      if (status === 'connected') {
        btnConnect.className = 'btn-connect connected';
        if (txtStatus) txtStatus.textContent = 'MQTT Connected';
        if (hudStatus) hudStatus.textContent = 'Live Sync Active';
        this.showToast('Connected to MQTT broker');
      } else if (status === 'connecting') {
        btnConnect.className = 'btn-connect disconnected';
        if (txtStatus) txtStatus.textContent = 'Connecting...';
        if (hudStatus) hudStatus.textContent = 'Connecting...';
      } else {
        btnConnect.className = 'btn-connect disconnected';
        if (txtStatus) txtStatus.textContent = 'Connect MQTT';
        if (hudStatus) hudStatus.textContent = 'Idle';
      }
    };

    this.mqttBridge.onServoUpdate = (chan, pos) => {
      // Physical robot sent status update -> update 3D model & sliders
      this.robotModel.setServoPwm(chan, pos);
      this.limbControls.setValue(chan, pos);
    };

    this.mqttBridge.onVoltageUpdate = (voltRaw) => {
      if (valVoltage) {
        const v = (voltRaw / 100.0).toFixed(2);
        valVoltage.textContent = `${v} V`;
      }
    };

    if (btnConnect) {
      btnConnect.onclick = () => {
        if (this.mqttBridge.isConnected) {
          this.mqttBridge.disconnect();
        } else {
          this.mqttBridge.connect();
        }
      };
    }
  }

  setupSettingsModal() {
    const modal = document.getElementById('modal-settings');
    const btnOpen = document.getElementById('btn-open-settings');
    const btnClose = document.getElementById('btn-close-settings');
    const btnCancel = document.getElementById('btn-cancel-settings');
    const btnSave = document.getElementById('btn-save-settings');

    const inputUrl = document.getElementById('cfg-broker-url');
    const inputRobotId = document.getElementById('cfg-robot-id');
    const inputUser = document.getElementById('cfg-username');
    const inputPass = document.getElementById('cfg-password');
    const inputAuto = document.getElementById('cfg-auto-connect');

    const openModal = () => {
      const cfg = this.mqttBridge.config;
      if (inputUrl) inputUrl.value = cfg.brokerUrl || '';
      if (inputRobotId) inputRobotId.value = cfg.robotId || 'robohero';
      if (inputUser) inputUser.value = cfg.username || '';
      if (inputPass) inputPass.value = cfg.password || '';
      if (inputAuto) inputAuto.checked = !!cfg.autoConnect;
      modal?.classList.add('show');
    };

    const closeModal = () => {
      modal?.classList.remove('show');
    };

    if (btnOpen) btnOpen.onclick = openModal;
    if (btnClose) btnClose.onclick = closeModal;
    if (btnCancel) btnCancel.onclick = closeModal;

    if (btnSave) {
      btnSave.onclick = () => {
        const newCfg = {
          brokerUrl: inputUrl.value.trim(),
          robotId: inputRobotId.value.trim() || 'robohero',
          username: inputUser.value.trim(),
          password: inputPass.value,
          autoConnect: inputAuto.checked,
        };
        this.mqttBridge.saveConfig(newCfg);
        closeModal();
        this.mqttBridge.connect();
        this.showToast('Settings saved & reconnecting...');
      };
    }
  }

  setupExportModal() {
    const modal = document.getElementById('modal-export');
    const btnClose = document.getElementById('btn-close-export');
    const btnCloseBtn = document.getElementById('btn-close-export-btn');
    const btnCopy = document.getElementById('btn-copy-code');
    const codeBox = document.getElementById('txt-export-code');

    const closeModal = () => modal?.classList.remove('show');
    if (btnClose) btnClose.onclick = closeModal;
    if (btnCloseBtn) btnCloseBtn.onclick = closeModal;

    if (btnCopy) {
      btnCopy.onclick = () => {
        if (codeBox) {
          navigator.clipboard.writeText(codeBox.textContent).then(() => {
            this.showToast('Code copied to clipboard!');
            closeModal();
          });
        }
      };
    }
  }

  showToast(msg) {
    if (!this.toastEl) return;
    this.toastEl.textContent = msg;
    this.toastEl.classList.add('show');
    setTimeout(() => {
      this.toastEl.classList.remove('show');
    }, 2400);
  }
}

// Bootstrap on DOM Ready
window.addEventListener('DOMContentLoaded', () => {
  new RoboHeroApp();
});
