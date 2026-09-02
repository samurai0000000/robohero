/*
 * ModelCalibrator.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Real-time Kinematic Tuning & Joint Calibration Studio UI.
 * Allows interactive adjustment of joint centers, angular scaling, direction signs,
 * and physical travel limits with instant 3D viewport reflection and code export.
 */

import { GROUPS } from './LimbControls.js';

export class ModelCalibrator {
  constructor(containerElement, robotModel, onPwmChange, onExtractClick) {
    this.container = containerElement;
    this.robotModel = robotModel;
    this.onPwmChange = onPwmChange;
    this.onExtractClick = onExtractClick;

    this.sliders = {};
    this.readouts = {};
    this.inputs = {};
    this.cards = {};
    this.filterQuery = '';

    this.render();
  }

  render() {
    this.container.innerHTML = '';

    // 1. Top Calibrator Header & Action Bar
    const topBar = document.createElement('div');
    topBar.className = 'calibrator-top-bar';

    const searchInput = document.createElement('input');
    searchInput.type = 'text';
    searchInput.className = 'calibrator-search';
    searchInput.placeholder = 'Search joint by name or CH...';
    searchInput.oninput = (e) => {
      this.filterQuery = e.target.value.toLowerCase().trim();
      this.applyFilter();
    };

    const actionGroup = document.createElement('div');
    actionGroup.className = 'calibrator-actions';

    const btnExtract = document.createElement('button');
    btnExtract.className = 'btn-action btn-accent btn-extract-params';
    btnExtract.innerHTML = `
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
        <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
        <polyline points="7 10 12 15 17 10"></polyline>
        <line x1="12" y1="15" x2="12" y2="3"></line>
      </svg>
      Extract Parameters
    `;
    btnExtract.title = 'Export URDF XML and JavaScript calibration snippets';
    btnExtract.onclick = () => {
      if (this.onExtractClick) {
        this.onExtractClick();
      }
    };

    const btnResetAll = document.createElement('button');
    btnResetAll.className = 'btn-action btn-reset-all';
    btnResetAll.textContent = 'Reset All Defaults';
    btnResetAll.title = 'Reset all 17 joints back to factory URDF defaults';
    btnResetAll.onclick = () => {
      if (confirm('Reset all model calibration adjustments back to URDF defaults?')) {
        this.robotModel.resetAllCalibrations();
        this.render();
      }
    };

    actionGroup.appendChild(btnExtract);
    actionGroup.appendChild(btnResetAll);

    // Unconstrained Angles Toggle (Unclamps joints for free calibration)
    const optionsRow = document.createElement('div');
    optionsRow.className = 'calibrator-options-row';
    optionsRow.innerHTML = `
      <label class="cal-check-label" title="When checked, 3D model rotation is unconstrained by limits so scaling and rotation angles can be explored freely">
        <input type="checkbox" id="chk-ignore-limits" checked>
        <span>Unconstrained Angles (Ignore Limits)</span>
      </label>
    `;
    const chkIgnore = optionsRow.querySelector('#chk-ignore-limits');
    chkIgnore.checked = true;
    this.robotModel.setIgnoreLimits(true);
    chkIgnore.onchange = () => {
      this.robotModel.setIgnoreLimits(chkIgnore.checked);
      for (const ch of Object.keys(this.cards)) {
        this.updateCardReadouts(parseInt(ch));
      }
    };

    topBar.appendChild(searchInput);
    topBar.appendChild(actionGroup);
    topBar.appendChild(optionsRow);
    this.container.appendChild(topBar);

    // 2. Joint Cards List Container
    const listContainer = document.createElement('div');
    listContainer.className = 'calibrator-cards-list';

    for (const group of GROUPS) {
      const groupSection = document.createElement('div');
      groupSection.className = 'cal-group-section';
      groupSection.id = `cal-group-${group.id}`;

      const groupHeader = document.createElement('div');
      groupHeader.className = 'cal-group-header';
      groupHeader.innerHTML = `
        <span class="cal-group-title">${group.title}</span>
        <span class="cal-group-count">${group.channels.length} Servos</span>
      `;
      groupSection.appendChild(groupHeader);

      for (const ch of group.channels) {
        const cal = this.robotModel.getCalibration(ch);
        const card = this.createJointCard(ch, cal);
        this.cards[ch] = card;
        groupSection.appendChild(card);
      }

      listContainer.appendChild(groupSection);
    }

    this.container.appendChild(listContainer);
    this.applyFilter();
  }

