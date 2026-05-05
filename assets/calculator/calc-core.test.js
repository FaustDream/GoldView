const assert = require("node:assert/strict");
const {
    calculateAverageByWeight,
    calculateAverageByAmount,
} = require("./calc-core.js");

function approxEqual(actual, expected, epsilon = 0.0000001) {
    assert.ok(
        Math.abs(actual - expected) < epsilon,
        `expected ${actual} to be close to ${expected}`
    );
}

function test(name, fn) {
    fn();
    console.log(`ok - ${name}`);
}

test("按克重买入时使用持仓成本加本次成本计算加权均价", function () {
    const result = calculateAverageByWeight({
        oldPrice: 700,
        oldWeight: 10,
        newPrice: 800,
        newWeight: 5,
    });

    assert.equal(result.totalWeight, 15);
    assert.equal(result.newCost, 4000);
    approxEqual(result.averagePrice, (700 * 10 + 800 * 5) / 15);
});

test("按金额买入时先用金额除以每克价格折算可买克重", function () {
    const result = calculateAverageByAmount({
        oldPrice: 700,
        oldWeight: 10,
        amount: 1600,
        pricePerGram: 800,
    });

    assert.equal(result.newWeight, 2);
    assert.equal(result.totalWeight, 12);
    approxEqual(result.averagePrice, (700 * 10 + 1600) / 12);
});

test("首次买入没有原持仓时均价等于本次每克价格", function () {
    const result = calculateAverageByAmount({
        oldPrice: 0,
        oldWeight: 0,
        amount: 1000,
        pricePerGram: 500,
    });

    assert.equal(result.newWeight, 2);
    assert.equal(result.totalWeight, 2);
    assert.equal(result.averagePrice, 500);
});

test("低于原均价补仓时新均价应下降且保持在两次价格之间", function () {
    const result = calculateAverageByWeight({
        oldPrice: 760,
        oldWeight: 8,
        newPrice: 720,
        newWeight: 4,
    });

    assert.equal(result.totalWeight, 12);
    approxEqual(result.averagePrice, 746.6666666666666);
    assert.ok(result.averagePrice < 760);
    assert.ok(result.averagePrice > 720);
});

test("高于原均价加仓时新均价应上升且保持在两次价格之间", function () {
    const result = calculateAverageByWeight({
        oldPrice: 720,
        oldWeight: 12.5,
        newPrice: 735.2,
        newWeight: 5.35,
    });

    assert.equal(result.totalWeight, 17.85);
    approxEqual(result.newCost, 3933.32);
    approxEqual(result.averagePrice, (720 * 12.5 + 735.2 * 5.35) / 17.85);
    assert.ok(result.averagePrice > 720);
    assert.ok(result.averagePrice < 735.2);
});

test("按金额买入支持小数金额和小数克价", function () {
    const result = calculateAverageByAmount({
        oldPrice: 728.88,
        oldWeight: 3.21,
        amount: 999.99,
        pricePerGram: 731.45,
    });

    approxEqual(result.newWeight, 999.99 / 731.45);
    approxEqual(result.totalWeight, 3.21 + 999.99 / 731.45);
    approxEqual(result.averagePrice, (728.88 * 3.21 + 999.99) / result.totalWeight);
});

test("非法黄金买入参数会被拒绝", function () {
    assert.throws(() => calculateAverageByAmount({
        oldPrice: 700,
        oldWeight: 10,
        amount: 1600,
        pricePerGram: 0,
    }), /每克价格必须大于 0/);

    assert.throws(() => calculateAverageByAmount({
        oldPrice: 700,
        oldWeight: 10,
        amount: -1,
        pricePerGram: 800,
    }), /买入金额必须大于 0/);

    assert.throws(() => calculateAverageByWeight({
        oldPrice: 700,
        oldWeight: -0.1,
        newPrice: 800,
        newWeight: 1,
    }), /原有克重不能小于 0/);
});

console.log("calculator core tests passed");
