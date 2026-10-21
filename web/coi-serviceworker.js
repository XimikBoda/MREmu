/*
 * Cross-origin isolation on a static host.
 *
 * The emulator is compiled with -pthread, so it needs SharedArrayBuffer, and
 * browsers only expose SharedArrayBuffer to cross-origin isolated documents.
 * Isolation normally comes from two response headers:
 *
 *   Cross-Origin-Opener-Policy: same-origin
 *   Cross-Origin-Embedder-Policy: require-corp
 *
 * GitHub Pages (and any plain static host) cannot set response headers, so this
 * service worker re-serves every response with them added. The page reloads
 * itself once after the worker takes control, because a fetch handler only
 * affects requests made after registration.
 *
 * Same-origin only, nothing is cached, no request is modified: the worker only
 * decorates the response headers, so a stale worker can never serve stale files.
 *
 * This file runs in two roles: as a worker (no `window`) and as a normal page
 * script (registers the worker and handles the one-time reload).
 */

if (typeof window === 'undefined') {
  /* ---------------------------------------------------------------- worker */

  self.addEventListener('install', function () {
    // Take over immediately; the page decides when to reload.
    self.skipWaiting();
  });

  self.addEventListener('activate', function (event) {
    event.waitUntil(self.clients.claim());
  });

  self.addEventListener('fetch', function (event) {
    var request = event.request;
    if (request.method !== 'GET') return;

    event.respondWith(
      fetch(request)
        .then(function (response) {
          // Opaque and error responses cannot be rewritten; passing them
          // through keeps the failure visible instead of masking it.
          if (response.status === 0 || !response.ok) return response;

          var headers = new Headers(response.headers);
          headers.set('Cross-Origin-Opener-Policy', 'same-origin');
          headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
          headers.set('Cross-Origin-Resource-Policy', 'same-origin');

          return new Response(response.body, {
            status: response.status,
            statusText: response.statusText,
            headers: headers,
          });
        })
        .catch(function () {
          // Offline or aborted: let the browser produce its own error page.
          return Response.error();
        })
    );
  });
} else {
  /* ------------------------------------------------------------------ page */

  (function () {
    var RELOAD_FLAG = 'mremu.coiReloaded';

    // Already isolated: either the worker is in control or the host sent the
    // headers itself (tools/web_serve.py does). Nothing to do.
    if (window.crossOriginIsolated) {
      try {
        sessionStorage.removeItem(RELOAD_FLAG);
      } catch (e) {}
      return;
    }

    if (!window.isSecureContext || !('serviceWorker' in navigator)) {
      window.MREmuCoi = { supported: false };
      return;
    }

    window.MREmuCoi = { supported: true, ready: false };

    navigator.serviceWorker
      .register(new URL('coi-serviceworker.js', window.location.href).href, {
        scope: './',
        updateViaCache: 'none',
      })
      .then(function (registration) {
        return navigator.serviceWorker.ready.then(function () {
          return registration;
        });
      })
      .then(function (registration) {
        // Always reload once the worker is active when the page is still not
        // isolated. `clients.claim()` can make navigator.serviceWorker.controller
        // non-null for a document whose navigation response was already sent
        // without COOP/COEP, so being controlled does not mean this document
        // has the headers. The sessionStorage flag below still prevents loops.
        var worker =
          registration.installing || registration.waiting || registration.active;
        if (!worker) {
          window.MREmuCoi.failed = true;
          return;
        }

        function reloadOnce() {
          // The first load is never controlled by the worker it just
          // installed, so the headers only arrive on the next navigation.
          // Reload once, and never in a loop: if isolation is still missing
          // afterwards the page reports it instead of reloading again.
          var alreadyReloaded = false;
          try {
            alreadyReloaded = sessionStorage.getItem(RELOAD_FLAG) === '1';
          } catch (e) {}

          if (alreadyReloaded) {
            window.MREmuCoi.failed = true;
            return;
          }

          try {
            sessionStorage.setItem(RELOAD_FLAG, '1');
          } catch (e) {}
          window.location.reload();
        }

        if (worker.state === 'activated') {
          reloadOnce();
          return;
        }

        worker.addEventListener('statechange', function () {
          if (worker.state === 'activated') reloadOnce();
        });
      })
      .catch(function (error) {
        console.error('[coi] service worker registration failed', error);
        window.MREmuCoi.failed = true;
      });
  })();
}