  createJointCard(ch, cal) {
    const card = document.createElement('div');
    card.className = 'cal-joint-card';
    card.id = `cal-card-${ch}`;
    card.dataset.channel = ch;
    card.dataset.name = cal.name.toLowerCase();
    card.dataset.label = cal.label.toLowerCase();

    // Card Header
    const header = document.createElement('div');
    header.className = 'cal-card-header';
    header.innerHTML = `
      <div class="cal-header-left">
        <span class="cal-ch-tag">CH ${ch.toString().padStart(2, '0')}</span>
        <span class="cal-joint-title">${cal.label}</span>
        <span class="cal-joint-linkname">(${cal.name})</span>
      </div>
      <div class="cal-header-right">
        <span class="cal-angle-badge" id="cal-badge-angle-${ch}">0.0°</span>
        <span class="cal-rad-badge" id="cal-badge-rad-${ch}">0.0000 rad</span>
      </div>
    `;
    card.appendChild(header);

    // Card Body
    const body = document.createElement('div');
    body.className = 'cal-card-body';

    // 1. Live Motion Slider
    const motionRow = document.createElement('div');
    motionRow.className = 'cal-motion-row';

    const btnMinus = document.createElement('button');
    btnMinus.className = 'btn-step';
    btnMinus.textContent = '−';
    btnMinus.title = 'Step Decrement PWM (-1)';
    btnMinus.onclick = () => this.stepPwm(ch, -1);

    const slider = document.createElement('input');
    slider.type = 'range';
    slider.min = (ch === 16) ? '0' : '1';
    slider.max = (ch === 16) ? '180' : '270';
    slider.value = this.robotModel.getServoPwm(ch);
    slider.className = 'cal-slider';
    slider.id = `cal-slider-${ch}`;
    this.sliders[ch] = slider;

    slider.oninput = () => {
      const val = parseInt(slider.value, 10);
      this.handleSliderInput(ch, val);
    };

    const btnPlus = document.createElement('button');
    btnPlus.className = 'btn-step';
    btnPlus.textContent = '+';
    btnPlus.title = 'Step Increment PWM (+1)';
    btnPlus.onclick = () => this.stepPwm(ch, 1);

    const pwmBadge = document.createElement('span');
    pwmBadge.className = 'cal-pwm-badge';
    pwmBadge.id = `cal-pwm-${ch}`;
    pwmBadge.textContent = `PWM: ${slider.value}`;
    this.readouts[ch] = pwmBadge;

    motionRow.appendChild(btnMinus);
    motionRow.appendChild(slider);
    motionRow.appendChild(btnPlus);
    motionRow.appendChild(pwmBadge);
    body.appendChild(motionRow);

    // 2. Kinematics Tuning Container
    const kinematicsBox = document.createElement('div');
    kinematicsBox.className = 'cal-kinematics-box';

    // A. Scale Tuning (Dedicated Full-Width Block)
    const scaleSection = document.createElement('div');
    scaleSection.className = 'cal-scale-section';
    scaleSection.innerHTML = `
      <div class="cal-field-header">
        <label class="cal-field-label" title="Degrees rotated per 1 PWM count">
          Kinematic Scale (°/count):
        </label>
        <span class="cal-scale-readout" id="cal-scale-val-${ch}">${cal.degPerPwm.toFixed(4)}°/step</span>
      </div>
    `;

    const scaleSliderRow = document.createElement('div');
    scaleSliderRow.className = 'cal-scale-slider-row';

    const scaleSlider = document.createElement('input');
    scaleSlider.type = 'range';
    scaleSlider.className = 'cal-scale-slider';
    scaleSlider.min = '0.300';
    scaleSlider.max = '2.500';
    scaleSlider.step = '0.005';
    scaleSlider.value = cal.degPerPwm.toFixed(4);
    scaleSlider.id = `cal-scale-slider-${ch}`;

    const scaleInput = document.createElement('input');
    scaleInput.type = 'number';
    scaleInput.className = 'cal-num-input cal-scale-num';
    scaleInput.step = '0.001';
    scaleInput.min = '0.1';
    scaleInput.max = '3.0';
    scaleInput.value = cal.degPerPwm.toFixed(4);
    scaleInput.id = `cal-scale-${ch}`;

    const syncScale = (degPerPwm) => {
      if (isNaN(degPerPwm) || degPerPwm <= 0) return;
      scaleSlider.value = degPerPwm.toFixed(4);
      scaleInput.value = degPerPwm.toFixed(4);
      const readout = document.getElementById(`cal-scale-val-${ch}`);
      if (readout) readout.textContent = `${degPerPwm.toFixed(4)}°/step`;
      this.robotModel.setJointCalibration(ch, { degPerPwm });
      this.updateCardReadouts(ch);
    };

    scaleSlider.oninput = () => {
      const val = parseFloat(scaleSlider.value);
      syncScale(val);
    };

    scaleInput.oninput = () => {
      const val = parseFloat(scaleInput.value);
      if (!isNaN(val) && val > 0) {
        syncScale(val);
      }
    };

    scaleSliderRow.appendChild(scaleSlider);
    scaleSliderRow.appendChild(scaleInput);

    // Quick scale preset pills
    const pillGroup = document.createElement('div');
    pillGroup.className = 'cal-pill-group';

    const pill180 = document.createElement('button');
    pill180.className = 'cal-pill';
    pill180.textContent = '1.0° (π/180)';
    pill180.title = 'Standard leg/head resolution (1.0 deg/count)';
    pill180.onclick = () => syncScale(1.0);

    const pill135 = document.createElement('button');
    pill135.className = 'cal-pill';
    pill135.textContent = '1.33° (π/135)';
    pill135.title = 'Arm resolution (1.3333 deg/count)';
    pill135.onclick = () => syncScale(180 / 135);

    const pill270 = document.createElement('button');
    pill270.className = 'cal-pill';
    pill270.textContent = '0.67° (π/270)';
    pill270.title = 'CAD nominal default (0.6667 deg/count)';
    pill270.onclick = () => syncScale(180 / 270);

    pillGroup.appendChild(pill180);
    pillGroup.appendChild(pill135);
    pillGroup.appendChild(pill270);

    scaleSection.appendChild(scaleSliderRow);
    scaleSection.appendChild(pillGroup);
    kinematicsBox.appendChild(scaleSection);

    // B. Center Datum & Axis Direction (Clean 2-Column Row)
    const datumSignRow = document.createElement('div');
    datumSignRow.className = 'cal-datum-sign-row';

    // Center Datum Column
    const centerCol = document.createElement('div');
    centerCol.className = 'cal-tune-field';
    centerCol.innerHTML = `
      <label class="cal-field-label" title="Zero angle datum pulse position (PWM at 0.0 rad)">
        Center Datum (PWM₀):
      </label>
    `;

    const centerInputGroup = document.createElement('div');
    centerInputGroup.className = 'cal-input-group';

    const centerInput = document.createElement('input');
    centerInput.type = 'number';
    centerInput.className = 'cal-num-input';
    centerInput.min = '1';
    centerInput.max = '270';
    centerInput.value = cal.center;
    centerInput.id = `cal-center-${ch}`;

    centerInput.onchange = () => {
      const center = parseInt(centerInput.value, 10);
      if (!isNaN(center)) {
        this.robotModel.setJointCalibration(ch, { center });
        this.updateCardReadouts(ch);
      }
    };

    const btnSetCenter = document.createElement('button');
    btnSetCenter.className = 'btn-cal-tool';
    btnSetCenter.textContent = 'Current -> Center';
    btnSetCenter.title = 'Set current slider PWM as the zero center datum';
    btnSetCenter.onclick = () => {
      const current = parseInt(this.sliders[ch].value, 10);
      centerInput.value = current;
      this.robotModel.setJointCalibration(ch, { center: current });
      this.updateCardReadouts(ch);
    };

    centerInputGroup.appendChild(centerInput);
    centerInputGroup.appendChild(btnSetCenter);
    centerCol.appendChild(centerInputGroup);
    datumSignRow.appendChild(centerCol);

    // Axis Direction Sign Column
    const signCol = document.createElement('div');
    signCol.className = 'cal-tune-field';
    signCol.innerHTML = `
      <label class="cal-field-label" title="Rotation direction relative to PWM increase">
        Axis Direction:
      </label>
    `;

    const btnSign = document.createElement('button');
    btnSign.className = `btn-sign-toggle ${cal.sign < 0 ? 'sign-inverted' : 'sign-normal'}`;
    btnSign.textContent = cal.sign > 0 ? '+1 (Normal)' : '-1 (Inverted)';
    btnSign.id = `cal-sign-${ch}`;
    btnSign.title = 'Click to flip rotation direction (+1 / -1)';
    btnSign.onclick = () => {
      const newSign = (this.robotModel.getCalibration(ch).sign > 0) ? -1.0 : 1.0;
      this.robotModel.setJointCalibration(ch, { sign: newSign });
      btnSign.className = `btn-sign-toggle ${newSign < 0 ? 'sign-inverted' : 'sign-normal'}`;
      btnSign.textContent = newSign > 0 ? '+1 (Normal)' : '-1 (Inverted)';
      this.updateCardReadouts(ch);
    };

    signCol.appendChild(btnSign);
    datumSignRow.appendChild(signCol);
    kinematicsBox.appendChild(datumSignRow);
    body.appendChild(kinematicsBox);

    // 3. Travel Limits Capture Bar (Lower & Upper)
    const limitBar = document.createElement('div');
    limitBar.className = 'cal-limit-bar';

    // Lower Limit
    const lowerBox = document.createElement('div');
    lowerBox.className = 'cal-limit-box';
    lowerBox.innerHTML = `
      <div class="cal-limit-header">
        <span class="cal-limit-label">Lower Limit (Min)</span>
        <span class="cal-limit-rad" id="cal-lower-rad-${ch}">${cal.lower.toFixed(4)} rad</span>
      </div>
    `;

    const lowerInputGroup = document.createElement('div');
    lowerInputGroup.className = 'cal-input-group';

    const lowerInput = document.createElement('input');
    lowerInput.type = 'number';
    lowerInput.className = 'cal-num-input';
    lowerInput.step = '1';
    lowerInput.value = Math.round(cal.lowerDeg);
    lowerInput.id = `cal-lower-${ch}`;
    lowerInput.onchange = () => {
      const lowerDeg = parseFloat(lowerInput.value);
      if (!isNaN(lowerDeg)) {
        this.robotModel.setJointCalibration(ch, { lowerDeg });
        document.getElementById(`cal-lower-rad-${ch}`).textContent =
          `${(lowerDeg * Math.PI / 180).toFixed(4)} rad`;
      }
    };

    const btnSetLower = document.createElement('button');
    btnSetLower.className = 'btn-cal-tool btn-limit-capture';
    btnSetLower.textContent = 'Set Current';
    btnSetLower.title = 'Capture current joint angle as lower mechanical stop';
    btnSetLower.onclick = () => {
      const angle = this.robotModel.getJointAngle(ch);
      const roundedDeg = Math.round(angle.deg);
      lowerInput.value = roundedDeg;
      this.robotModel.setJointCalibration(ch, { lowerDeg: roundedDeg });
      document.getElementById(`cal-lower-rad-${ch}`).textContent = `${angle.rad.toFixed(4)} rad`;
    };

    lowerInputGroup.appendChild(lowerInput);
    lowerInputGroup.appendChild(btnSetLower);
    lowerBox.appendChild(lowerInputGroup);

    // Upper Limit
    const upperBox = document.createElement('div');
    upperBox.className = 'cal-limit-box';
    upperBox.innerHTML = `
      <div class="cal-limit-header">
        <span class="cal-limit-label">Upper Limit (Max)</span>
        <span class="cal-limit-rad" id="cal-upper-rad-${ch}">${cal.upper.toFixed(4)} rad</span>
      </div>
    `;

    const upperInputGroup = document.createElement('div');
    upperInputGroup.className = 'cal-input-group';

    const upperInput = document.createElement('input');
    upperInput.type = 'number';
    upperInput.className = 'cal-num-input';
    upperInput.step = '1';
    upperInput.value = Math.round(cal.upperDeg);
    upperInput.id = `cal-upper-${ch}`;
    upperInput.onchange = () => {
      const upperDeg = parseFloat(upperInput.value);
      if (!isNaN(upperDeg)) {
        this.robotModel.setJointCalibration(ch, { upperDeg });
        document.getElementById(`cal-upper-rad-${ch}`).textContent =
          `${(upperDeg * Math.PI / 180).toFixed(4)} rad`;
      }
    };

    const btnSetUpper = document.createElement('button');
    btnSetUpper.className = 'btn-cal-tool btn-limit-capture';
    btnSetUpper.textContent = 'Set Current';
    btnSetUpper.title = 'Capture current joint angle as upper mechanical stop';
    btnSetUpper.onclick = () => {
      const angle = this.robotModel.getJointAngle(ch);
      const roundedDeg = Math.round(angle.deg);
      upperInput.value = roundedDeg;
      this.robotModel.setJointCalibration(ch, { upperDeg: roundedDeg });
      document.getElementById(`cal-upper-rad-${ch}`).textContent = `${angle.rad.toFixed(4)} rad`;
    };

    upperInputGroup.appendChild(upperInput);
    upperInputGroup.appendChild(btnSetUpper);
    upperBox.appendChild(upperInputGroup);

    limitBar.appendChild(lowerBox);
    limitBar.appendChild(upperBox);
    body.appendChild(limitBar);

    // 4. Card Footer: Reset Joint Defaults
    const cardFooter = document.createElement('div');
    cardFooter.className = 'cal-card-footer';

    const btnResetJoint = document.createElement('button');
    btnResetJoint.className = 'btn-reset-joint';
    btnResetJoint.textContent = '↺ Reset Joint to Default';
    btnResetJoint.onclick = () => {
      this.robotModel.resetJointCalibration(ch);
      const def = this.robotModel.getCalibration(ch);
      scaleInput.value = def.degPerPwm.toFixed(4);
      scaleSlider.value = def.degPerPwm.toFixed(4);
      const scaleReadout = document.getElementById(`cal-scale-val-${ch}`);
      if (scaleReadout) scaleReadout.textContent = `${def.degPerPwm.toFixed(4)}°`;
      centerInput.value = def.center;
      lowerInput.value = def.lowerDeg.toFixed(1);
      upperInput.value = def.upperDeg.toFixed(1);
      document.getElementById(`cal-lower-rad-${ch}`).textContent = `${def.lower.toFixed(4)} rad`;
      document.getElementById(`cal-upper-rad-${ch}`).textContent = `${def.upper.toFixed(4)} rad`;
      btnSign.className = `btn-sign-toggle ${def.sign < 0 ? 'sign-inverted' : 'sign-normal'}`;
      btnSign.textContent = def.sign > 0 ? '+1 (Normal)' : '-1 (Inverted)';
      this.updateCardReadouts(ch);
    };

    cardFooter.appendChild(btnResetJoint);
    body.appendChild(cardFooter);

    card.appendChild(body);

    // Initial readouts update
    this.updateCardReadouts(ch);

    return card;
  }

