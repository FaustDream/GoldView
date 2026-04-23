(function () {
    "use strict";

    const MAX_HISTORY = 20;
    const HISTORY_KEY = "goldview.average.history.v1";

    const form = document.getElementById("calculatorForm");
    const oldPriceInput = document.getElementById("oldPrice");
    const oldWeightInput = document.getElementById("oldWeight");
    const newPriceInput = document.getElementById("newPrice");
    const newWeightInput = document.getElementById("newWeight");
    const resultPrice = document.getElementById("resultPrice");
    const resultWeight = document.getElementById("resultWeight");
    const errorText = document.getElementById("errorText");
    const historyList = document.getElementById("historyList");
    const emptyHistory = document.getElementById("emptyHistory");
    const calculateBtn = document.getElementById("calculateBtn");
    const clearHistoryBtn = document.getElementById("clearHistoryBtn");

    let history = loadHistory();

    function normalizeNumericText(value) {
        return String(value || "").trim().replace(/[，,。]/g, ".");
    }

    function parseNumber(rawValue) {
        const normalized = normalizeNumericText(rawValue);
        if (!normalized) {
            return Number.NaN;
        }
        return Number(normalized);
    }

    function formatNumber(value) {
        return Number(value).toFixed(2);
    }

    function showError(message) {
        if (!message) {
            errorText.hidden = true;
            errorText.textContent = "";
            return;
        }
        errorText.hidden = false;
        errorText.textContent = message;
    }

    function renderHistory() {
        historyList.innerHTML = "";
        for (const item of history) {
            const row = document.createElement("li");
            row.textContent = item;
            historyList.appendChild(row);
        }
        emptyHistory.hidden = history.length > 0;
    }

    function saveHistory() {
        localStorage.setItem(HISTORY_KEY, JSON.stringify(history));
    }

    function loadHistory() {
        try {
            const raw = localStorage.getItem(HISTORY_KEY);
            if (!raw) {
                return [];
            }
            const parsed = JSON.parse(raw);
            if (!Array.isArray(parsed)) {
                return [];
            }
            return parsed.filter(item => typeof item === "string").slice(0, MAX_HISTORY);
        } catch (_) {
            return [];
        }
    }

    function handleCalculate() {
        const oldPrice = parseNumber(oldPriceInput.value);
        const oldWeight = parseNumber(oldWeightInput.value);
        const newPrice = parseNumber(newPriceInput.value);
        const newWeight = parseNumber(newWeightInput.value);

        if (![oldPrice, oldWeight, newPrice, newWeight].every(Number.isFinite)) {
            showError("请输入有效数字。");
            return;
        }

        const totalWeight = oldWeight + newWeight;
        if (totalWeight <= 0) {
            showError("总克重必须大于 0。");
            return;
        }

        const totalCost = oldPrice * oldWeight + newPrice * newWeight;
        const averagePrice = totalCost / totalWeight;

        resultPrice.textContent = formatNumber(averagePrice);
        resultWeight.textContent = formatNumber(totalWeight);
        showError("");

        const record = `旧 ${formatNumber(oldPrice)} / ${formatNumber(oldWeight)}g + 新 ${formatNumber(newPrice)} / ${formatNumber(newWeight)}g -> 均 ${formatNumber(averagePrice)}`;
        history.unshift(record);
        if (history.length > MAX_HISTORY) {
            history = history.slice(0, MAX_HISTORY);
        }
        saveHistory();
        renderHistory();
    }

    function clearHistory() {
        history = [];
        localStorage.removeItem(HISTORY_KEY);
        renderHistory();
        showError("");
    }

    calculateBtn.addEventListener("click", handleCalculate);
    clearHistoryBtn.addEventListener("click", clearHistory);
    form.addEventListener("submit", function (event) {
        event.preventDefault();
        handleCalculate();
    });

    [oldPriceInput, oldWeightInput, newPriceInput, newWeightInput].forEach(input => {
        input.addEventListener("keydown", function (event) {
            if (event.key === "Enter") {
                event.preventDefault();
                handleCalculate();
            }
        });
    });

    renderHistory();
})();
