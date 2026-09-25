/**
 * MREmu WebAssembly Emulator JavaScript API Wrapper
 * Optimized zero-allocation rendering and memory inspection pipeline.
 */
class MREmu {
  /**
   * @param {Object} options Configuration options
   * @param {HTMLCanvasElement} [options.canvas] Target HTML Canvas element
   * @param {string} [options.wasmScriptUrl='mremu.js'] URL to emscripten JS loader script
   */
  constructor(options = {}) {
    this.canvas = options.canvas || null;
    this.wasmScriptUrl = options.wasmScriptUrl || 'mremu.js';
    this.status = 'uninitialized';
    this.volume = 1.0;
    this.audioCtx = null;
    this.audioNode = null;
    this.audioEnabled = false;
    this.module = null;
    this.renderAnimFrameId = null;
    this.imageData = null;
    this.pixelUint32Buffer = null;
    this.activeFileName = null;

    // Fast-path cached function wrappers
    this._getBuffer = null;
    this._getWidth = null;
    this._getHeight = null;
    this._sendKey = null;
    this._sendPen = null;
    this._loadFile = null;
    this._getAudioBuffer = null;
  }

  async init() {
    if (this.status !== 'uninitialized' && this.status !== 'error') {
      return;
    }

    this.status = 'loading';

    try {
      if (!window.Module) {
        window.Module = {};
      }

      const self = this;
      window.Module.canvas = this.canvas;
      window.Module.print = (text) => console.log('[MREmu WASM]: ' + text);
      window.Module.printErr = (text) => console.error('[MREmu WASM Error]: ' + text);
      window.Module.setStatus = (text) => {
        if (text === 'Ready' && self.status === 'loading') {
          self.status = 'ready';
        }
      };

      if (typeof window.Module.cwrap !== 'function') {
        await new Promise((resolve, reject) => {
          const script = document.createElement('script');
          script.src = self.wasmScriptUrl;
          script.onload = resolve;
          script.onerror = () => reject(new Error(`Failed to load WASM script from ${self.wasmScriptUrl}`));
          document.body.appendChild(script);
        });
      }

      this.module = window.Module;
      this._bindWasmFunctions();
      this.status = 'ready';
    } catch (err) {
      this.status = 'error';
      throw new Error(`MREmu WASM Initialization Error: ${err.message}`);
    }
  }

  _bindWasmFunctions() {
    if (this.module && typeof this.module.cwrap === 'function') {
      this._getBuffer = this.module.cwrap('mremu_wasm_get_screen_buffer', 'number', []);
      this._getWidth = this.module.cwrap('mremu_wasm_get_screen_width', 'number', []);
      this._getHeight = this.module.cwrap('mremu_wasm_get_screen_height', 'number', []);
      this._sendKey = this.module.cwrap('mremu_wasm_send_key_event', null, ['number', 'number']);
      this._sendPen = this.module.cwrap('mremu_wasm_send_pen_event', null, ['number', 'number', 'number']);
      this._loadFile = this.module.cwrap('mremu_wasm_load_file', 'number', ['string', 'number', 'number']);
      this._getAudioBuffer = this.module.cwrap('mremu_wasm_get_audio_buffer', 'number', []);
    }
  }

  async load(fileOrBuffer, filename = 'app.vxp') {
    if (this.status === 'uninitialized') {
      await this.init();
    }

    let arrayBuffer;
    let name = filename;

    if (typeof File !== 'undefined' && fileOrBuffer instanceof File) {
      name = fileOrBuffer.name;
      arrayBuffer = await fileOrBuffer.arrayBuffer();
    } else if (fileOrBuffer instanceof ArrayBuffer) {
      arrayBuffer = fileOrBuffer;
    } else if (fileOrBuffer instanceof Uint8Array) {
      arrayBuffer = fileOrBuffer.buffer;
    } else {
      throw new Error('Invalid file format. Expected File, ArrayBuffer, or Uint8Array.');
    }

    const uint8Array = new Uint8Array(arrayBuffer);
    if (uint8Array.length < 4) {
      throw new Error('Corrupt or empty file buffer.');
    }

    const ptr = this.module._malloc(uint8Array.length);
    this.module.HEAPU8.set(uint8Array, ptr);

    this._bindWasmFunctions();

    if (!this._loadFile) {
      this.module._free(ptr);
      throw new Error('WASM load function wrapper not bound.');
    }

    const res = this._loadFile(name, ptr, uint8Array.length);
    this.module._free(ptr);

    if (res === 1) {
      this.activeFileName = name;
      return true;
    } else {
      throw new Error(`Failed to load ${name} into MREmu MEMFS.`);
    }
  }

