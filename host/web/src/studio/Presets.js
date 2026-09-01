/*
 * Presets.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Built-in poses for RoboHero matching Motions.cxx firmware definitions.
 */

export const PRESET_POSES = {
  zero: {
    name: 'Zero Alignment Pose',
    pwm: [
      135, 135, 135, 135, 135, 135, 135, 135,
      135, 135, 135, 135, 135, 135, 135, 135,  90
    ],
  },
  standby: {
    name: 'Standby / Center Pose',
    pwm: [
      135, 135, 135, 135, 135, 135, 200, 135,
      135,  60, 135, 135, 135, 135, 135, 135,  90
    ],
  },
  bow: {
    name: 'Bow Pose',
    pwm: [
      135, 120, 135, 100, 135, 160, 200, 110,
      200,  50, 210, 135, 165, 135, 150, 135,  90
    ],
  },
  wave: {
    name: 'Wave Pose',
    pwm: [
      125, 135, 135, 135, 125, 135, 200, 135,
      135,  90, 250, 125, 135, 135, 135, 125,  60
    ],
  },
  ironman: {
    name: 'Iron Man Pose',
    pwm: [
      145, 135, 135, 135, 145, 135, 200, 180,
       40, 145, 135, 145, 135, 135, 135, 145,  60
    ],
  },
  apache: {
    name: 'Apache Pose',
    pwm: [
      125, 135, 135, 135, 125, 135, 170,  60,
      210,  50, 270, 150, 135, 135, 135, 115, 120
    ],
  },
};
