/*
    SPDX-FileCopyrightText: 2023-2025 Peter Fajdiga
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-3.0-only

    Pure helpers for the tiling-reflow KWin effect.
    Consumes WindowTilingReflowRole (QVariantMap from tilingreflow.h) when
    present; otherwise infers motion from the geometry delta (upstream
    geometry_change behaviour).
*/

var TilingReflowAnimation = (function () {
    "use strict";

    var Reason = {
        Reflow: 0,
        Add: 1,
        Remove: 2,
        Migrate: 3,
        GapChange: 4,
        LayoutSwitch: 5,
        Retile: 6,
        DesktopSwitch: 7,
        Float: 8,
        Swap: 9,
        Insert: 10,
        Resize: 11,
        Reorder: 12,
    };

    // Matches ReflowHint::Direction in tilingreflow.h / tilingreflow.cpp:
    // FromRight means the window center moved +x (rightward travel), so the
    // painted window starts to the *left* of the new geometry and slides right.
    var Direction = {
        None: 0,
        FromLeft: 1,
        FromRight: 2,
        FromAbove: 3,
        FromBelow: 4,
    };

    var LayoutKind = {
        MasterStack: 0,
        Stacked: 1,
        Scrolling: 2,
        Centered: 3,
        Grid: 4,
    };

    var AXIS_EPSILON = 2;
    var FALLBACK_SLIDE_FRACTION = 0.25;

    function isFiniteNumber(value) {
        return typeof value === "number" && isFinite(value);
    }

    function toInt(value, fallback) {
        var n = Number(value);
        if (!isFinite(n)) {
            return fallback;
        }
        return n | 0;
    }

    function parseHint(raw) {
        if (raw === undefined || raw === null || raw === false || raw === "") {
            return null;
        }
        if (typeof raw !== "object") {
            return null;
        }
        var hasReason = Object.prototype.hasOwnProperty.call(raw, "reason");
        var hasDirection = Object.prototype.hasOwnProperty.call(raw, "direction");
        if (!hasReason && !hasDirection) {
            return null;
        }
        return {
            reason: toInt(raw.reason, Reason.Reflow),
            direction: toInt(raw.direction, Direction.None),
            layout: toInt(raw.layout, LayoutKind.MasterStack),
            groupId: toInt(raw.groupId, 0),
            staggerIndex: toInt(raw.staggerIndex, 0),
        };
    }

    function motionForHint(hint, options) {
        var duration = options.duration;
        var crossFade = !!options.crossFade;
        var curve = "OutCubic";
        var fade = false;
        var useHintDirection = false;

        if (!hint) {
            return {
                duration: duration,
                curve: "OutExpo",
                crossFade: crossFade,
                fade: false,
                useHintDirection: false,
                delay: 0,
            };
        }

        switch (hint.reason) {
            case Reason.DesktopSwitch:
                duration = Math.max(1, Math.round(duration * 0.6));
                curve = "OutCubic";
                crossFade = false;
                useHintDirection = hint.direction !== Direction.None;
                break;
            case Reason.LayoutSwitch:
                curve = "InOutCubic";
                crossFade = true;
                useHintDirection = false;
                break;
            case Reason.Float:
                curve = "OutCubic";
                fade = true;
                useHintDirection = false;
                break;
            case Reason.GapChange:
            case Reason.Resize:
                curve = "OutCubic";
                useHintDirection = false;
                break;
            default:
                curve = "OutCubic";
                useHintDirection = hint.direction !== Direction.None;
                break;
        }

        var staggerMs = options.staggerMs || 0;
        var delay = 0;
        if (staggerMs > 0 && hint.staggerIndex > 0) {
            delay = hint.staggerIndex * staggerMs;
        }

        return {
            duration: duration,
            curve: curve,
            crossFade: crossFade,
            fade: fade,
            useHintDirection: useHintDirection,
            delay: delay,
        };
    }

    function zeroNeg(n) {
        return n === 0 ? 0 : n;
    }

    function scaleCompensation(oldGeometry, newGeometry) {
        return {
            x: -(newGeometry.width - oldGeometry.width) / 2,
            y: -(newGeometry.height - oldGeometry.height) / 2,
        };
    }

    function slideDistance(delta, size) {
        var abs = Math.abs(delta);
        if (abs >= AXIS_EPSILON) {
            return abs;
        }
        return size * FALLBACK_SLIDE_FRACTION;
    }

    function translationFrom(oldGeometry, newGeometry, hint, motion) {
        var xDelta = newGeometry.x - oldGeometry.x;
        var yDelta = newGeometry.y - oldGeometry.y;
        var widthDelta = newGeometry.width - oldGeometry.width;
        var heightDelta = newGeometry.height - oldGeometry.height;
        var comp = scaleCompensation(oldGeometry, newGeometry);

        if (!motion.useHintDirection || !hint) {
            return {
                value1: zeroNeg(-xDelta - widthDelta / 2),
                value2: zeroNeg(-yDelta - heightDelta / 2),
            };
        }

        var tx = comp.x;
        var ty = comp.y;
        switch (hint.direction) {
            case Direction.FromRight:
                // Rightward travel: start left of the new geometry.
                tx = -slideDistance(xDelta, newGeometry.width) + comp.x;
                break;
            case Direction.FromLeft:
                tx = slideDistance(xDelta, newGeometry.width) + comp.x;
                break;
            case Direction.FromBelow:
                ty = -slideDistance(yDelta, newGeometry.height) + comp.y;
                break;
            case Direction.FromAbove:
                ty = slideDistance(yDelta, newGeometry.height) + comp.y;
                break;
            case Direction.None:
            default:
                break;
        }
        return {
            value1: zeroNeg(tx),
            value2: zeroNeg(ty),
        };
    }

    function scaleFrom(oldGeometry, newGeometry) {
        return {
            value1: oldGeometry.width / newGeometry.width,
            value2: oldGeometry.height / newGeometry.height,
        };
    }

    function nearlyEqual(a, b, epsilon) {
        return Math.abs(a - b) < epsilon;
    }

    function buildAnimationPlan(oldGeometry, newGeometry, hint, options) {
        options = options || {};
        var preferHints = options.preferHints !== false;

        if (!newGeometry || newGeometry.width <= 0 || newGeometry.height <= 0) {
            return { skip: true };
        }
        if (!oldGeometry || oldGeometry.width <= 0 || oldGeometry.height <= 0) {
            return { skip: true };
        }

        var effectiveHint = preferHints ? hint : null;
        var motion = motionForHint(effectiveHint, {
            duration: options.duration || 220,
            crossFade: !!options.crossFade,
            staggerMs: options.staggerMs || 0,
        });

        var translation = translationFrom(oldGeometry, newGeometry, effectiveHint, motion);
        var scale = scaleFrom(oldGeometry, newGeometry);

        var noMove =
            nearlyEqual(translation.value1, 0, 0.5) &&
            nearlyEqual(translation.value2, 0, 0.5) &&
            nearlyEqual(scale.value1, 1, 0.001) &&
            nearlyEqual(scale.value2, 1, 0.001);

        if (noMove && !motion.fade && !motion.crossFade) {
            return { skip: true };
        }

        return {
            skip: false,
            duration: motion.duration,
            delay: motion.delay,
            curve: motion.curve,
            crossFade: motion.crossFade,
            fade: motion.fade,
            translation: translation,
            scale: scale,
            reason: effectiveHint ? effectiveHint.reason : null,
            direction: effectiveHint ? effectiveHint.direction : null,
        };
    }

    return {
        Reason: Reason,
        Direction: Direction,
        LayoutKind: LayoutKind,
        parseHint: parseHint,
        motionForHint: motionForHint,
        translationFrom: translationFrom,
        buildAnimationPlan: buildAnimationPlan,
        isFiniteNumber: isFiniteNumber,
    };
})();

if (typeof module === "object" && module.exports) {
    module.exports = TilingReflowAnimation;
}
