/*
 * PoseSequencer.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Keyframe sequencer, playback interpolator, and C++ code generator for Motions.cxx.
 */

export class PoseSequencer {
  constructor(robotModel, onApplyPose) {
    this.robotModel = robotModel;
    this.onApplyPose = onApplyPose;
    this.keyframes = [];
    this.isPlaying = false;
    this.currentPlayIdx = 0;
  }

  addKeyframe(pwmArray = null, durationMs = 500) {
    const pose = pwmArray ? [...pwmArray] : this.robotModel.getAllPwm();
    this.keyframes.push({
      pwm: pose,
      durationMs: durationMs,
    });
    return this.keyframes.length - 1;
  }

  removeKeyframe(index) {
    if (index >= 0 && index < this.keyframes.length) {
      this.keyframes.splice(index, 1);
    }
  }

  clear() {
    this.keyframes = [];
  }

  getKeyframe(index) {
    return this.keyframes[index];
  }

  count() {
    return this.keyframes.length;
  }

  async play(onStepCallback = null) {
    if (this.keyframes.length === 0 || this.isPlaying) return;
    this.isPlaying = true;

    for (let i = 0; i < this.keyframes.length; i++) {
      if (!this.isPlaying) break;
      const kf = this.keyframes[i];
      this.currentPlayIdx = i;
      if (onStepCallback) onStepCallback(i);

      if (this.onApplyPose) {
        this.onApplyPose(kf.pwm);
      }

      await new Promise((res) => setTimeout(res, Math.max(kf.durationMs, 100)));
    }

    this.isPlaying = false;
    if (onStepCallback) onStepCallback(-1);
  }

  stop() {
    this.isPlaying = false;
  }

  generateCppCode(progNum = 10, progName = "Custom_Gait") {
    const steps = this.keyframes.length;
    let code = `// Movement - ${progName}\n`;
    code += `const int Servo_Prg_${progNum}_Step = ${steps};\n`;
    code += `const int Servo_Prg_${progNum}[][ALLMATRIX] = {\n`;

    for (let i = 0; i < steps; i++) {
      const kf = this.keyframes[i];
      const p = kf.pwm;
      const dur = kf.durationMs;

      // Line 1: Servos 0-7
      const line1 = `    { ${p[0].toString().padStart(3)}, ${p[1].toString().padStart(3)}, ${p[2].toString().padStart(3)}, ${p[3].toString().padStart(3)}, ${p[4].toString().padStart(3)}, ${p[5].toString().padStart(3)}, ${p[6].toString().padStart(3)}, ${p[7].toString().padStart(3)},`;
      // Line 2: Servos 8-15, Head (16), Delay (17)
      const line2 = `      ${p[8].toString().padStart(3)}, ${p[9].toString().padStart(3)}, ${p[10].toString().padStart(3)}, ${p[11].toString().padStart(3)}, ${p[12].toString().padStart(3)}, ${p[13].toString().padStart(3)}, ${p[14].toString().padStart(3)}, ${p[15].toString().padStart(3)}, ${p[16].toString().padStart(3)}, ${dur.toString().padStart(4)}, },`;

      code += `${line1}\n${line2}\n`;
    }

    code += `};\n`;
    return code;
  }
}