  start() {
    if (this.status === 'running') return;

    this.status = 'running';
    if (this.canvas) {
      this._startRenderingLoop();
    }
  }

  pause() {
    if (this.status !== 'running') return;

    this.status = 'paused';
    if (this.renderAnimFrameId) {
      cancelAnimationFrame(this.renderAnimFrameId);
      this.renderAnimFrameId = null;
    }
  }

  reset() {
    this.pause();
    this.imageData = null;
    this.pixelUint32Buffer = null;
    this.activeFileName = null;
    this.status = 'ready';
  }

  setKey(keycode, eventType = 0) {
    if (this._sendKey) {
      this._sendKey(keycode, eventType);
    }
  }

  setPen(x, y, eventType) {
    if (this._sendPen) {
      this._sendPen(x, y, eventType);
    }
  }

  setVolume(vol) {
    this.volume = Math.max(0.0, Math.min(1.0, vol));
  }

  enableAudio() {
    if (this.audioEnabled) return;

    const AudioContext = window.AudioContext || window.webkitAudioContext;
    this.audioCtx = new AudioContext({ sampleRate: 44100 });

    if (this.audioCtx.state === 'suspended') {
      this.audioCtx.resume();
    }

    const self = this;
    const bufferSize = 2048;
    this.audioNode = this.audioCtx.createScriptProcessor(bufferSize, 1, 2);

    this.audioNode.onaudioprocess = function(e) {
      const outputL = e.outputBuffer.getChannelData(0);
      const outputR = e.outputBuffer.getChannelData(1);

      const bufPtr = self._getAudioBuffer ? self._getAudioBuffer() : 0;
      if (bufPtr && self.status === 'running') {
        const heap16 = self.module.HEAP16;
        const startIdx = bufPtr >> 1;
        const vol = self.volume;

        for (let i = 0; i < bufferSize; i++) {
          outputL[i] = ((heap16[startIdx + i * 2] || 0) / 32768.0) * vol;
          outputR[i] = ((heap16[startIdx + i * 2 + 1] || 0) / 32768.0) * vol;
        }
      } else {
        outputL.fill(0);
        outputR.fill(0);
      }
    };

    this.audioNode.connect(this.audioCtx.destination);
    this.audioEnabled = true;
  }

  getStatus() {
    return this.status;
  }

  _startRenderingLoop() {
    if (!this.canvas) return;

    const ctx = this.canvas.getContext('2d');
    const self = this;

    function renderFrame() {
      if (self.status !== 'running') return;

      const width = (self._getWidth ? self._getWidth() : 0) || 240;
      const height = (self._getHeight ? self._getHeight() : 0) || 320;

      if (self.canvas.width !== width || self.canvas.height !== height) {
        self.canvas.width = width;
        self.canvas.height = height;
        self.imageData = null;
        self.pixelUint32Buffer = null;
      }

      if (!self.imageData) {
        self.imageData = ctx.createImageData(width, height);
        self.pixelUint32Buffer = new Uint32Array(self.imageData.data.buffer);
      }

      const bufPtr = self._getBuffer ? self._getBuffer() : 0;
      if (bufPtr) {
        const numPixels = width * height;
        const heap16 = self.module.HEAPU16;
        const startIdx = bufPtr >> 1;
        const data32 = self.pixelUint32Buffer;

        // Zero-allocation loop with fast bitwise shifts
        for (let i = 0; i < numPixels; i++) {
          const rgb565 = heap16[startIdx + i];
          const r = ((rgb565 >> 11) & 0x1F) * 8;
          const g = ((rgb565 >> 5) & 0x3F) * 4;
          const b = (rgb565 & 0x1F) * 8;
          data32[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }

        ctx.putImageData(self.imageData, 0, 0);
      }

      self.renderAnimFrameId = requestAnimationFrame(renderFrame);
    }

    this.renderAnimFrameId = requestAnimationFrame(renderFrame);
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = MREmu;
}
