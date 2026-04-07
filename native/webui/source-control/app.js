const state = {
  server: null,
  draft: null,
  dirty: false
};

const elements = {
  lastSuccess: document.getElementById('last-success'),
  autoRefreshEnabled: document.getElementById('auto-refresh-enabled'),
  autoSwitchSource: document.getElementById('auto-switch-source'),
  preferredSource: document.getElementById('preferred-source'),
  successThreshold: document.getElementById('success-threshold'),
  latencyThreshold: document.getElementById('latency-threshold'),
  sourceRows: document.getElementById('source-rows'),
  saveNotice: document.getElementById('save-notice'),
  saveButton: document.getElementById('save-button'),
  cancelButton: document.getElementById('cancel-button')
};

function cloneState(value) {
  return JSON.parse(JSON.stringify(value));
}

function setDirty(dirty, notice = '') {
  state.dirty = dirty;
  elements.saveNotice.textContent = notice;
  elements.saveNotice.className = 'notice';
}

function renderPreferredSourceOptions(sources) {
  if (elements.preferredSource.options.length === sources.length) {
    return;
  }
  elements.preferredSource.innerHTML = sources
    .map((source) => `<option value="${source.key}">${source.label}</option>`)
    .join('');
}

function renderSourceRows(sources) {
  elements.sourceRows.innerHTML = sources.map((source) => `
    <div class="table-row">
      <div class="source-name">
        <strong>${source.label}</strong>
        <small>${source.transport}</small>
      </div>
      <label class="checkbox"><input data-role="enabled" data-key="${source.key}" type="checkbox" ${source.enabled ? 'checked' : ''} /><span>启用</span></label>
      <input data-role="priority" data-key="${source.key}" type="number" min="1" max="99" step="1" value="${source.priority}" />
      <input data-role="weight" data-key="${source.key}" type="number" min="1" max="100" step="1" value="${source.weight}" />
    </div>
  `).join('');
}

function render(data, force = false) {
  state.server = cloneState(data);
  if (!state.dirty || force || !state.draft) {
    state.draft = cloneState(data);
    renderPreferredSourceOptions(data.sources);
    elements.autoRefreshEnabled.checked = data.autoRefreshEnabled;
    elements.autoSwitchSource.checked = data.autoSwitchSource;
    elements.preferredSource.value = data.preferredSource;
    elements.successThreshold.value = data.successRateThreshold;
    elements.latencyThreshold.value = data.latencyThresholdMs;
    renderSourceRows(data.sources);
    setDirty(false, data.notice || '');
  }
  elements.lastSuccess.textContent = data.lastSuccessfulAt || '--';
}

function collectDraft() {
  const sources = state.server.sources.map((source) => ({
    ...source,
    enabled: elements.sourceRows.querySelector(`[data-role="enabled"][data-key="${source.key}"]`).checked,
    priority: Number(elements.sourceRows.querySelector(`[data-role="priority"][data-key="${source.key}"]`).value),
    weight: Number(elements.sourceRows.querySelector(`[data-role="weight"][data-key="${source.key}"]`).value)
  }));

  return {
    ...state.server,
    autoRefreshEnabled: elements.autoRefreshEnabled.checked,
    autoSwitchSource: elements.autoSwitchSource.checked,
    preferredSource: elements.preferredSource.value,
    successRateThreshold: Number(elements.successThreshold.value),
    latencyThresholdMs: Number(elements.latencyThreshold.value),
    sources
  };
}

function sendSave() {
  const draft = collectDraft();
  const payload = {
    autoRefreshEnabled: draft.autoRefreshEnabled ? 1 : 0,
    autoSwitchSource: draft.autoSwitchSource ? 1 : 0,
    preferredSource: draft.preferredSource,
    successRateThreshold: draft.successRateThreshold,
    latencyThresholdMs: draft.latencyThresholdMs
  };

  draft.sources.forEach((source) => {
    payload[`enabled_${source.key}`] = source.enabled ? 1 : 0;
    payload[`priority_${source.key}`] = source.priority;
    payload[`weight_${source.key}`] = source.weight;
  });

  window.goldviewBridge.send('saveSourceSettings', payload);
}

function bindEvents() {
  document.body.addEventListener('input', () => {
    if (state.server) {
      state.draft = collectDraft();
      setDirty(true, '有未保存的改动');
    }
  });

  elements.saveButton.addEventListener('click', sendSave);
  elements.cancelButton.addEventListener('click', () => {
    if (state.server) {
      render(state.server, true);
      setDirty(false, '已还原到当前生效配置');
    }
  });
}

window.goldviewBridge.onMessage((message) => {
  if (message.type === 'sourceControlState') {
    render(message.data, !!message.data.force);
  }
});

bindEvents();
window.goldviewBridge.requestInit();
