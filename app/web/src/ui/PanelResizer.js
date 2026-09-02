/*
 * PanelResizer.js
 *
 * Copyright (C) 2026, Charles Chiou
 *
 * Interactive split-pane resizer management with localStorage persistence,
 * smooth dragging, and Three.js canvas auto-adaptation.
 */

const STORAGE_KEYS = {
  SIDEBAR_WIDTH: 'robohero_sidebar_width',
  SIDEBAR_TOP_HEIGHT: 'robohero_sidebar_top_height',
  DOCK_HEIGHT: 'robohero_dock_height',
};

const DEFAULTS = {
  SIDEBAR_WIDTH: 380,
  SIDEBAR_TOP_HEIGHT: 385,
  DOCK_HEIGHT: 140,
};

export class PanelResizer {
  constructor() {
    this.resizerSidebarH = document.getElementById('resizer-sidebar-h');
    this.resizerSidebarV = document.getElementById('resizer-sidebar-v');
    this.resizerDockV = document.getElementById('resizer-dock-v');

    this.sidebarEl = document.getElementById('control-sidebar');
    this.sidebarTopPane = document.querySelector('.sidebar-top-pane');

    this.init();
  }

  init() {
    this.loadSavedSizes();
    this.setupSidebarH();
    this.setupSidebarV();
    this.setupDockV();
  }

  loadSavedSizes() {
    const savedWidth = parseInt(localStorage.getItem(STORAGE_KEYS.SIDEBAR_WIDTH), 10);
    const savedTopHeight = parseInt(localStorage.getItem(STORAGE_KEYS.SIDEBAR_TOP_HEIGHT), 10);
    const savedDockHeight = parseInt(localStorage.getItem(STORAGE_KEYS.DOCK_HEIGHT), 10);

    const width = (!isNaN(savedWidth) && savedWidth >= 260 && savedWidth <= 800)
      ? savedWidth : DEFAULTS.SIDEBAR_WIDTH;
    const topHeight = (!isNaN(savedTopHeight) && savedTopHeight >= 120 && savedTopHeight <= 700)
      ? savedTopHeight : DEFAULTS.SIDEBAR_TOP_HEIGHT;
    const dockHeight = (!isNaN(savedDockHeight) && savedDockHeight >= 60 && savedDockHeight <= 500)
      ? savedDockHeight : DEFAULTS.DOCK_HEIGHT;

    document.documentElement.style.setProperty('--sidebar-width', `${width}px`);
    document.documentElement.style.setProperty('--sidebar-top-height', `${topHeight}px`);
    document.documentElement.style.setProperty('--dock-height', `${dockHeight}px`);
  }

  /* 1. Horizontal Resizer: Sidebar Width */
  setupSidebarH() {
    if (!this.resizerSidebarH) return;

    let isDragging = false;
    let startX = 0;
    let startWidth = 0;

    const onMouseDown = (e) => {
      isDragging = true;
      startX = e.clientX || (e.touches && e.touches[0].clientX);
      startWidth = this.sidebarEl ? this.sidebarEl.getBoundingClientRect().width : DEFAULTS.SIDEBAR_WIDTH;

      document.body.style.cursor = 'col-resize';
      document.body.style.userSelect = 'none';
      this.resizerSidebarH.classList.add('active');

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
      window.addEventListener('touchmove', onMouseMove);
      window.addEventListener('touchend', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!isDragging) return;
      const clientX = e.clientX !== undefined ? e.clientX : (e.touches && e.touches[0].clientX);
      if (clientX === undefined) return;

      const deltaX = clientX - startX;
      const maxAllowed = Math.min(window.innerWidth - 320, 750);
      const newWidth = Math.max(260, Math.min(maxAllowed, Math.round(startWidth + deltaX)));

      document.documentElement.style.setProperty('--sidebar-width', `${newWidth}px`);
      window.dispatchEvent(new Event('resize'));
    };

    const onMouseUp = () => {
      if (!isDragging) return;
      isDragging = false;

      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      this.resizerSidebarH.classList.remove('active');

      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
      window.removeEventListener('touchmove', onMouseMove);
      window.removeEventListener('touchend', onMouseUp);

      const curWidth = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sidebar-width'), 10);
      if (!isNaN(curWidth)) {
        localStorage.setItem(STORAGE_KEYS.SIDEBAR_WIDTH, curWidth);
      }
      window.dispatchEvent(new Event('resize'));
    };

    this.resizerSidebarH.addEventListener('mousedown', onMouseDown);
    this.resizerSidebarH.addEventListener('touchstart', onMouseDown, { passive: false });

    // Double-click to reset default width
    this.resizerSidebarH.addEventListener('dblclick', () => {
      document.documentElement.style.setProperty('--sidebar-width', `${DEFAULTS.SIDEBAR_WIDTH}px`);
      localStorage.setItem(STORAGE_KEYS.SIDEBAR_WIDTH, DEFAULTS.SIDEBAR_WIDTH);
      window.dispatchEvent(new Event('resize'));
    });
  }

