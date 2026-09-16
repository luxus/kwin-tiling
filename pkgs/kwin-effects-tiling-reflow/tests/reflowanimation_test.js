"use strict";

const assert = require("assert");
const path = require("path");
const A = require(path.join(__dirname, "..", "lib", "reflowanimation.js"));

function rect(x, y, width, height) {
    return { x, y, width, height };
}

function testParseHint() {
    assert.strictEqual(A.parseHint(null), null);
    assert.strictEqual(A.parseHint(undefined), null);
    assert.strictEqual(A.parseHint(""), null);
    assert.strictEqual(A.parseHint(42), null);
    assert.strictEqual(A.parseHint({}), null);
    assert.strictEqual(A.parseHint({ leftover: 1 }), null);

    const hint = A.parseHint({
        reason: A.Reason.Add,
        direction: A.Direction.FromRight,
        layout: A.LayoutKind.Scrolling,
        groupId: 7,
        staggerIndex: 2,
    });
    assert.deepStrictEqual(hint, {
        reason: A.Reason.Add,
        direction: A.Direction.FromRight,
        layout: A.LayoutKind.Scrolling,
        groupId: 7,
        staggerIndex: 2,
    });

    const coerced = A.parseHint({ reason: "5", direction: "2" });
    assert.strictEqual(coerced.reason, A.Reason.LayoutSwitch);
    assert.strictEqual(coerced.direction, A.Direction.FromRight);
}

function testFallbackMatchesUpstream() {
    const oldG = rect(10, 20, 200, 100);
    const newG = rect(40, 50, 160, 80);
    const plan = A.buildAnimationPlan(oldG, newG, null, { duration: 250, crossFade: false });
    assert.strictEqual(plan.skip, false);
    assert.strictEqual(plan.curve, "OutExpo");
    assert.strictEqual(plan.fade, false);
    assert.strictEqual(plan.delay, 0);

    const xDelta = newG.x - oldG.x;
    const yDelta = newG.y - oldG.y;
    const widthDelta = newG.width - oldG.width;
    const heightDelta = newG.height - oldG.height;
    assert.strictEqual(plan.translation.value1, -xDelta - widthDelta / 2);
    assert.strictEqual(plan.translation.value2, -yDelta - heightDelta / 2);
    assert.strictEqual(plan.scale.value1, oldG.width / newG.width);
    assert.strictEqual(plan.scale.value2, oldG.height / newG.height);
}

function testDirectionalSlideUsesHintAxis() {
    const oldG = rect(100, 200, 200, 200);
    const newG = rect(300, 240, 200, 200); // +200 x, +40 y
    const hint = A.parseHint({
        reason: A.Reason.Add,
        direction: A.Direction.FromRight,
        layout: 0,
        groupId: 1,
        staggerIndex: 0,
    });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220, staggerMs: 16 });
    assert.strictEqual(plan.skip, false);
    assert.strictEqual(plan.curve, "OutCubic");
    assert.strictEqual(plan.direction, A.Direction.FromRight);
    // Cardinal slide: X uses |dx|, Y is scale-compensation only (0 here).
    assert.ok(plan.translation.value1 < 0, "rightward travel starts left of the new geom");
    assert.strictEqual(plan.translation.value1, -200);
    assert.strictEqual(plan.translation.value2, 0);
}

function testFromLeftOppositeSign() {
    const oldG = rect(300, 200, 200, 200);
    const newG = rect(100, 200, 200, 200);
    const hint = A.parseHint({
        reason: A.Reason.Remove,
        direction: A.Direction.FromLeft,
        staggerIndex: 0,
    });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220 });
    assert.ok(plan.translation.value1 > 0, "leftward travel starts right of the new geom");
    assert.strictEqual(plan.translation.value1, 200);
    assert.strictEqual(plan.translation.value2, 0);
}

function testReasonLayoutSwitchCrossFades() {
    const oldG = rect(0, 0, 400, 300);
    const newG = rect(200, 0, 400, 300);
    const hint = A.parseHint({
        reason: A.Reason.LayoutSwitch,
        direction: A.Direction.FromRight,
        staggerIndex: 1,
    });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220, staggerMs: 16 });
    assert.strictEqual(plan.crossFade, true);
    assert.strictEqual(plan.curve, "InOutCubic");
    // Layout switch ignores direction and uses the actual geometry delta.
    assert.strictEqual(plan.translation.value1, -(newG.x - oldG.x));
    assert.strictEqual(plan.delay, 16);
}

function testReasonFloatFades() {
    const oldG = rect(0, 0, 800, 600);
    const newG = rect(100, 80, 400, 300);
    const hint = A.parseHint({ reason: A.Reason.Float, direction: A.Direction.FromRight });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220 });
    assert.strictEqual(plan.fade, true);
    assert.strictEqual(plan.curve, "OutCubic");
}

