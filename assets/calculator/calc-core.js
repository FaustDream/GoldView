(function (root, factory) {
    if (typeof module === "object" && module.exports) {
        module.exports = factory();
    } else {
        root.GoldViewCalculator = factory();
    }
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
    "use strict";

    function assertPositive(value, message) {
        if (!Number.isFinite(value) || value <= 0) {
            throw new Error(message);
        }
    }

    function assertNonNegative(value, message) {
        if (!Number.isFinite(value) || value < 0) {
            throw new Error(message);
        }
    }

    function calculateAverageByWeight(input) {
        const oldPrice = input.oldPrice;
        const oldWeight = input.oldWeight;
        const newPrice = input.newPrice;
        const newWeight = input.newWeight;

        assertNonNegative(oldWeight, "原有克重不能小于 0。");
        assertPositive(newWeight, "买入克重必须大于 0。");
        if (oldWeight > 0) {
            assertPositive(oldPrice, "原有单价必须大于 0。");
        }
        assertPositive(newPrice, "买入单价必须大于 0。");

        const totalWeight = oldWeight + newWeight;
        const totalCost = oldPrice * oldWeight + newPrice * newWeight;

        return {
            averagePrice: totalCost / totalWeight,
            totalWeight,
            newWeight,
            newPrice,
            newCost: newPrice * newWeight,
        };
    }

    function calculateAverageByAmount(input) {
        const oldPrice = input.oldPrice;
        const oldWeight = input.oldWeight;
        const amount = input.amount;
        const pricePerGram = input.pricePerGram;

        assertNonNegative(oldWeight, "原有克重不能小于 0。");
        if (oldWeight > 0) {
            assertPositive(oldPrice, "原有单价必须大于 0。");
        }
        assertPositive(amount, "买入金额必须大于 0。");
        assertPositive(pricePerGram, "每克价格必须大于 0。");

        return calculateAverageByWeight({
            oldPrice,
            oldWeight,
            newPrice: pricePerGram,
            newWeight: amount / pricePerGram,
        });
    }

    return {
        calculateAverageByWeight,
        calculateAverageByAmount,
    };
});
