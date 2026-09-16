/*
    SPDX-FileCopyrightText: 2023-2025 Peter Fajdiga
    SPDX-FileCopyrightText: 2026 luxus
    SPDX-License-Identifier: GPL-3.0-only

    Fork of kwin4_effect_geometry_change: animate programmatic move/resize,
    reading WindowTilingReflowRole when the native tiling patch published a
    hint, otherwise falling back to geometry-delta inference.
*/

"use strict";

function easingCurve(name) {
    if (typeof QEasingCurve === "undefined") {
        return 0;
    }
    if (name === "InOutCubic") {
        return QEasingCurve.InOutCubic;
    }
    if (name === "OutExpo") {
        return QEasingCurve.OutExpo;
    }
    return QEasingCurve.OutCubic;
}

function readHintFromWindow(window) {
    if (typeof Effect === "undefined" || typeof Effect.WindowTilingReflowRole === "undefined") {
        return null;
    }
    var raw = null;
    if (typeof effect !== "undefined" && typeof effect.readWindowData === "function") {
        raw = effect.readWindowData(window, Effect.WindowTilingReflowRole);
    } else if (typeof window.data === "function") {
        raw = window.data(Effect.WindowTilingReflowRole);
    }
    return TilingReflowAnimation.parseHint(raw);
}

function clearHint(window) {
    if (typeof Effect === "undefined" || typeof Effect.WindowTilingReflowRole === "undefined") {
        return;
    }
    if (typeof window.setData === "function") {
        window.setData(Effect.WindowTilingReflowRole, null);
    }
}

class TilingReflowEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(this.restoreForceBlurState.bind(this));

        const manageFn = this.manage.bind(this);
        effects.windowAdded.connect(manageFn);
        effects.stackingOrder.forEach(manageFn);

        this.loadConfig();
    }

    loadConfig() {
        const duration = effect.readConfig("Duration", 220);
        this.duration = animationTime(duration);
        this.excludedWindowClasses = effect
            .readConfig("ExcludedWindowClasses", "krunner,yakuake,plasmashell,org.kde.plasmashell")
            .split(",");
        this.crossFade = effect.readConfig("CrossFade", false);
        this.preferHints = effect.readConfig("PreferHints", true);
        this.staggerMs = effect.readConfig("StaggerMs", 16);
    }

    manage(window) {
        window.geometryChangeData = {
            createdTime: Date.now(),
            animationInstances: 0,
            maximizedStateAboutToChange: false,
        };
        window.windowFrameGeometryChanged.connect(this.onWindowFrameGeometryChanged.bind(this));
        window.windowMaximizedStateAboutToChange.connect(
            this.onWindowMaximizedStateAboutToChange.bind(this),
        );
        window.windowStartUserMovedResized.connect(this.onWindowStartUserMovedResized.bind(this));
        window.windowFinishUserMovedResized.connect(this.onWindowFinishUserMovedResized.bind(this));
    }

    restoreForceBlurState(window) {
        window.geometryChangeData.animationInstances--;
        if (window.geometryChangeData.animationInstances === 0) {
            window.setData(Effect.WindowForceBlurRole, null);
        }
    }

    isWindowClassExcluded(windowClass) {
        return windowClass.split(" ").some((part) => this.excludedWindowClasses.includes(part));
    }

    onWindowFrameGeometryChanged(window, oldGeometry) {
        const windowTypeSupportsAnimation = window.normalWindow || window.dialog || window.modal;
        const isUserMoveResize = window.move || window.resize || this.userResizing;
        const maximizationChange = window.geometryChangeData.maximizedStateAboutToChange;
        window.geometryChangeData.maximizedStateAboutToChange = false;
        if (
            !window.managed ||
            !window.visible ||
            !window.onCurrentDesktop ||
            window.minimized ||
            !windowTypeSupportsAnimation ||
            (isUserMoveResize && !maximizationChange) ||
            this.isWindowClassExcluded(window.windowClass)
        ) {
            return;
        }

        if (maximizationChange && effects.activeEffects.includes("maximize")) {
            return;
        }

        const windowAgeMs = Date.now() - window.geometryChangeData.createdTime;
        if (windowAgeMs < 0) {
            window.geometryChangeData.createdTime = Date.now();
        } else if (windowAgeMs < 10) {
            return;
        }

        const hint = this.preferHints ? readHintFromWindow(window) : null;
        if (this.preferHints) {
            clearHint(window);
        }

        const plan = TilingReflowAnimation.buildAnimationPlan(oldGeometry, window.geometry, hint, {
            duration: this.duration,
            crossFade: this.crossFade,
            staggerMs: this.staggerMs,
            preferHints: this.preferHints,
        });
        if (plan.skip) {
            return;
        }

        const animations = [
            {
                type: Effect.Translation,
                from: plan.translation,
                to: { value1: 0, value2: 0 },
            },
            {
                type: Effect.Scale,
                from: plan.scale,
                to: { value1: 1, value2: 1 },
            },
        ];

        if (plan.crossFade) {
            animations.push({
                type: Effect.CrossFadePrevious,
                from: 0,
                to: 1,
            });
        }

        if (plan.fade) {
            animations.push({
                type: Effect.Opacity,
                from: 0,
                to: 1,
            });
        }

        window.geometryChangeData.animationInstances += animations.length;
        window.setData(Effect.WindowForceBlurRole, true);

        animate({
            window: window,
            duration: plan.duration,
            delay: plan.delay,
            curve: easingCurve(plan.curve),
            animations: animations,
        });
    }

    onWindowMaximizedStateAboutToChange(window, horizontal, vertical) {
        window.geometryChangeData.maximizedStateAboutToChange = true;
    }

    onWindowStartUserMovedResized(window) {
        this.userResizing = true;
    }

    onWindowFinishUserMovedResized(window) {
        this.userResizing = false;
    }
}

new TilingReflowEffect();
