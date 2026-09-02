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
import { ModelCalibrator } from './ui/ModelCalibrator.js';
import { PanelResizer } from './ui/PanelResizer.js';
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
    this.modelCalibrator = null;
    this.mqttBridge = new MqttBridge();
    this.sequencer = null;
    this.panelResizer = null;

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

    // 4. Initialize Model Calibrator UI
    const calibratorContainer = document.getElementById('model-calibrator-container');
    this.modelCalibrator = new ModelCalibrator(
      calibratorContainer,
      this.robotModel,
      (chan, val) => this.handleSliderChange(chan, val),
      () => this.openExtractModal()
    );

    // 5. Initialize Pose Sequencer
    this.sequencer = new PoseSequencer(this.robotModel, (pwm) => {
      this.applyPoseToAll(pwm);
    });

    // 6. Load URDF Model
    try {
      await this.robotModel.load('./urdf/robohero.urdf');
      this.limbControls.setAllValues(this.robotModel.getAllPwm());
      this.modelCalibrator.setAllPwmValues(this.robotModel.getAllPwm());
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

    // 7. Setup Event Listeners & Modals
    this.setupSidebarTabs();
    this.setupViewControls();
    this.setupPresetButtons();
    this.setupSequencerUI();
    this.setupMqttBridge();
    this.setupSettingsModal();
    this.setupExportModal();
    this.setupExtractModal();

    // 8. Setup Interactive Panel Split Resizers
    this.panelResizer = new PanelResizer();

    // 9. Load Configuration & Auto-connect if enabled
    await this.loadInitialConfig();
  }

  async loadInitialConfig() {
    try {
      const res = await fetch('./config.json');
      if (res.ok) {
        const defaults = await res.json();
        const cfg = this.mqttBridge.loadConfig();
        const merged = {
          brokerUrl: cfg.brokerUrl || defaults.brokerUrl,
          robotId: cfg.robotId || defaults.robotId,
          username: (cfg.username !== undefined && cfg.username !== '') ? cfg.username : defaults.username,
          password: (cfg.password !== undefined && cfg.password !== '') ? cfg.password : defaults.password,
          autoConnect: cfg.autoConnect !== undefined ? cfg.autoConnect : defaults.autoConnect,
        };
        this.mqttBridge.saveConfig(merged);
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

  applyPoseToAll(pwmArray, sendToRobot = true) {
    this.robotModel.applyAllPwm(pwmArray);
    this.limbControls.setAllValues(pwmArray);
    if (this.modelCalibrator) {
      this.modelCalibrator.setAllPwmValues(pwmArray);
    }

    const liveSend = document.getElementById('chk-live-send')?.checked;
    if (sendToRobot && liveSend && this.mqttBridge.isConnected) {
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
    const bindBtn = (btnId, fn) => {
      const btn = document.getElementById(btnId);
      if (btn) btn.onclick = fn;
    };

    // Quick Poses
    bindBtn('preset-standby', () => {
      this.showToast('Standby Pose');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendCenter();
      } else if (PRESET_POSES.standby) {
        this.applyPoseToAll(PRESET_POSES.standby.pwm, false);
      }
    });

    bindBtn('preset-zero', () => {
      this.showToast('Zero Alignment Pose');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendZero();
      } else if (PRESET_POSES.zero) {
        this.applyPoseToAll(PRESET_POSES.zero.pwm, false);
      }
    });

    bindBtn('preset-relax', () => {
      this.showToast('Relax (PWM Stopped)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendRelax();
      }
    });

    // Locomotion (D-Pad matching firmware)
    bindBtn('btn-turn-left', () => {
      this.showToast('Turn Left (PM 3)');
      this.mqttBridge.sendPm(3);
    });
    bindBtn('btn-forward', () => {
      this.showToast('Forward (PM 1)');
      this.mqttBridge.sendPm(1);
    });
    bindBtn('btn-turn-right', () => {
      this.showToast('Turn Right (PM 4)');
      this.mqttBridge.sendPm(4);
    });
    bindBtn('btn-move-left', () => {
      this.showToast('Move Left (PM 5)');
      this.mqttBridge.sendPm(5);
    });
    bindBtn('btn-loco-standby', () => {
      this.showToast('Standby Pose');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendCenter();
      } else if (PRESET_POSES.standby) {
        this.applyPoseToAll(PRESET_POSES.standby.pwm, false);
      }
    });
    bindBtn('btn-move-right', () => {
      this.showToast('Move Right (PM 6)');
      this.mqttBridge.sendPm(6);
    });
    bindBtn('btn-loco-relax', () => {
      this.showToast('Relax (PWM Stopped)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendRelax();
      }
    });
    bindBtn('btn-backward', () => {
      this.showToast('Backward (PM 2)');
      this.mqttBridge.sendPm(2);
    });
    bindBtn('btn-loco-stop', () => {
      this.showToast('Stopped');
      this.mqttBridge.sendStop();
    });

    // Recovery
    bindBtn('btn-get-up', () => {
      this.showToast('Get Up (PM 11)');
      this.mqttBridge.sendPm(11);
    });
    bindBtn('btn-get-up-face', () => {
      this.showToast('Face-Down Get Up (PM 12)');
      this.mqttBridge.sendPm(12);
    });

    // Actions & Gestures (PMS 1..9, 99 matching firmware)
    bindBtn('btn-action-bow', () => {
      this.showToast('Bow (PMS 1)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendPms(1);
      } else if (PRESET_POSES.bow) {
        this.applyPoseToAll(PRESET_POSES.bow.pwm, false);
      }
    });
    bindBtn('btn-action-apache', () => {
      this.showToast('Apache (PMS 4)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendPms(4);
      } else if (PRESET_POSES.apache) {
        this.applyPoseToAll(PRESET_POSES.apache.pwm, false);
      }
    });
    bindBtn('btn-action-wave', () => {
      this.showToast('Waving (PMS 2)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendPms(2);
      } else if (PRESET_POSES.wave) {
        this.applyPoseToAll(PRESET_POSES.wave.pwm, false);
      }
    });
    bindBtn('btn-action-balance', () => {
      this.showToast('Balance (PMS 5)');
      this.mqttBridge.sendPms(5);
    });
    bindBtn('btn-action-ironman', () => {
      this.showToast('Iron Man (PMS 3)');
      if (this.mqttBridge.isConnected) {
        this.mqttBridge.sendPms(3);
      } else if (PRESET_POSES.ironman) {
        this.applyPoseToAll(PRESET_POSES.ironman.pwm, false);
      }
    });
    bindBtn('btn-action-warmup', () => {
      this.showToast('Warm-Up (PMS 6)');
      this.mqttBridge.sendPms(6);
    });
    bindBtn('btn-action-clap', () => {
      this.showToast('Clap Hands (PMS 7)');
      this.mqttBridge.sendPms(7);
    });
    bindBtn('btn-action-goilc', () => {
      this.showToast('GOILC (PMS 8)');
      this.mqttBridge.sendPms(8);
    });
    bindBtn('btn-action-dance', () => {
      this.showToast('Dance (PMS 9)');
      this.mqttBridge.sendPms(9);
    });
    bindBtn('btn-action-auto', () => {
      this.showToast('Auto Demo Loop (PMS 99)');
      this.mqttBridge.sendPms(99);
    });
    bindBtn('btn-action-stop', () => {
      this.showToast('STOP Command Sent');
      this.mqttBridge.sendStop();
    });
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

    this.mqttBridge.onStatusChange = (status, errorDetail = null) => {
      if (status === 'connected') {
        btnConnect.className = 'btn-connect connected';
        if (txtStatus) txtStatus.textContent = 'MQTT Connected';
        if (hudStatus) hudStatus.textContent = 'Live Sync Active';
        this.showToast('Connected to MQTT broker');
      } else if (status === 'connecting') {
        btnConnect.className = 'btn-connect disconnected';
        if (txtStatus) txtStatus.textContent = 'Connecting...';
        if (hudStatus) hudStatus.textContent = 'Connecting...';
      } else if (status === 'error') {
        btnConnect.className = 'btn-connect disconnected';
        if (txtStatus) txtStatus.textContent = 'Auth / Conn Error';
        if (hudStatus) hudStatus.textContent = 'Auth Failed';
        this.showToast(`MQTT Error: ${errorDetail || 'Connection failed'}`);
      } else {
        btnConnect.className = 'btn-connect disconnected';
        if (txtStatus) txtStatus.textContent = 'Connect MQTT';
        if (hudStatus) hudStatus.textContent = 'Idle';
        if (valVoltage) valVoltage.textContent = '---';
      }
    };

    this.mqttBridge.onServoUpdate = (chan, pos) => {
      // Physical robot sent status update -> update 3D model & sliders
      this.robotModel.setServoPwm(chan, pos);
      this.limbControls.setValue(chan, pos);
    };

    this.mqttBridge.onVoltageUpdate = (voltRaw) => {
      if (valVoltage) {
        valVoltage.textContent = String(voltRaw);
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

  setupSidebarTabs() {
    const tabStudio = document.getElementById('tab-btn-studio');
    const tabCalibrator = document.getElementById('tab-btn-calibrator');
    const panelStudio = document.getElementById('panel-pose-studio');
    const panelCalibrator = document.getElementById('panel-model-calibrator');

    if (tabStudio && tabCalibrator && panelStudio && panelCalibrator) {
      tabStudio.onclick = () => {
        tabStudio.classList.add('active');
        tabCalibrator.classList.remove('active');
        panelStudio.style.display = 'flex';
        panelCalibrator.style.display = 'none';
      };

      tabCalibrator.onclick = () => {
        tabCalibrator.classList.add('active');
        tabStudio.classList.remove('active');
        panelStudio.style.display = 'none';
        panelCalibrator.style.display = 'flex';
        if (this.modelCalibrator) {
          this.modelCalibrator.setAllPwmValues(this.robotModel.getAllPwm());
        }
      };
    }
  }

  openExtractModal(tab = 'urdf') {
    const modal = document.getElementById('modal-extract-params');
    if (!modal) return;
    this.switchExtractTab(tab);
    modal.classList.add('show');
  }

  switchExtractTab(tab) {
    this.activeExtractTab = tab;
    const btnUrdf = document.getElementById('btn-tab-urdf');
    const btnJs = document.getElementById('btn-tab-js');
    const btnJson = document.getElementById('btn-tab-json');
    const desc = document.getElementById('txt-extract-desc');
    const code = document.getElementById('txt-extract-code');

    btnUrdf?.classList.toggle('active', tab === 'urdf');
    btnJs?.classList.toggle('active', tab === 'js');
    btnJson?.classList.toggle('active', tab === 'json');

    if (tab === 'urdf') {
      if (desc) desc.innerHTML = 'Copy and replace the corresponding <code>&lt;limit ... /&gt;</code> tags in <code>urdf/robohero.urdf</code>:';
      if (code) code.textContent = this.modelCalibrator.generateUrdfXml();
    } else if (tab === 'js') {
      if (desc) desc.innerHTML = 'Replace <code>export const CHANNEL_MAP = ...</code> in <code>app/web/src/viewer/RobotModel.js</code>:';
      if (code) code.textContent = this.modelCalibrator.generateChannelMapJs();
    } else if (tab === 'json') {
      if (desc) desc.innerHTML = 'Complete machine-readable calibration dataset (can be saved to <code>doc/joint_calibration_results.json</code>):';
      if (code) code.textContent = this.modelCalibrator.generateCalibrationJson();
    }
  }

  setupExtractModal() {
    const modal = document.getElementById('modal-extract-params');
    const btnClose = document.getElementById('btn-close-extract-params');
    const btnCloseBtn = document.getElementById('btn-close-extract-modal-btn');
    const btnCopy = document.getElementById('btn-copy-extract-code');
    const btnDownload = document.getElementById('btn-download-json');
    const codeBox = document.getElementById('txt-extract-code');

    const btnUrdf = document.getElementById('btn-tab-urdf');
    const btnJs = document.getElementById('btn-tab-js');
    const btnJson = document.getElementById('btn-tab-json');

    if (btnUrdf) btnUrdf.onclick = () => this.switchExtractTab('urdf');
    if (btnJs) btnJs.onclick = () => this.switchExtractTab('js');
    if (btnJson) btnJson.onclick = () => this.switchExtractTab('json');

    const closeModal = () => modal?.classList.remove('show');
    if (btnClose) btnClose.onclick = closeModal;
    if (btnCloseBtn) btnCloseBtn.onclick = closeModal;

    if (btnCopy) {
      btnCopy.onclick = () => {
        if (codeBox) {
          navigator.clipboard.writeText(codeBox.textContent).then(() => {
            this.showToast('Parameters copied to clipboard!');
          });
        }
      };
    }

    if (btnDownload) {
      btnDownload.onclick = () => {
        const jsonStr = this.modelCalibrator.generateCalibrationJson();
        const blob = new Blob([jsonStr], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `robohero_calibration_${new Date().toISOString().slice(0, 10)}.json`;
        a.click();
        URL.revokeObjectURL(url);
        this.showToast('Downloaded calibration JSON!');
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