  /* 2. Vertical Resizer: Sidebar Top vs Sliders Height */
  setupSidebarV() {
    if (!this.resizerSidebarV) return;

    let isDragging = false;
    let startY = 0;
    let startHeight = 0;

    const onMouseDown = (e) => {
      isDragging = true;
      startY = e.clientY || (e.touches && e.touches[0].clientY);
      startHeight = this.sidebarTopPane ? this.sidebarTopPane.getBoundingClientRect().height : DEFAULTS.SIDEBAR_TOP_HEIGHT;

      document.body.style.cursor = 'row-resize';
      document.body.style.userSelect = 'none';
      this.resizerSidebarV.classList.add('active');

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
      window.addEventListener('touchmove', onMouseMove);
      window.addEventListener('touchend', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!isDragging) return;
      const clientY = e.clientY !== undefined ? e.clientY : (e.touches && e.touches[0].clientY);
      if (clientY === undefined) return;

      const deltaY = clientY - startY;
      const sidebarHeight = this.sidebarEl ? this.sidebarEl.getBoundingClientRect().height : window.innerHeight;
      const maxAllowed = Math.max(160, sidebarHeight - 140);
      const newHeight = Math.max(120, Math.min(maxAllowed, Math.round(startHeight + deltaY)));

      document.documentElement.style.setProperty('--sidebar-top-height', `${newHeight}px`);
    };

    const onMouseUp = () => {
      if (!isDragging) return;
      isDragging = false;

      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      this.resizerSidebarV.classList.remove('active');

      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
      window.removeEventListener('touchmove', onMouseMove);
      window.removeEventListener('touchend', onMouseUp);

      const curHeight = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sidebar-top-height'), 10);
      if (!isNaN(curHeight)) {
        localStorage.setItem(STORAGE_KEYS.SIDEBAR_TOP_HEIGHT, curHeight);
      }
    };

    this.resizerSidebarV.addEventListener('mousedown', onMouseDown);
    this.resizerSidebarV.addEventListener('touchstart', onMouseDown, { passive: false });

    // Double click to reset default height
    this.resizerSidebarV.addEventListener('dblclick', () => {
      document.documentElement.style.setProperty('--sidebar-top-height', `${DEFAULTS.SIDEBAR_TOP_HEIGHT}px`);
      localStorage.setItem(STORAGE_KEYS.SIDEBAR_TOP_HEIGHT, DEFAULTS.SIDEBAR_TOP_HEIGHT);
    });
  }

  /* 3. Vertical Resizer: Sequencer Dock Height */
  setupDockV() {
    if (!this.resizerDockV) return;

    let isDragging = false;
    let startY = 0;
    let startHeight = 0;

    const onMouseDown = (e) => {
      isDragging = true;
      startY = e.clientY || (e.touches && e.touches[0].clientY);
      const curDock = document.getElementById('sequencer-dock');
      startHeight = curDock ? curDock.getBoundingClientRect().height : DEFAULTS.DOCK_HEIGHT;

      document.body.style.cursor = 'row-resize';
      document.body.style.userSelect = 'none';
      this.resizerDockV.classList.add('active');

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
      window.addEventListener('touchmove', onMouseMove);
      window.addEventListener('touchend', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!isDragging) return;
      const clientY = e.clientY !== undefined ? e.clientY : (e.touches && e.touches[0].clientY);
      if (clientY === undefined) return;

      const deltaY = clientY - startY;
      const maxAllowed = Math.min(window.innerHeight - 200, 420);
      const newHeight = Math.max(60, Math.min(maxAllowed, Math.round(startHeight - deltaY)));

      document.documentElement.style.setProperty('--dock-height', `${newHeight}px`);
      window.dispatchEvent(new Event('resize'));
    };

    const onMouseUp = () => {
      if (!isDragging) return;
      isDragging = false;

      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      this.resizerDockV.classList.remove('active');

      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
      window.removeEventListener('touchmove', onMouseMove);
      window.removeEventListener('touchend', onMouseUp);

      const curHeight = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--dock-height'), 10);
      if (!isNaN(curHeight)) {
        localStorage.setItem(STORAGE_KEYS.DOCK_HEIGHT, curHeight);
      }
      window.dispatchEvent(new Event('resize'));
    };

    this.resizerDockV.addEventListener('mousedown', onMouseDown);
    this.resizerDockV.addEventListener('touchstart', onMouseDown, { passive: false });

    // Double click to reset default dock height
    this.resizerDockV.addEventListener('dblclick', () => {
      document.documentElement.style.setProperty('--dock-height', `${DEFAULTS.DOCK_HEIGHT}px`);
      localStorage.setItem(STORAGE_KEYS.DOCK_HEIGHT, DEFAULTS.DOCK_HEIGHT);
      window.dispatchEvent(new Event('resize'));
    });
  }
}
