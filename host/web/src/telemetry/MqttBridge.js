/*
 * MqttBridge.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * MQTT WebSocket client & packed binary ABI frame encoder/decoder.
 * Matches include/robohero/msg.h wire format.
 */

import mqtt from 'mqtt';

export const RH_MSG_MAGIC = 0x31424852; // 'RHB1' LE

export const RH_MSG_STATUS  = 1;
export const RH_MSG_STOP    = 2;
export const RH_MSG_CENTER  = 3;
export const RH_MSG_ZERO    = 4;
export const RH_MSG_RELAX   = 5;
export const RH_MSG_PM      = 6;
export const RH_MSG_PMS     = 7;
export const RH_MSG_SET_PWM = 8;

export const RH_TLV_TIME    = 1;
export const RH_TLV_VOLTAGE = 2;
export const RH_TLV_SERVO   = 3;
export const RH_TLV_PROG    = 4;

export class MqttBridge {
  constructor() {
    this.client = null;
    this.isConnected = false;
    this.config = {
      brokerUrl: 'ws://localhost:9001',
      robotId: 'robohero',
      username: '',
      password: '',
      autoConnect: false,
    };

    this.onStatusChange = null;
    this.onServoUpdate = null;
    this.onVoltageUpdate = null;
  }

  loadConfig() {
    try {
      const saved = localStorage.getItem('robohero_mqtt_cfg');
      if (saved) {
        this.config = { ...this.config, ...JSON.parse(saved) };
      }
    } catch (e) {
      console.warn('Could not load MQTT config from localStorage', e);
    }
    return this.config;
  }

  saveConfig(newCfg) {
    this.config = { ...this.config, ...newCfg };
    try {
      localStorage.setItem('robohero_mqtt_cfg', JSON.stringify(this.config));
    } catch (e) {
      console.warn('Could not save MQTT config to localStorage', e);
    }
  }

  connect(customCfg = null) {
    if (customCfg) {
      this.saveConfig(customCfg);
    }

    if (this.client) {
      try {
        this.client.end(true);
      } catch (_) {}
    }

    const opts = {
      clientId: 'robohero_web_' + Math.random().toString(16).substring(2, 8),
      clean: true,
      reconnectPeriod: 4000,
      connectTimeout: 5000,
    };

    if (this.config.username) {
      opts.username = this.config.username;
    }
    if (this.config.password) {
      opts.password = this.config.password;
    }

    if (this.onStatusChange) this.onStatusChange('connecting');

    try {
      this.client = mqtt.connect(this.config.brokerUrl, opts);

      this.client.on('connect', () => {
        this.isConnected = true;
        if (this.onStatusChange) this.onStatusChange('connected');
        const statusTopic = `${this.config.robotId}/status`;
        this.client.subscribe(statusTopic, (err) => {
          if (err) console.error(`Failed to subscribe to ${statusTopic}`, err);
          else console.log(`Subscribed to ${statusTopic}`);
        });
      });

      this.client.on('close', () => {
        this.isConnected = false;
        if (this.onStatusChange) this.onStatusChange('disconnected');
      });

      this.client.on('error', (err) => {
        console.error('MQTT Error:', err);
        this.isConnected = false;
        if (this.onStatusChange) this.onStatusChange('error', err);
      });

      this.client.on('message', (topic, payload) => {
        this.handleMessage(topic, payload);
      });
    } catch (err) {
      console.error('Failed to initiate MQTT connection:', err);
      if (this.onStatusChange) this.onStatusChange('error', err);
    }
  }

  disconnect() {
    if (this.client) {
      this.client.end(true);
      this.client = null;
      this.isConnected = false;
      if (this.onStatusChange) this.onStatusChange('disconnected');
    }
  }

  handleMessage(topic, payload) {
    if (payload.byteLength < 6) return;
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);

    const magic = view.getUint32(0, true);
    if (magic !== RH_MSG_MAGIC) return;

    const msgType = view.getUint8(4);
    const payloadLen = view.getUint8(5);

    if (payload.byteLength < 6 + payloadLen) return;

    let off = 6;
    while (off + 2 <= 6 + payloadLen) {
      const type = view.getUint8(off);
      const len = view.getUint8(off + 1);
      if (off + 2 + len > 6 + payloadLen) break;

      if (type === RH_TLV_SERVO && len === 3) {
        const chan = view.getUint8(off + 2);
        const pos = view.getInt16(off + 3, true);
        if (this.onServoUpdate) {
          this.onServoUpdate(chan, pos);
        }
      } else if (type === RH_TLV_VOLTAGE && len === 2) {
        const volt = view.getInt16(off + 2, true);
        if (this.onVoltageUpdate) {
          this.onVoltageUpdate(volt);
        }
      }
      off += 2 + len;
    }
  }

  sendSimpleCmd(msgType) {
    if (!this.isConnected || !this.client) return false;
    const buf = new Uint8Array(6);
    const view = new DataView(buf.buffer);
    view.setUint32(0, RH_MSG_MAGIC, true);
    view.setUint8(4, msgType);
    view.setUint8(5, 0); // payload len = 0

    const cmdTopic = `${this.config.robotId}/cmd`;
    this.client.publish(cmdTopic, buf);
    return true;
  }

  sendSetPwm(chan, pos) {
    if (!this.isConnected || !this.client) return false;
    const buf = new Uint8Array(6 + 5); // header(6) + TLV_SERVO(2+3)
    const view = new DataView(buf.buffer);
    view.setUint32(0, RH_MSG_MAGIC, true);
    view.setUint8(4, RH_MSG_SET_PWM);
    view.setUint8(5, 5); // payload len

    view.setUint8(6, RH_TLV_SERVO);
    view.setUint8(7, 3);
    view.setUint8(8, chan);
    view.setInt16(9, pos, true);

    const cmdTopic = `${this.config.robotId}/cmd`;
    this.client.publish(cmdTopic, buf);
    return true;
  }
}
