/*
 * LimbControls.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Interactive hardware joint sliders grouped by body segments.
 */

import { CHANNEL_MAP } from '../viewer/RobotModel.js';

export const GROUPS = [
  { id: 'head', title: 'Head & Neck', channels: [16] },
  { id: 'left_arm', title: 'Left Arm', channels: [5, 6, 7] },
  { id: 'right_arm', title: 'Right Arm', channels: [10, 9, 8] },
  { id: 'left_leg', title: 'Left Leg', channels: [4, 3, 2, 1, 0] },
  { id: 'right_leg', title: 'Right Leg', channels: [11, 12, 13, 14, 15] },
];

export class LimbControls {
  constructor(containerElement, onSliderChange) {
    this.container = containerElement;
    this.onSliderChange = onSliderChange;
    this.sliders = {};
    this.valReadouts = {};
    this.rows = {};
    this.render();
  }

  render() {
    this.container.innerHTML = '';

    for (const group of GROUPS) {
      const groupEl = document.createElement('div');
      groupEl.className = 'joint-group';
      groupEl.id = `group-${group.id}`;

      const header = document.createElement('div');
      header.className = 'joint-group-header';
      header.innerHTML = `<span class="joint-group-title">${group.title}</span>`;
      groupEl.appendChild(header);

      for (const ch of group.channels) {
        const info = CHANNEL_MAP[ch];
        const row = document.createElement('div');
        row.className = 'joint-slider-row';
        row.id = `slider-row-${ch}`;
        this.rows[ch] = row;

        const meta = document.createElement('div');
        meta.className = 'slider-meta';
        meta.innerHTML = `
          <div>
            <span class="joint-chan-tag">CH ${ch.toString().padStart(2, '0')}</span>
            <span class="joint-name">${info.label}</span>
          </div>
          <span class="joint-value-readout" id="readout-${ch}">135</span>
        `;
        row.appendChild(meta);

        const controls = document.createElement('div');
        controls.className = 'slider-controls';

        const btnMinus = document.createElement('button');
        btnMinus.className = 'btn-step';
        btnMinus.textContent = '−';
        btnMinus.title = 'Step Decrement';
        btnMinus.onclick = () => this.stepValue(ch, -1);

        const slider = document.createElement('input');
        slider.type = 'range';
        slider.min = (ch === 16) ? '0' : '1';
        slider.max = (ch === 16) ? '180' : '270';
        slider.value = (ch === 16) ? '90' : '135';
        slider.id = `slider-${ch}`;
        this.sliders[ch] = slider;
        this.valReadouts[ch] = meta.querySelector(`#readout-${ch}`);

        slider.oninput = () => {
          const val = parseInt(slider.value, 10);
          this.updateReadout(ch, val);
          if (this.onSliderChange) {
            this.onSliderChange(ch, val);
          }
        };

        const btnPlus = document.createElement('button');
        btnPlus.className = 'btn-step';
        btnPlus.textContent = '+';
        btnPlus.title = 'Step Increment';
        btnPlus.onclick = () => this.stepValue(ch, 1);

        controls.appendChild(btnMinus);
        controls.appendChild(slider);
        controls.appendChild(btnPlus);

        row.appendChild(controls);
        groupEl.appendChild(row);
      }

      this.container.appendChild(groupEl);
    }
  }

  stepValue(chan, delta) {
    const slider = this.sliders[chan];
    if (!slider) return;
    let val = parseInt(slider.value, 10) + delta;
    val = Math.max(parseInt(slider.min, 10), Math.min(parseInt(slider.max, 10), val));
    slider.value = val;
    this.updateReadout(chan, val);
    if (this.onSliderChange) {
      this.onSliderChange(chan, val);
    }
  }

  updateReadout(chan, val) {
    const readout = this.valReadouts[chan];
    if (readout) {
      readout.textContent = val.toString();
    }
  }

  setValue(chan, val) {
    const slider = this.sliders[chan];
    if (slider) {
      slider.value = val;
      this.updateReadout(chan, val);
    }
  }

  setAllValues(pwmArray) {
    if (!pwmArray) return;
    for (let i = 0; i < 17 && i < pwmArray.length; i++) {
      this.setValue(i, pwmArray[i]);
    }
  }

  highlightChannel(chan) {
    for (const [ch, row] of Object.entries(this.rows)) {
      if (parseInt(ch, 10) === chan) {
        row.classList.add('highlighted');
        row.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      } else {
        row.classList.remove('highlighted');
      }
    }
  }
}
