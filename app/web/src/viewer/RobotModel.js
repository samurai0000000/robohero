/*
 * RobotModel.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * URDF loader, joint indexing, and hardware channel <-> radian kinematics mapping.
 */

import * as THREE from 'three';
import URDFLoader from 'urdf-loader';
import modelCalibration from '../../public/model/calibration.json';

// Construct CHANNEL_MAP dynamically from single source of truth (model/calibration.json)
export const CHANNEL_MAP = {};
for (const [ch, info] of Object.entries(modelCalibration.channels)) {
  const c = parseInt(ch, 10);
  CHANNEL_MAP[c] = {
    name: info.name,
    group: info.group,
    label: info.label,
    center: info.center,
    sign: info.sign,
    radPerPwm: info.rad_per_pwm,
    lower: info.urdf_limits.lower,
    upper: info.urdf_limits.upper,
  };
}

const STORAGE_KEY = 'robohero_model_calibration_v1';

export class RobotModel {
  constructor(scene) {
    this.scene = scene;
    this.robot = null;
    this.joints = {};
    this.currentPwm = [
      160, 161, 141, 168, 158, 158, 243, 159,
      163,  75, 156, 161, 129, 150, 165, 162,  90
    ];

    // Dynamic runtime calibration overrides
    this.calibration = {};
    this.ignoreLimits = false;
    this.initCalibration();
    this.loadCalibrationFromStorage();
  }

  initCalibration() {
    this.calibration = {};
    for (const [ch, map] of Object.entries(CHANNEL_MAP)) {
      const c = parseInt(ch, 10);
      const degPerPwm = (map.radPerPwm * 180) / Math.PI;
      this.calibration[c] = {
        channel: c,
        name: map.name,
        label: map.label,
        group: map.group,
        center: map.center,
        sign: map.sign,
        degPerPwm: parseFloat(degPerPwm.toFixed(4)),
        radPerPwm: map.radPerPwm,
        lower: map.lower,
        upper: map.upper,
        lowerDeg: parseFloat((map.lower * 180 / Math.PI).toFixed(1)),
        upperDeg: parseFloat((map.upper * 180 / Math.PI).toFixed(1)),
      };
    }
  }

