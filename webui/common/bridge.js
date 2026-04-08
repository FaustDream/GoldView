(() => {
  const listeners = new Set();

  function safeParseMessage(data) {
    if (typeof data !== 'string') {
      return data;
    }
    try {
      return JSON.parse(data);
    } catch {
      return { type: 'raw', data };
    }
  }

  function send(action, payload = {}) {
    const params = new URLSearchParams();
    params.set('action', action);
    Object.entries(payload).forEach(([key, value]) => {
      if (value === undefined || value === null) {
        return;
      }
      params.set(key, String(value));
    });
    window.chrome?.webview?.postMessage(params.toString());
  }

  window.goldviewBridge = {
    send,
    requestInit() {
      send('ready');
    },
    closeWindow() {
      send('closeWindow');
    },
    onMessage(handler) {
      listeners.add(handler);
      return () => listeners.delete(handler);
    }
  };

  window.chrome?.webview?.addEventListener('message', (event) => {
    const message = safeParseMessage(event.data);
    listeners.forEach((listener) => listener(message));
  });
})();