  handleSliderInput(chan, val) {
    this.robotModel.setServoPwm(chan, val);
    this.updateCardReadouts(chan);

    if (this.onPwmChange) {
      this.onPwmChange(chan, val);
    }
  }

  stepPwm(chan, delta) {
    const slider = this.sliders[chan];
    if (!slider) return;
    let val = parseInt(slider.value, 10) + delta;
    val = Math.max(parseInt(slider.min, 10), Math.min(parseInt(slider.max, 10), val));
    slider.value = val;
    this.handleSliderInput(chan, val);
  }

  updateCardReadouts(chan) {
    const pwmBadge = this.readouts[chan];
    const slider = this.sliders[chan];
    if (pwmBadge && slider) {
      pwmBadge.textContent = `PWM: ${slider.value}`;
    }

    const angle = this.robotModel.getJointAngle(chan);
    const badgeAngle = document.getElementById(`cal-badge-angle-${chan}`);
    const badgeRad = document.getElementById(`cal-badge-rad-${chan}`);

    if (badgeAngle) {
      const signStr = angle.deg > 0 ? '+' : '';
      badgeAngle.textContent = `${signStr}${angle.deg.toFixed(1)}°`;
    }
    if (badgeRad) {
      const signStr = angle.rad > 0 ? '+' : '';
      badgeRad.textContent = `${signStr}${angle.rad.toFixed(4)} rad`;
    }
  }

