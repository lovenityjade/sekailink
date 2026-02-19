const CACHE_VERSION = new URL(self.location.href).searchParams.get("v") || "v1";
const STATIC_CACHE = `skl-static-${CACHE_VERSION}`;
const CORE_ASSETS = [
  "/static/styles/globalStyles.css",
  "/static/styles/appShell.css",
  "/static/styles/termsModal.css",
  "/static/styles/tooltip.css",
  "/static/styles/roomList.css",
  "/static/styles/boot.css",
  "/static/assets/sfx.js",
  "/static/assets/baseHeader.js",
  "/static/assets/lobbiesLanding.js",
  "/static/assets/termsModal.js",
  "/static/assets/cookieconsent-config.js"
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(STATIC_CACHE).then((cache) => cache.addAll(CORE_ASSETS))
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(
        keys
          .filter((key) => key.startsWith("skl-static-") && key !== STATIC_CACHE)
          .map((key) => caches.delete(key))
      )
    )
  );
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  if (event.request.method !== "GET") return;
  const url = new URL(event.request.url);
  const isSameOrigin = url.origin === self.location.origin;

  if (event.request.mode === "navigate") {
    event.respondWith(
      fetch(event.request)
        .then((response) => {
          return response;
        })
        .catch(() => caches.match(event.request))
    );
    return;
  }

  if (isSameOrigin && url.pathname.startsWith("/static/")) {
    event.respondWith(
      caches.match(event.request).then((cached) => {
        if (cached) return cached;
        return fetch(event.request).then((response) => {
          if (response && response.status === 200) {
            const clone = response.clone();
            caches.open(STATIC_CACHE).then((cache) => cache.put(event.request, clone));
          }
          return response;
        });
      })
    );
  }
});
