/*
 * RobotModel.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * URDF loader, joint indexing, and hardware channel <-> radian kinematics mapping.
 */

import * as THREE from 'three';
import URDFLoader from 'urdf-loader';

// Mapping from RoboHero hardware channel (0..16) to URDF Joint Name & conversion properties
export const CHANNEL_MAP = {
  0:  { name: 'left_ankle_roll_joint',      group: 'left_leg',  label: 'Ankle Roll',      center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  1:  { name: 'left_ankle_pitch_joint',     group: 'left_leg',  label: 'Ankle Pitch',     center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  2:  { name: 'left_knee_pitch_joint',      group: 'left_leg',  label: 'Knee Pitch',      center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  3:  { name: 'left_hip_pitch_joint',       group: 'left_leg',  label: 'Hip Pitch',       center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  4:  { name: 'left_hip_roll_joint',        group: 'left_leg',  label: 'Hip Roll',        center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  5:  { name: 'left_shoulder_pitch_joint',  group: 'left_arm',  label: 'Shoulder Pitch',  center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  6:  { name: 'left_shoulder_roll_joint',   group: 'left_arm',  label: 'Shoulder Roll',   center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  7:  { name: 'left_elbow_joint',           group: 'left_arm',  label: 'Elbow',           center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  8:  { name: 'right_elbow_joint',          group: 'right_arm', label: 'Elbow',           center: 135, sign: -1.0, radPerPwm: (Math.PI / 180) },
  9:  { name: 'right_shoulder_roll_joint',  group: 'right_arm', label: 'Shoulder Roll',   center: 135, sign: -1.0, radPerPwm: (Math.PI / 180) },
  10: { name: 'right_shoulder_pitch_joint', group: 'right_arm', label: 'Shoulder Pitch',  center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  11: { name: 'right_hip_roll_joint',       group: 'right_leg', label: 'Hip Roll',        center: 135, sign: -1.0, radPerPwm: (Math.PI / 180) },
  12: { name: 'right_hip_pitch_joint',      group: 'right_leg', label: 'Hip Pitch',       center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  13: { name: 'right_knee_pitch_joint',     group: 'right_leg', label: 'Knee Pitch',      center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  14: { name: 'right_ankle_pitch_joint',    group: 'right_leg', label: 'Ankle Pitch',     center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  15: { name: 'right_ankle_roll_joint',     group: 'right_leg', label: 'Ankle Roll',      center: 135, sign:  1.0, radPerPwm: (Math.PI / 180) },
  16: { name: 'head_yaw_joint',             group: 'head',      label: 'Head Yaw',        center:  90, sign:  1.0, radPerPwm: (Math.PI / 180) },
};

export class RobotModel {
  constructor(scene) {
    this.scene = scene;
    this.robot = null;
    this.joints = {};
    this.currentPwm = new Array(17).fill(135);
    this.currentPwm[16] = 90; // Head centered at 90
  }

  async load(urdfUrl = './urdf/robohero.urdf') {
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

          // Apply initial zero / center pose
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
    const map = CHANNEL_MAP[chan];
    if (!map) return 0;
    return (val - map.center) * map.radPerPwm * map.sign;
  }

  radToPwm(chan, rad) {
    const map = CHANNEL_MAP[chan];
    if (!map) return 135;
    return Math.round(map.center + (rad / (map.radPerPwm * map.sign)));
  }

  setServoPwm(chan, val) {
    if (chan < 0 || chan > 16 || !this.robot) return;
    this.currentPwm[chan] = val;
    const map = CHANNEL_MAP[chan];
    if (!map) return;
    const joint = this.joints[map.name];
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
    return CHANNEL_MAP[chan]?.name;
  }

  getChannelByJointName(name) {
    for (const [ch, info] of Object.entries(CHANNEL_MAP)) {
      if (info.name === name) return parseInt(ch, 10);
    }
    return -1;
  }
}