  loadCalibrationFromStorage() {
    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved) {
        const data = JSON.parse(saved);
        for (const [ch, overrides] of Object.entries(data)) {
          const c = parseInt(ch, 10);
          if (this.calibration[c]) {
            Object.assign(this.calibration[c], overrides);
            if (overrides.degPerPwm !== undefined) {
              this.calibration[c].radPerPwm = (overrides.degPerPwm * Math.PI) / 180;
            }
            if (overrides.lowerDeg !== undefined) {
              this.calibration[c].lower = parseFloat(((overrides.lowerDeg * Math.PI) / 180).toFixed(4));
            }
            if (overrides.upperDeg !== undefined) {
              this.calibration[c].upper = parseFloat(((overrides.upperDeg * Math.PI) / 180).toFixed(4));
            }
          }
        }
      }
    } catch (e) {
      console.warn('Failed to load calibration from storage:', e);
    }
  }

  saveCalibrationToStorage() {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.calibration));
    } catch (e) {
      console.warn('Failed to save calibration to storage:', e);
    }
  }

  setJointCalibration(chan, params) {
    if (chan < 0 || chan > 16) return;
    const item = this.calibration[chan];
    if (!item) return;

    if (params.center !== undefined) item.center = Math.round(params.center);
    if (params.sign !== undefined) item.sign = params.sign >= 0 ? 1.0 : -1.0;
    if (params.degPerPwm !== undefined) {
      item.degPerPwm = parseFloat(params.degPerPwm);
      item.radPerPwm = (item.degPerPwm * Math.PI) / 180;
    }
    if (params.lowerDeg !== undefined) {
      item.lowerDeg = parseFloat(params.lowerDeg);
      item.lower = parseFloat(((item.lowerDeg * Math.PI) / 180).toFixed(4));
    }
    if (params.upperDeg !== undefined) {
      item.upperDeg = parseFloat(params.upperDeg);
      item.upper = parseFloat(((item.upperDeg * Math.PI) / 180).toFixed(4));
    }

    // Sync physical joint limits on 3D joint mesh
    const joint = this.joints[item.name];
    if (joint) {
      joint.ignoreLimits = this.ignoreLimits;
      if (joint.limit) {
        joint.limit.lower = Math.min(item.lower, item.upper);
        joint.limit.upper = Math.max(item.lower, item.upper);
      }
    }

    // Re-apply live PWM to 3D joint with new kinematics immediately
    this.setServoPwm(chan, this.currentPwm[chan]);
    this.saveCalibrationToStorage();
  }

  syncJointLimits() {
    if (!this.joints) return;
    for (const [ch, cal] of Object.entries(this.calibration)) {
      const joint = this.joints[cal.name];
      if (joint) {
        joint.ignoreLimits = this.ignoreLimits;
        if (joint.limit) {
          joint.limit.lower = Math.min(cal.lower, cal.upper);
          joint.limit.upper = Math.max(cal.lower, cal.upper);
        }
      }
    }
  }

  setIgnoreLimits(ignore) {
    this.ignoreLimits = !!ignore;
    if (this.joints) {
      for (const joint of Object.values(this.joints)) {
        joint.ignoreLimits = this.ignoreLimits;
      }
    }
    // Re-apply all current PWMs so unconstrained visual rotation takes effect
    this.applyAllPwm(this.currentPwm);
  }

  resetJointCalibration(chan) {
    if (chan < 0 || chan > 16) return;
    const map = CHANNEL_MAP[chan];
    if (!map) return;
    const degPerPwm = (map.radPerPwm * 180) / Math.PI;
    this.calibration[chan] = {
      channel: chan,
      name: map.name,
      label: map.label,
      group: map.group,
      center: map.center,
      sign: map.sign,
      degPerPwm: parseFloat(degPerPwm.toFixed(4)),
      radPerPwm: map.radPerPwm,
      lower: map.lower,
      upper: map.upper,
      lowerDeg: parseFloat((map.lower * 180 / Math.PI).toFixed(1)),
      upperDeg: parseFloat((map.upper * 180 / Math.PI).toFixed(1)),
    };
    const joint = this.joints ? this.joints[map.name] : null;
    if (joint && joint.limit) {
      joint.limit.lower = Math.min(map.lower, map.upper);
      joint.limit.upper = Math.max(map.lower, map.upper);
    }
    this.setServoPwm(chan, this.currentPwm[chan]);
    this.saveCalibrationToStorage();
  }

  resetAllCalibrations() {
    this.initCalibration();
    try {
      localStorage.removeItem(STORAGE_KEY);
    } catch (_) {}
    this.syncJointLimits();
    this.applyAllPwm(this.currentPwm);
  }

  getCalibration(chan) {
    return this.calibration[chan];
  }

  getAllCalibrations() {
    return this.calibration;
  }

  async load(urdfUrl = './model/robohero.urdf') {
    return new Promise((resolve, reject) => {
      const loader = new URDFLoader();
      loader.load(
        urdfUrl,
        (robot) => {
          this.robot = robot;
          // Rotate from Z-up (ROS standard) to Y-up (Three.js standard)
          this.robot.rotation.x = -Math.PI / 2;
          this.robot.position.y = 0.165; // Place feet firmly on ground plane

          // Enable shadow casting & receiving on all meshes
          this.robot.traverse((child) => {
            if (child.isMesh) {
              child.castShadow = true;
              child.receiveShadow = true;
              if (child.material) {
                child.material.roughness = 0.45;
                child.material.metalness = 0.25;
              }
            }
          });

          this.joints = robot.joints;
          this.scene.add(this.robot);

          // Synchronize joint limits from calibration and apply pose
          this.syncJointLimits();
          this.applyAllPwm(this.currentPwm);
          resolve(this);
        },
        undefined,
        (error) => {
          console.error('Error loading URDF:', error);
          reject(error);
        }
      );
    });
  }

  pwmToRad(chan, val) {
    const cal = this.calibration[chan] || CHANNEL_MAP[chan];
    if (!cal) return 0;
    return (val - cal.center) * cal.radPerPwm * cal.sign;
  }

  radToPwm(chan, rad) {
    const cal = this.calibration[chan] || CHANNEL_MAP[chan];
    if (!cal) return 135;
    return Math.round(cal.center + (rad / (cal.radPerPwm * cal.sign)));
  }

  getJointAngle(chan, pwm = null) {
    const val = pwm !== null ? pwm : (this.currentPwm[chan] ?? 135);
    const rad = this.pwmToRad(chan, val);
    const deg = (rad * 180) / Math.PI;
    return {
      pwm: val,
      rad: parseFloat(rad.toFixed(4)),
      deg: parseFloat(deg.toFixed(1)),
    };
  }

  setServoPwm(chan, val) {
    if (chan < 0 || chan > 16 || !this.robot) return;
    this.currentPwm[chan] = val;
    const cal = this.calibration[chan] || CHANNEL_MAP[chan];
    if (!cal) return;
    const joint = this.joints[cal.name];
    if (joint) {
      const rad = this.pwmToRad(chan, val);
      joint.setJointValue(rad);
    }
  }

  getServoPwm(chan) {
    return this.currentPwm[chan] ?? 135;
  }

  applyAllPwm(pwmArray) {
    if (!pwmArray || !this.robot) return;
    for (let i = 0; i < 17 && i < pwmArray.length; i++) {
      this.setServoPwm(i, pwmArray[i]);
    }
  }

  getAllPwm() {
    return [...this.currentPwm];
  }

  getJointNameByChannel(chan) {
    return (this.calibration[chan] || CHANNEL_MAP[chan])?.name;
  }

  getChannelByJointName(name) {
    for (const [ch, info] of Object.entries(this.calibration)) {
      if (info.name === name) return parseInt(ch, 10);
    }
    return -1;
  }
}