  setPwmValue(chan, val) {
    const slider = this.sliders[chan];
    if (slider) {
      slider.value = val;
      this.updateCardReadouts(chan);
    }
  }

  setAllPwmValues(pwmArray) {
    if (!pwmArray) return;
    for (let i = 0; i < 17 && i < pwmArray.length; i++) {
      this.setPwmValue(i, pwmArray[i]);
    }
  }

  applyFilter() {
    for (const [ch, card] of Object.entries(this.cards)) {
      if (!this.filterQuery) {
        card.style.display = '';
        continue;
      }
      const match =
        card.dataset.channel.includes(this.filterQuery) ||
        card.dataset.name.includes(this.filterQuery) ||
        card.dataset.label.includes(this.filterQuery);
      card.style.display = match ? '' : 'none';
    }
  }

  generateUrdfXml() {
    const lines = [
      '<!-- RoboHero Calibrated Joint Limits for urdf/robohero.urdf -->',
      '<!-- Replace the corresponding <limit ... /> tag in each joint definition -->',
      '',
    ];

    const cals = this.robotModel.getAllCalibrations();
    for (let i = 0; i < 17; i++) {
      const cal = cals[i];
      if (!cal) continue;
      const lower = Math.min(cal.lower, cal.upper).toFixed(4);
      const upper = Math.max(cal.lower, cal.upper).toFixed(4);
      lines.push(`<!-- CH ${i.toString().padStart(2, '0')}: ${cal.label} (${cal.name}) -->`);
      lines.push(`<!-- Measured Range: [${cal.lowerDeg.toFixed(1)}°, ${cal.upperDeg.toFixed(1)}°] | Scale: ${cal.degPerPwm.toFixed(4)}°/PWM -->`);
      lines.push(`<joint name="${cal.name}" ...>`);
      lines.push(`  <limit lower="${lower}" upper="${upper}" effort="1.2" velocity="3.14"/>`);
      lines.push(`</joint>`);
      lines.push('');
    }

    return lines.join('\n');
  }

