/*
 * SceneManager.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Three.js scene setup: renderer, lighting, shadows, floor grid, camera presets.
 */

import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';

export class SceneManager {
  constructor(containerElement, onObjectClick) {
    this.container = containerElement;
    this.onObjectClick = onObjectClick;

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x0a0f1d);
    this.scene.fog = new THREE.FogExp2(0x0a0f1d, 0.4);

    this.initCamera();
    this.initRenderer();
    this.initControls();
    this.initLighting();
    this.initEnvironment();
    this.initRaycaster();

    window.addEventListener('resize', this.onWindowResize.bind(this));
    this.animate = this.animate.bind(this);
    this.animate();
  }

  initCamera() {
    const aspect = this.container.clientWidth / this.container.clientHeight;
    this.camera = new THREE.PerspectiveCamera(45, aspect, 0.05, 50);
    this.setCameraPreset('persp');
  }

  initRenderer() {
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.1;
    this.container.appendChild(this.renderer.domElement);
  }

  initControls() {
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.05;
    this.controls.maxPolarAngle = Math.PI / 2 + 0.05; // don't go far below floor
    this.controls.minDistance = 0.2;
    this.controls.maxDistance = 2.5;
    this.controls.target.set(0, 0.12, 0); // Focus on robot torso/center
  }

  initLighting() {
    // Ambient Light
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.7);
    this.scene.add(ambientLight);

    // Main Key Light (Directional with shadows)
    this.keyLight = new THREE.DirectionalLight(0xffffff, 1.6);
    this.keyLight.position.set(0.6, 1.2, 0.8);
    this.keyLight.castShadow = true;
    this.keyLight.shadow.mapSize.width = 2048;
    this.keyLight.shadow.mapSize.height = 2048;
    this.keyLight.shadow.camera.near = 0.1;
    this.keyLight.shadow.camera.far = 4;
    this.keyLight.shadow.camera.left = -0.5;
    this.keyLight.shadow.camera.right = 0.5;
    this.keyLight.shadow.camera.top = 0.5;
    this.keyLight.shadow.camera.bottom = -0.5;
    this.keyLight.shadow.bias = -0.0005;
    this.scene.add(this.keyLight);

    // Fill Light
    const fillLight = new THREE.DirectionalLight(0x06b6d4, 0.8);
    fillLight.position.set(-0.8, 0.5, -0.6);
    this.scene.add(fillLight);

    // Rim / Accent Blue Light
    const rimLight = new THREE.DirectionalLight(0x3b82f6, 0.9);
    rimLight.position.set(0, 0.8, -1.0);
    this.scene.add(rimLight);
  }

  initEnvironment() {
    // Floor Shadow Receiver
    const planeGeo = new THREE.PlaneGeometry(10, 10);
    const planeMat = new THREE.ShadowMaterial({ opacity: 0.35 });
    const shadowFloor = new THREE.Mesh(planeGeo, planeMat);
    shadowFloor.rotation.x = -Math.PI / 2;
    shadowFloor.position.y = 0;
    shadowFloor.receiveShadow = true;
    this.scene.add(shadowFloor);

    // Grid Helper
    const gridHelper = new THREE.GridHelper(3, 30, 0x06b6d4, 0x1e293b);
    gridHelper.position.y = 0.001;
    this.scene.add(gridHelper);
  }

  initRaycaster() {
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2();

    this.renderer.domElement.addEventListener('pointerdown', (e) => {
      const rect = this.renderer.domElement.getBoundingClientRect();
      this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

      this.raycaster.setFromCamera(this.mouse, this.camera);
      const intersects = this.raycaster.intersectObjects(this.scene.children, true);
      for (const hit of intersects) {
        if (hit.object && hit.object.type === 'Mesh' && hit.object.parent) {
          let cur = hit.object;
          while (cur && !cur.isURDFJoint && !cur.isURDFLink && cur.parent) {
            cur = cur.parent;
          }
          if (cur && this.onObjectClick) {
            this.onObjectClick(cur);
            break;
          }
        }
      }
    });
  }

  setCameraPreset(type) {
    switch (type) {
      case 'front':
        // Robot faces +X in Three.js world coordinates (chest is at +X)
        this.camera.position.set(0.55, 0.14, 0);
        break;
      case 'side':
        // Robot's right side profile is viewed along +Z
        this.camera.position.set(0, 0.14, 0.55);
        break;
      case 'top':
        // Looking straight down from above (+Y)
        this.camera.position.set(0.01, 0.65, 0);
        break;
      case 'persp':
      default:
        // Three-quarter perspective showing front (+X) and side (+Z)
        this.camera.position.set(0.42, 0.28, 0.42);
        break;
    }
    if (this.controls) {
      this.controls.target.set(0, 0.12, 0);
      this.controls.update();
    }
  }

  onWindowResize() {
    if (!this.container) return;
    const width = this.container.clientWidth;
    const height = this.container.clientHeight;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  animate() {
    requestAnimationFrame(this.animate);
    if (this.controls) {
      this.controls.update();
    }
    this.renderer.render(this.scene, this.camera);
  }
}
