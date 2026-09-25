let emulator = null;

document.addEventListener('DOMContentLoaded', () => {
  const canvas = document.getElementById('canvas');
  const statusEl = document.getElementById('statusText');
  const errorEl = document.getElementById('errorBanner');
  const gamepadEl = document.getElementById('gamepadStatus');
  const startBtn = document.getElementById('startBtn');
  const pauseBtn = document.getElementById('pauseBtn');
  const resetBtn = document.getElementById('resetBtn');
  const audioBtn = document.getElementById('audioBtn');
  const volumeSlider = document.getElementById('volumeSlider');
  const dropZone = document.getElementById('dropZone');
  const fileInput = document.getElementById('fileInput');

  // Initialize MREmu API
  emulator = new MREmu({
    canvas: canvas,
    wasmScriptUrl: 'mremu.js'
  });

  async function initEmulator() {
    try {
      updateStatus('Initializing WebAssembly runtime...');
      await emulator.init();
      emulator.start();
      updateStatus('Emulator Active (Ready)');
      hideError();
    } catch (err) {
      showError('Failed to initialize WebAssembly emulator: ' + err.message);
    }
  }

  function updateStatus(text) {
    if (statusEl) {
      statusEl.innerText = text;
    }
  }

  function showError(msg) {
    if (errorEl) {
      errorEl.innerText = msg;
      errorEl.style.display = 'block';
    }
    updateStatus('Error');
  }

  function hideError() {
    if (errorEl) {
      errorEl.style.display = 'none';
    }
  }

  // Toolbar Button Event Listeners
  startBtn.addEventListener('click', () => {
    emulator.start();
    updateStatus('Emulator Running');
    hideError();
  });

  pauseBtn.addEventListener('click', () => {
    if (emulator.getStatus() === 'running') {
      emulator.pause();
      pauseBtn.innerText = 'Resume';
      updateStatus('Emulator Paused');
    } else if (emulator.getStatus() === 'paused') {
      emulator.start();
      pauseBtn.innerText = 'Pause';
      updateStatus('Emulator Running');
    }
  });

  resetBtn.addEventListener('click', () => {
    emulator.reset();
    emulator.start();
    pauseBtn.innerText = 'Pause';
    updateStatus('Emulator Reset');
    hideError();
  });

  // Audio Controls
  audioBtn.addEventListener('click', () => {
    try {
      emulator.enableAudio();
      audioBtn.innerText = '🔊 Audio Active';
      audioBtn.classList.remove('btn-secondary');
      audioBtn.classList.add('btn');
      hideError();
    } catch (err) {
      showError('Audio initialization failed: ' + err.message);
    }
  });

  volumeSlider.addEventListener('input', (e) => {
    const vol = parseFloat(e.target.value);
    emulator.setVolume(vol);
  });

  // Client-Side File Selector & Drag and Drop
  dropZone.addEventListener('click', () => fileInput.click());

  dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('hover');
  });

  dropZone.addEventListener('dragleave', (e) => {
    e.preventDefault();
    dropZone.classList.remove('hover');
  });

  dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('hover');
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      handleSelectedFile(e.dataTransfer.files[0]);
    }
  });

  fileInput.addEventListener('change', (e) => {
    if (e.target.files && e.target.files.length > 0) {
      handleSelectedFile(e.target.files[0]);
    }
  });

  async function handleSelectedFile(file) {
    try {
      updateStatus(`Loading ${file.name}...`);
      hideError();
      await emulator.load(file);
      emulator.start();
      updateStatus(`Running: ${file.name}`);
    } catch (err) {
      showError(`File load error: ${err.message}`);
    }
  }

  // Keyboard Event Handling
  const keyMap = {
    'ArrowUp': 1,
    'ArrowDown': 2,
    'ArrowLeft': 3,
    'ArrowRight': 4,
    'Enter': 5,
    'Space': 5,
    'Digit0': 48,
    'Digit1': 49,
    'Digit2': 50,
    'Digit3': 51,
    'Digit4': 52,
    'Digit5': 53,
    'Digit6': 54,
    'Digit7': 55,
    'Digit8': 56,
    'Digit9': 57,
    'KeyQ': 1001,
    'KeyE': 1002
  };

  window.addEventListener('keydown', (e) => {
    if (keyMap[e.code] !== undefined) {
      e.preventDefault();
      emulator.setKey(keyMap[e.code], 0);
    }
  });

  window.addEventListener('keyup', (e) => {
    if (keyMap[e.code] !== undefined) {
      e.preventDefault();
      emulator.setKey(keyMap[e.code], 1);
    }
  });

  // Gamepad Connection Indicator
  window.addEventListener('gamepadconnected', (e) => {
    if (gamepadEl) {
      gamepadEl.innerText = `🎮 Gamepad Connected (${e.gamepad.id})`;
      gamepadEl.classList.add('active');
    }
    pollGamepads();
  });

  window.addEventListener('gamepaddisconnected', () => {
    if (gamepadEl) {
      gamepadEl.innerText = '🎮 No Gamepad Detected';
      gamepadEl.classList.remove('active');
    }
  });

  const lastGpState = {};
  function pollGamepads() {
    const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
    for (let i = 0; i < gamepads.length; i++) {
      const gp = gamepads[i];
      if (!gp) continue;

      checkGpButton(gp.id, 0, gp.buttons[0].pressed, 5);
      checkGpButton(gp.id, 12, gp.buttons[12]?.pressed, 1);
      checkGpButton(gp.id, 13, gp.buttons[13]?.pressed, 2);
      checkGpButton(gp.id, 14, gp.buttons[14]?.pressed, 3);
      checkGpButton(gp.id, 15, gp.buttons[15]?.pressed, 4);
      checkGpButton(gp.id, 4, gp.buttons[4]?.pressed, 1001);
      checkGpButton(gp.id, 5, gp.buttons[5]?.pressed, 1002);
    }
    requestAnimationFrame(pollGamepads);
  }

  function checkGpButton(gpId, btnIdx, isPressed, mreKey) {
    const stateKey = `${gpId}_${btnIdx}`;
    const wasPressed = !!lastGpState[stateKey];
    if (isPressed && !wasPressed) {
      emulator.setKey(mreKey, 0);
      lastGpState[stateKey] = true;
    } else if (!isPressed && wasPressed) {
      emulator.setKey(mreKey, 1);
      lastGpState[stateKey] = false;
    }
  }

  initEmulator();
});
