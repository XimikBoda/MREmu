/*
 * Browser host for the Emscripten build.
 *
 * Responsibilities, in the order they matter:
 *
 *  1. Make sure the page is cross-origin isolated. The emulator is a pthread
 *     build, so without SharedArrayBuffer it cannot start; on a static host
 *     (GitHub Pages) that comes from coi-serviceworker.js, never from headers.
 *  2. Load the .vxp files the user uploaded earlier and write them into the
 *     emulator's filesystem before main() runs, so the app list shows them.
 *  3. Keep that storage in sync when files are added or deleted from the
 *     options panel, and tell the running menu to rescan.
 *
 * Uploads live in IndexedDB, not localStorage: a single VXP is a few hundred
 * kilobytes and a catalog runs to tens of megabytes, well past localStorage's
 * ~5 MB ceiling.
 */
(function () {
  'use strict';

  var APP_DIR = '/fs/e/mre';
  var DB_NAME = 'mremu-web';
  var DB_VERSION = 1;
  var STORE = 'vxps';
  var PREF_SCALE = 'mremu.scale';
  var PREF_MUTED = 'mremu.muted';

  var els = {};
  var state = {
    module: null, // the moduleArg object, which Emscripten turns into Module
    uploads: [],
    scale: 'fit',
    started: false,
  };

  /* ------------------------------------------------------------------ util */

  function $(id) {
    return document.getElementById(id);
  }

  function formatBytes(n) {
    if (n < 1024) return n + ' B';
    if (n < 1024 * 1024) return (n / 1024).toFixed(0) + ' KB';
    return (n / (1024 * 1024)).toFixed(1) + ' MB';
  }

  /** Strip any path the browser may have included and characters that would
   *  confuse the guest file APIs; the emulator keys apps by file name. */
  function sanitizeName(name) {
    var base = String(name || '').split(/[\\/]/).pop() || 'app.vxp';
    base = base.replace(/[^\w.\-() ]+/g, '_').replace(/^\.+/, '');
    if (!/\.vxp$/i.test(base)) base += '.vxp';
    return base.slice(0, 120);
  }

  var toastTimer = null;
  function toast(message) {
    els.toast.textContent = message;
    els.toast.classList.add('visible');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () {
      els.toast.classList.remove('visible');
    }, 4000);
  }

  function fatal(message) {
    els.overlay.classList.remove('hidden');
    els.bar.style.width = '0%';
    els.status.textContent = message;
  }

  /* --------------------------------------------------------------- storage */

  var dbPromise = null;

  function openDb() {
    if (dbPromise) return dbPromise;
    dbPromise = new Promise(function (resolve, reject) {
      var request = indexedDB.open(DB_NAME, DB_VERSION);
      request.onupgradeneeded = function () {
        var db = request.result;
        if (!db.objectStoreNames.contains(STORE)) {
          db.createObjectStore(STORE, { keyPath: 'name' });
        }
      };
      request.onsuccess = function () {
        resolve(request.result);
      };
      request.onerror = function () {
        reject(request.error);
      };
    });
    return dbPromise;
  }

  function withStore(mode, fn) {
    return openDb().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(STORE, mode);
        var store = tx.objectStore(STORE);
        var out = fn(store);
        tx.oncomplete = function () {
          resolve(out && out.result !== undefined ? out.result : out);
        };
        tx.onerror = function () {
          reject(tx.error);
        };
        tx.onabort = function () {
          reject(tx.error);
        };
      });
    });
  }

  function loadUploads() {
    return withStore('readonly', function (store) {
      return store.getAll();
    }).then(function (rows) {
      rows.sort(function (a, b) {
        return (a.addedAt || 0) - (b.addedAt || 0);
      });
      return rows;
    });
  }

  function saveUpload(record) {
    return withStore('readwrite', function (store) {
      store.put(record);
    });
  }

  function removeUpload(name) {
    return withStore('readwrite', function (store) {
      store.delete(name);
    });
  }

  function removeAllUploads() {
    return withStore('readwrite', function (store) {
      store.clear();
    });
  }

  /* ---------------------------------------------------------- emulator fs */

  function fs() {
    return state.module && state.module.FS;
  }

  function writeFileToEmulator(name, bytes) {
    var FS = fs();
    if (!FS) return false;
    try {
      FS.mkdirTree(APP_DIR);
    } catch (e) {
      /* already there */
    }
    try {
      FS.writeFile(APP_DIR + '/' + name, new Uint8Array(bytes));
      return true;
    } catch (e) {
      console.error('[mremu] could not write ' + name, e);
      return false;
    }
  }

  function deleteFileFromEmulator(name) {
    var FS = fs();
    if (!FS) return;
    try {
      FS.unlink(APP_DIR + '/' + name);
    } catch (e) {
      /* already gone */
    }
  }

  /** Ask the app list to rescan. The C++ side only sets a flag; the rescan
   *  itself happens on the menu's own timer, between frames. */
  function notifyAppsChanged() {
    var mod = state.module;
    if (mod && typeof mod._mremu_web_apps_changed === 'function') {
      mod._mremu_web_apps_changed();
    }
  }

  /* --------------------------------------------------------------- uploads */

  function renderUploads() {
    els.uploads.textContent = '';

    if (!state.uploads.length) {
      els.uploadsTable.hidden = true;
      els.uploadsSummary.textContent = 'No apps uploaded yet.';
      els.clearAll.hidden = true;
      return;
    }

    var total = 0;
    state.uploads.forEach(function (row) {
      total += row.size || 0;

      var tr = document.createElement('tr');

      var name = document.createElement('td');
      name.className = 'upload-name';
      name.textContent = row.name;
      name.title = row.name;

      var size = document.createElement('td');
      size.className = 'size';
      size.textContent = formatBytes(row.size || 0);

      var action = document.createElement('td');
      action.className = 'action';
      var remove = document.createElement('button');
      remove.className = 'upload-remove';
      remove.type = 'button';
      remove.textContent = 'Remove';
      remove.setAttribute('aria-label', 'Remove ' + row.name);
      remove.addEventListener('click', function () {
        deleteUpload(row.name);
      });
      action.appendChild(remove);

      tr.appendChild(name);
      tr.appendChild(size);
      tr.appendChild(action);
      els.uploads.appendChild(tr);
    });

    els.uploadsTable.hidden = false;
    els.uploadsSummary.textContent =
      state.uploads.length +
      (state.uploads.length === 1 ? ' app, ' : ' apps, ') +
      formatBytes(total) +
      ' stored in this browser.';
    els.clearAll.hidden = false;
  }

  function acceptFile(file) {
    if (!file) return false;
    if (!/\.vxp$/i.test(file.name)) {
      toast('Skipped "' + file.name + '": not a .vxp file.');
      return false;
    }
    if (!file.size) {
      toast('Skipped "' + file.name + '": the file is empty.');
      return false;
    }
    return true;
  }

  function addFiles(fileList) {
    var files = Array.prototype.slice.call(fileList || []);
    if (!files.length) return Promise.resolve();

    // Ask for durable storage on the first upload: without it the browser may
    // evict the whole origin under storage pressure.
    if (navigator.storage && navigator.storage.persist) {
      navigator.storage.persist().catch(function () {});
    }

    var added = 0;
    var chain = Promise.resolve();

    files.forEach(function (file) {
      chain = chain
        .then(function () {
          if (!acceptFile(file)) return null;
          return file.arrayBuffer().then(function (bytes) {
            var record = {
              name: sanitizeName(file.name),
              bytes: bytes,
              size: bytes.byteLength,
              addedAt: Date.now(),
            };
            return saveUpload(record).then(function () {
              writeFileToEmulator(record.name, record.bytes);
              added++;
            });
          });
        })
        .catch(function (error) {
          console.error('[mremu] upload failed', file.name, error);
          toast('Could not store "' + file.name + '": ' + (error && error.message ? error.message : 'storage error'));
        });
    });

    return chain
      .then(loadUploads)
      .then(function (rows) {
        state.uploads = rows;
        renderUploads();
        if (added) {
          notifyAppsChanged();
          toast(
            added === 1 ? 'Added 1 app.' : 'Added ' + added + ' apps.'
          );
        }
      });
  }

  function deleteUpload(name) {
    return removeUpload(name)
      .then(function () {
        deleteFileFromEmulator(name);
        return loadUploads();
      })
      .then(function (rows) {
        state.uploads = rows;
        renderUploads();
        notifyAppsChanged();
        toast('Removed "' + name + '".');
      })
      .catch(function (error) {
        console.error('[mremu] delete failed', error);
        toast('Could not remove "' + name + '".');
      });
  }

  function deleteEverything() {
    if (!state.uploads.length) return;
    var names = state.uploads.map(function (row) {
      return row.name;
    });
    removeAllUploads()
      .then(function () {
        names.forEach(deleteFileFromEmulator);
        return loadUploads();
      })
      .then(function (rows) {
        state.uploads = rows;
        renderUploads();
        notifyAppsChanged();
        toast('Removed all uploaded apps.');
      });
  }

  /* --------------------------------------------------------------- options */

  function applyScale() {
    var canvas = els.canvas;
    if (!canvas || !canvas.width) return;

    var scale = state.scale;
    var factor;
    if (scale === 'fit') {
      var availableW = window.innerWidth - 32;
      var availableH = window.innerHeight - 32;
      factor = Math.min(availableW / canvas.width, availableH / canvas.height);
      factor = Math.max(1, Math.min(factor, 4));
      // Fit to whole pixels where possible so the 240x320 output stays crisp.
      factor = Math.floor(factor * 2) / 2;
    } else {
      factor = parseFloat(scale) || 1;
    }

    canvas.style.width = Math.round(canvas.width * factor) + 'px';
    canvas.style.height = Math.round(canvas.height * factor) + 'px';
  }

  function setScale(value) {
    state.scale = value;
    try {
      localStorage.setItem(PREF_SCALE, value);
    } catch (e) {}
    applyScale();
  }

  function toggleFullscreen() {
    if (document.fullscreenElement) {
      document.exitFullscreen().catch(function () {});
      return;
    }
    var target = document.documentElement;
    if (target.requestFullscreen) target.requestFullscreen().catch(function () {});
  }

  function applyMuted(muted) {
    els.muted.checked = muted;
    try {
      localStorage.setItem(PREF_MUTED, muted ? '1' : '0');
    } catch (e) {}
    var mod = state.module;
    if (mod && typeof mod._mremu_web_set_muted === 'function') {
      mod._mremu_web_set_muted(muted ? 1 : 0);
    }
  }

  /* ---------------------------------------------------------------- panel */

  function openPanel() {
    els.panel.classList.add('open');
    els.panel.removeAttribute('hidden');
    els.panelOpen.setAttribute('aria-expanded', 'true');
    renderUploads();
  }

  function closePanel() {
    els.panel.classList.remove('open');
    els.panel.setAttribute('hidden', '');
    els.panelOpen.setAttribute('aria-expanded', 'false');
    els.canvas.focus();
  }

  function togglePanel() {
    if (els.panel.classList.contains('open')) closePanel();
    else openPanel();
  }

  /**
   * Keep the emulated phone from seeing keys typed into the panel.
   *
   * SFML installs its key handlers on the window in the capture phase when the
   * window is created, i.e. after this file has run. Registering first and
   * calling stopImmediatePropagation() while the panel owns the keyboard keeps
   * a filename being typed out of the guest.
   */
  function trapKeys() {
    function block(event) {
      var target = event.target;
      var typing = target && /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName);
      if (!els.panel.classList.contains('open') && !typing) return;
      if (event.key === 'Escape' && els.panel.classList.contains('open')) {
        closePanel();
      }
      event.stopImmediatePropagation();
    }
    // Capture on window, before any listener SFML can add.
    window.addEventListener('keydown', block, true);
    window.addEventListener('keyup', block, true);
    window.addEventListener('keypress', block, true);
  }

  /* ------------------------------------------------------------------ boot */

  function waitFor(condition, timeoutMs, label) {
    return new Promise(function (resolve, reject) {
      var start = performance.now();
      (function poll() {
        if (condition()) return resolve();
        if (performance.now() - start > timeoutMs) {
          return reject(new Error('timed out waiting for ' + label));
        }
        setTimeout(poll, 50);
      })();
    });
  }

  async function waitForIsolation(timeoutMs) {
    if (window.crossOriginIsolated) return true;
    var deadline = performance.now() + timeoutMs;
    while (performance.now() < deadline) {
      if (window.crossOriginIsolated) return true;
      // coi-serviceworker.js either reloads the page or gives up; stop waiting
      // as soon as it reports failure.
      if (window.MREmuCoi && window.MREmuCoi.failed) return false;
      await new Promise(function (resolve) {
        setTimeout(resolve, 100);
      });
    }
    return window.crossOriginIsolated;
  }

  function writeAllUploads() {
    var written = 0;
    state.uploads.forEach(function (row) {
      if (writeFileToEmulator(row.name, row.bytes)) written++;
    });
    if (state.uploads.length) {
      console.info('[mremu] mounted ' + written + ' uploaded app(s)');
    }
  }

  async function start() {
    els.status.textContent = 'Preparing browser storage…';

    var isolated = await waitForIsolation(6000);
    if (!isolated) {
      fatal(
        'This page is not cross-origin isolated, so WebAssembly threads cannot start. ' +
          'Serve it from https:// (or localhost) with coi-serviceworker.js next to index.html, ' +
          'or use tools/web_serve.py locally.'
      );
      return;
    }

    try {
      state.uploads = await loadUploads();
    } catch (error) {
      console.error('[mremu] IndexedDB unavailable', error);
      state.uploads = [];
      toast('Browser storage is unavailable, uploads will not be kept.');
    }
    renderUploads();

    els.status.textContent = 'Loading emulator…';

    try {
      await waitFor(function () {
        return typeof window.MREmuModule === 'function';
      }, 30000, 'the emulator bundle');
    } catch (error) {
      fatal('index.js did not load. Check the browser console and that the bundle is complete.');
      return;
    }

    var moduleArg = {
      canvas: els.canvas,
      // Emscripten reports download/compile progress as "message (done/total)".
      setStatus: function (text) {
        var m = text && text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
        if (m) {
          els.status.textContent = m[1].trim();
          els.bar.style.width = (parseInt(m[2], 10) / parseInt(m[4], 10)) * 100 + '%';
        } else if (text) {
          els.status.textContent = text;
        }
      },
      onRuntimeInitialized: function () {
        // Frame 0 of main(): the preloaded catalog is in the filesystem and
        // the app list has not been built yet, so uploads appear in the very
        // first scan instead of needing a rescan.
        writeAllUploads();
        els.overlay.classList.add('hidden');
        els.canvas.focus();
        applyScale();
        applyMuted(els.muted.checked);
        state.started = true;
        els.panelOpen.hidden = false;
      },
      onAbort: function (what) {
        fatal('The emulator stopped: ' + what);
      },
    };

    state.module = moduleArg;
    window.MREmuModule(moduleArg);
  }

  /* ------------------------------------------------------------------ init */

  function init() {
    els.canvas = $('canvas');
    els.overlay = $('overlay');
    els.status = $('status');
    els.bar = document.querySelector('#bar > div');
    els.toast = $('toast');
    els.panel = $('panel');
    els.panelOpen = $('panel-open');
    els.uploads = $('uploads');
    els.uploadsTable = $('uploads-table');
    els.uploadsSummary = $('uploads-summary');
    els.fileInput = $('file-input');
    els.addFiles = $('add-files');
    els.clearAll = $('uploads-clear');
    els.scale = $('scale');
    els.muted = $('muted');

    try {
      state.scale = localStorage.getItem(PREF_SCALE) || 'fit';
      els.muted.checked = localStorage.getItem(PREF_MUTED) === '1';
    } catch (e) {}

    els.scale.value = state.scale;
    trapKeys();

    els.panelOpen.addEventListener('click', togglePanel);
    $('panel-close').addEventListener('click', closePanel);

    els.addFiles.addEventListener('click', function () {
      els.fileInput.click();
    });
    els.fileInput.addEventListener('change', function () {
      addFiles(els.fileInput.files).then(function () {
        els.fileInput.value = '';
      });
    });
    els.clearAll.addEventListener('click', deleteEverything);

    els.scale.addEventListener('change', function () {
      setScale(els.scale.value);
    });
    $('fullscreen').addEventListener('click', toggleFullscreen);
    els.muted.addEventListener('change', function () {
      applyMuted(els.muted.checked);
    });
    $('reload').addEventListener('click', function () {
      window.location.reload();
    });

    // Dropping a .vxp anywhere opens the panel and imports it, matching what
    // the emulator's own empty-list hint promises.
    ['dragenter', 'dragover'].forEach(function (type) {
      document.addEventListener(type, function (event) {
        event.preventDefault();
        els.stage.classList.add('dropping');
      });
    });
    ['dragleave', 'dragend'].forEach(function (type) {
      document.addEventListener(type, function () {
        els.stage.classList.remove('dropping');
      });
    });
    document.addEventListener('drop', function (event) {
      event.preventDefault();
      els.stage.classList.remove('dropping');
      if (!event.dataTransfer || !event.dataTransfer.files.length) return;
      if (!els.panel.classList.contains('open')) openPanel();
      addFiles(event.dataTransfer.files);
    });

    window.addEventListener('resize', applyScale);

    start();
  }

  els.stage = document.getElementById('stage');

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