function testReasonDesktopSwitchShorter() {
    const oldG = rect(0, 0, 200, 200);
    const newG = rect(10, 0, 200, 200);
    const hint = A.parseHint({ reason: A.Reason.DesktopSwitch, direction: A.Direction.None });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220 });
    assert.strictEqual(plan.duration, Math.round(220 * 0.6));
    assert.strictEqual(plan.crossFade, false);
}

function testResizeIgnoresDirection() {
    const oldG = rect(0, 0, 200, 200);
    const newG = rect(0, 0, 300, 180);
    const hint = A.parseHint({ reason: A.Reason.Resize, direction: A.Direction.FromRight });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220 });
    const widthDelta = newG.width - oldG.width;
    const heightDelta = newG.height - oldG.height;
    assert.strictEqual(plan.translation.value1, -widthDelta / 2);
    assert.strictEqual(plan.translation.value2, -heightDelta / 2);
    assert.strictEqual(plan.fade, false);
}

function testStaggerDelay() {
    const oldG = rect(0, 0, 100, 100);
    const newG = rect(50, 0, 100, 100);
    const hint = A.parseHint({
        reason: A.Reason.Reflow,
        direction: A.Direction.FromRight,
        staggerIndex: 3,
    });
    const plan = A.buildAnimationPlan(oldG, newG, hint, { duration: 220, staggerMs: 16 });
    assert.strictEqual(plan.delay, 48);
}

function testPreferHintsFalseFallsBack() {
    const oldG = rect(0, 10, 100, 100);
    const newG = rect(40, 50, 100, 100);
    const hint = A.parseHint({
        reason: A.Reason.Add,
        direction: A.Direction.FromAbove,
        staggerIndex: 2,
    });
    const plan = A.buildAnimationPlan(oldG, newG, hint, {
        duration: 250,
        preferHints: false,
        staggerMs: 16,
    });
    assert.strictEqual(plan.curve, "OutExpo");
    assert.strictEqual(plan.delay, 0);
    assert.strictEqual(plan.translation.value1, -40);
    assert.strictEqual(plan.translation.value2, -40);
}

function testSkipNoop() {
    const g = rect(10, 10, 100, 100);
    const plan = A.buildAnimationPlan(g, g, null, { duration: 220 });
    assert.strictEqual(plan.skip, true);
}

function testSkipZeroSize() {
    const plan = A.buildAnimationPlan(rect(0, 0, 100, 100), rect(0, 0, 0, 100), null, {
        duration: 220,
    });
    assert.strictEqual(plan.skip, true);
}

function testEnumsMatchCpp() {
    assert.strictEqual(A.Reason.Reflow, 0);
    assert.strictEqual(A.Reason.Add, 1);
    assert.strictEqual(A.Reason.Remove, 2);
    assert.strictEqual(A.Reason.Migrate, 3);
    assert.strictEqual(A.Reason.GapChange, 4);
    assert.strictEqual(A.Reason.LayoutSwitch, 5);
    assert.strictEqual(A.Reason.Retile, 6);
    assert.strictEqual(A.Reason.DesktopSwitch, 7);
    assert.strictEqual(A.Reason.Float, 8);
    assert.strictEqual(A.Reason.Swap, 9);
    assert.strictEqual(A.Reason.Insert, 10);
    assert.strictEqual(A.Reason.Resize, 11);
    assert.strictEqual(A.Reason.Reorder, 12);
    assert.strictEqual(A.Direction.None, 0);
    assert.strictEqual(A.Direction.FromLeft, 1);
    assert.strictEqual(A.Direction.FromRight, 2);
    assert.strictEqual(A.Direction.FromAbove, 3);
    assert.strictEqual(A.Direction.FromBelow, 4);
    assert.strictEqual(A.LayoutKind.MasterStack, 0);
    assert.strictEqual(A.LayoutKind.Grid, 4);
}

const tests = [
    testParseHint,
    testFallbackMatchesUpstream,
    testDirectionalSlideUsesHintAxis,
    testFromLeftOppositeSign,
    testReasonLayoutSwitchCrossFades,
    testReasonFloatFades,
    testReasonDesktopSwitchShorter,
    testResizeIgnoresDirection,
    testStaggerDelay,
    testPreferHintsFalseFallsBack,
    testSkipNoop,
    testSkipZeroSize,
    testEnumsMatchCpp,
];

for (const fn of tests) {
    fn();
    console.log("ok", fn.name);
}
console.log("all", tests.length, "tests passed");
