const elements = {
  oldPrice: document.getElementById('old-price'),
  oldWeight: document.getElementById('old-weight'),
  newPrice: document.getElementById('new-price'),
  newWeight: document.getElementById('new-weight'),
  resultPrice: document.getElementById('result-price'),
  resultTotal: document.getElementById('result-total'),
  notice: document.getElementById('calculator-notice'),
  historyList: document.getElementById('history-list'),
  clearButton: document.getElementById('clear-button'),
  calculateButton: document.getElementById('calculate-button')
};

function renderHistory(history) {
  if (!history.length) {
    elements.historyList.innerHTML = '<div class="history-item">暂无历史记录</div>';
    return;
  }
  elements.historyList.innerHTML = history
    .map((item) => `<div class="history-item">${item}</div>`)
    .join('');
}

function renderState(data) {
  elements.resultPrice.textContent = data.resultPrice || '--';
  elements.resultTotal.textContent = data.resultTotal || '--';
  elements.notice.textContent = data.error || '';
  elements.notice.className = 'notice' + (data.error ? ' error' : '');
  renderHistory(data.history || []);
}

function calculate() {
  window.goldviewBridge.send('calculateAverage', {
    oldPrice: elements.oldPrice.value,
    oldWeight: elements.oldWeight.value,
    newPrice: elements.newPrice.value,
    newWeight: elements.newWeight.value
  });
}

function clearInputs() {
  elements.oldPrice.value = '';
  elements.oldWeight.value = '';
  elements.newPrice.value = '';
  elements.newWeight.value = '';
  elements.resultPrice.textContent = '--';
  elements.resultTotal.textContent = '--';
  elements.notice.textContent = '';
  elements.notice.className = 'notice';
}

window.goldviewBridge.onMessage((message) => {
  if (message.type === 'calculatorState') {
    renderState(message.data);
  }
});

elements.calculateButton.addEventListener('click', calculate);
elements.clearButton.addEventListener('click', clearInputs);
window.goldviewBridge.requestInit();
