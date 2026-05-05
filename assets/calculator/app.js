(function () {
    "use strict";

    const MAX_HISTORY = 20;
    const HISTORY_KEY = "goldview.average.history.v1";

    const form = document.getElementById("calculatorForm");
    const oldPriceInput = document.getElementById("oldPrice");
    const oldWeightInput = document.getElementById("oldWeight");
    const newPriceInput = document.getElementById("newPrice");
    const newWeightInput = document.getElementById("newWeight");
    const newAmountInput = document.getElementById("newAmount");
    const amountPriceInput = document.getElementById("amountPrice");
    const derivedWeight = document.getElementById("derivedWeight");
    const weightModeBtn = document.getElementById("weightModeBtn");
    const amountModeBtn = document.getElementById("amountModeBtn");
    const weightModeFields = document.getElementById("weightModeFields");
    const amountModeFields = document.getElementById("amountModeFields");
    const resultPrice = document.getElementById("resultPrice");
    const resultWeight = document.getElementById("resultWeight");
    const errorText = document.getElementById("errorText");
    const historyList = document.getElementById("historyList");
    const emptyHistory = document.getElementById("emptyHistory");
    const clearHistoryBtn = document.getElementById("clearHistoryBtn");

    let history = loadHistory();
    let buyMode = "weight";
    let currentCalculation = null;
    let currentRecordSignature = "";
    let lastSavedRecordSignature = "";

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

    function formatWeight(value) {
        return `${formatNumber(value)}g`;
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

    function setBuyMode(nextMode) {
        buyMode = nextMode;
        const isAmountMode = buyMode === "amount";

        amountModeFields.hidden = !isAmountMode;
        weightModeFields.hidden = isAmountMode;
        amountModeBtn.classList.toggle("active", isAmountMode);
        weightModeBtn.classList.toggle("active", !isAmountMode);
        amountModeBtn.setAttribute("aria-pressed", String(isAmountMode));
        weightModeBtn.setAttribute("aria-pressed", String(!isAmountMode));

        if (isAmountMode) {
            newPriceInput.value = "";
            newWeightInput.value = "";
        } else {
            newAmountInput.value = "";
            amountPriceInput.value = "";
            derivedWeight.textContent = "--";
        }

        resultPrice.textContent = "--";
        resultWeight.textContent = "--";
        showError("");
        updateCalculation();
    }

    function readBaseInputs() {
        return {
            oldPrice: parseNumber(oldPriceInput.value),
            oldWeight: parseNumber(oldWeightInput.value),
        };
    }

    function calculateByActiveMode() {
        const base = readBaseInputs();

        if (buyMode === "amount") {
            const amount = parseNumber(newAmountInput.value);
            const pricePerGram = parseNumber(amountPriceInput.value);
            if (![base.oldPrice, base.oldWeight, amount, pricePerGram].every(Number.isFinite)) {
                throw new Error("请输入有效数字。");
            }
            return {
                mode: "amount",
                amount,
                pricePerGram,
                result: GoldViewCalculator.calculateAverageByAmount({
                    ...base,
                    amount,
                    pricePerGram,
                }),
            };
        }

        const newPrice = parseNumber(newPriceInput.value);
        const newWeight = parseNumber(newWeightInput.value);
        if (![base.oldPrice, base.oldWeight, newPrice, newWeight].every(Number.isFinite)) {
            throw new Error("请输入有效数字。");
        }
        return {
            mode: "weight",
            result: GoldViewCalculator.calculateAverageByWeight({
                ...base,
                newPrice,
                newWeight,
            }),
        };
    }

    function resetResults() {
        resultPrice.textContent = "--";
        resultWeight.textContent = "--";
        if (buyMode === "amount") {
            derivedWeight.textContent = "--";
        }
        currentCalculation = null;
        currentRecordSignature = "";
    }

    function buildRecord(calculation) {
        const oldPrice = parseNumber(oldPriceInput.value);
        const oldWeight = parseNumber(oldWeightInput.value);
        const result = calculation.result;

        if (calculation.mode === "amount") {
            return `旧 ${formatNumber(oldPrice)} / ${formatWeight(oldWeight)} + 新 ${formatNumber(calculation.pricePerGram)} / ${formatNumber(calculation.amount)}元 = ${formatWeight(result.newWeight)} -> 均 ${formatNumber(result.averagePrice)}`;
        }

        return `旧 ${formatNumber(oldPrice)} / ${formatWeight(oldWeight)} + 新 ${formatNumber(result.newPrice)} / ${formatWeight(result.newWeight)} -> 均 ${formatNumber(result.averagePrice)}`;
    }

    function buildRecordSignature(calculation) {
        const oldPrice = normalizeNumericText(oldPriceInput.value);
        const oldWeight = normalizeNumericText(oldWeightInput.value);
        if (calculation.mode === "amount") {
            return [calculation.mode, oldPrice, oldWeight, normalizeNumericText(newAmountInput.value), normalizeNumericText(amountPriceInput.value)].join("|");
        }
        return [calculation.mode, oldPrice, oldWeight, normalizeNumericText(newPriceInput.value), normalizeNumericText(newWeightInput.value)].join("|");
    }

    function saveCurrentCalculationToHistory() {
        if (!currentCalculation || !currentRecordSignature || currentRecordSignature === lastSavedRecordSignature) {
            return;
        }

        const record = buildRecord(currentCalculation);
        history.unshift(record);
        if (history.length > MAX_HISTORY) {
            history = history.slice(0, MAX_HISTORY);
        }
        lastSavedRecordSignature = currentRecordSignature;
        saveHistory();
        renderHistory();
    }

    function updateCalculation() {
        let calculation;
        try {
            calculation = calculateByActiveMode();
        } catch (error) {
            resetResults();
            showError("");
            return;
        }

        const result = calculation.result;
        resultPrice.textContent = formatNumber(result.averagePrice);
        resultWeight.textContent = formatNumber(result.totalWeight);
        if (calculation.mode === "amount") {
            derivedWeight.textContent = formatWeight(result.newWeight);
        }
        currentCalculation = calculation;
        currentRecordSignature = buildRecordSignature(calculation);
        showError("");
    }

    function clearHistory() {
        history = [];
        localStorage.removeItem(HISTORY_KEY);
        renderHistory();
        showError("");
    }

    weightModeBtn.addEventListener("click", function () {
        setBuyMode("weight");
    });
    amountModeBtn.addEventListener("click", function () {
        setBuyMode("amount");
    });
    clearHistoryBtn.addEventListener("click", clearHistory);
    form.addEventListener("submit", function (event) {
        event.preventDefault();
    });

    [oldPriceInput, oldWeightInput, newPriceInput, newWeightInput, newAmountInput, amountPriceInput].forEach(input => {
        input.addEventListener("input", updateCalculation);
        input.addEventListener("blur", saveCurrentCalculationToHistory);
        input.addEventListener("keydown", function (event) {
            if (event.key === "Enter") {
                event.preventDefault();
                saveCurrentCalculationToHistory();
            }
        });
    });

    renderHistory();
})();