  generateChannelMapJs() {
    const lines = [
      '// Calibrated CHANNEL_MAP snippet for app/web/src/viewer/RobotModel.js',
      'export const CHANNEL_MAP = {',
    ];

    const cals = this.robotModel.getAllCalibrations();
    for (let i = 0; i < 17; i++) {
      const cal = cals[i];
      if (!cal) continue;
      const radPerPwmStr = Math.abs(cal.degPerPwm - 1.0) < 0.01
        ? '(Math.PI / 180)'
        : Math.abs(cal.degPerPwm - 1.3333) < 0.01
          ? '(Math.PI / 135)'
          : `(${cal.radPerPwm.toFixed(8)})`;
      const signStr = cal.sign > 0 ? ' 1.0' : '-1.0';
      const lower = Math.min(cal.lower, cal.upper).toFixed(4);
      const upper = Math.max(cal.lower, cal.upper).toFixed(4);

      lines.push(
        `  ${i.toString().padEnd(2, ' ')}: { name: '${cal.name}', ` +
        `group: '${cal.group}', label: '${cal.label}', ` +
        `center: ${cal.center.toString().padStart(3, ' ')}, ` +
        `sign: ${signStr}, radPerPwm: ${radPerPwmStr}, ` +
        `lower: ${lower}, upper: ${upper} },`
      );
    }

    lines.push('};');
    return lines.join('\n');
  }

  generateCalibrationJson() {
    const cals = this.robotModel.getAllCalibrations();
    const output = {
      timestamp: new Date().toISOString(),
      robot_id: 'TTR-ee40',
      channels: {},
    };

    for (let i = 0; i < 17; i++) {
      const cal = cals[i];
      if (!cal) continue;
      output.channels[i] = {
        name: cal.name,
        label: cal.label,
        group: cal.group,
        center_pwm: cal.center,
        sign: cal.sign,
        deg_per_pwm: cal.degPerPwm,
        rad_per_pwm: cal.radPerPwm,
        lower_deg: Math.min(cal.lowerDeg, cal.upperDeg),
        upper_deg: Math.max(cal.lowerDeg, cal.upperDeg),
        urdf_limits: {
          lower: parseFloat(Math.min(cal.lower, cal.upper).toFixed(4)),
          upper: parseFloat(Math.max(cal.lower, cal.upper).toFixed(4)),
        },
      };
    }

    return JSON.stringify(output, null, 2);
  }
}
