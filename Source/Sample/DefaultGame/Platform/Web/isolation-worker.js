
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

self.addEventListener('fetch', (event) => {
    const request = event.request;

    if (request.cache === 'only-if-cached' && request.mode !== 'same-origin')
    {
        return;
    }

    event.respondWith(fetch(request).then((response) => {
        // an opaque response can't be read, so it can't be given new headers either
        if (response.status === 0)
        {
            return response;
        }

        const headers = new Headers(response.headers);
        headers.set('Cross-Origin-Opener-Policy', 'same-origin');
        headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
        headers.set('Cross-Origin-Resource-Policy', 'same-origin');

        return new Response(response.body, { status: response.status, statusText: response.statusText, headers });
    }));
});
