let meta = #{
    name: "layout-foundation",
    description: "Full-force layout foundation: plan, implement KWin-quality code, pure tests, package build, optional host switch on lea",
    when_to_use: "Harden kwin-tiling StackColumn + pure math foundation; commit on main; deploy to lea when allow_switch",
    phases: [
        #{ title: "Survey", detail: "repo state, engines, pure suite, dirty work" },
        #{ title: "Plan", detail: "reuse-first tasks with acceptance criteria" },
        #{ title: "Implement", detail: "KWin-quality code + pure tests + commits (no PR)" },
        #{ title: "Build", detail: "pure suite + flake check + nix build .#kwin-tiling" },
        #{ title: "Deploy", detail: "push, pin luxusAi kwin-tiling, nh os switch when allowed" },
        #{ title: "Report", detail: "landed work + session smoke for user" },
    ],
};

// args:
//   dry_run (bool)
//   skip_build (bool)
//   skip_commit (bool)
//   include_layouts (bool)
//   allow_switch (bool) — when true, push + flake update + nh os switch on lea
//   skip_push (bool)    — commit only, no origin push / no switch

let dry_run = false;
let skip_build = false;
let skip_commit = false;
let include_layouts = false;
let allow_switch = false;
let skip_push = false;
if args != () {
    if args.dry_run != () { dry_run = args.dry_run; }
    if args.skip_build != () { skip_build = args.skip_build; }
    if args.skip_commit != () { skip_commit = args.skip_commit; }
    if args.include_layouts != () { include_layouts = args.include_layouts; }
    if args.allow_switch != () { allow_switch = args.allow_switch; }
    if args.skip_push != () { skip_push = args.skip_push; }
}

let survey_schema = #{
    "type": "object",
    "required": ["summary", "branch", "dirty", "engines", "pure_tests", "blockers"],
    "properties": #{
        "summary": #{ "type": "string" },
        "branch": #{ "type": "string" },
        "dirty": #{ "type": "boolean" },
        "engines": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "pure_tests": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 20 },
        "shared_primitives": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 16 },
        "duplication_hotspots": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "uncommitted_notes": #{ "type": "string" },
        "blockers": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "luxusai_pin": #{ "type": "string" },
    },
};

let plan_schema = #{
    "type": "object",
    "required": ["goal", "tasks", "out_of_scope", "success"],
    "properties": #{
        "goal": #{ "type": "string" },
        "tasks": #{
            "type": "array",
            "minItems": 1,
            "maxItems": 12,
            "items": #{
                "type": "object",
                "required": ["id", "title", "why", "files", "acceptance", "priority"],
                "properties": #{
                    "id": #{ "type": "string" },
                    "title": #{ "type": "string" },
                    "why": #{ "type": "string" },
                    "files": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
                    "acceptance": #{ "type": "string" },
                    "priority": #{ "type": "string" },
                    "pure_test": #{ "type": "string" },
                },
            },
        },
        "out_of_scope": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "success": #{ "type": "string" },
        "architecture_rule": #{ "type": "string" },
    },
};

let impl_schema = #{
    "type": "object",
    "required": ["changed", "tasks_done", "tasks_skipped", "notes", "pure_tests_pass", "commits"],
    "properties": #{
        "changed": #{ "type": "boolean" },
        "tasks_done": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "tasks_skipped": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "notes": #{ "type": "string" },
        "pure_tests_pass": #{ "type": "boolean" },
        "commits": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "files_touched": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 40 },
        "blockers": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "head_sha": #{ "type": "string" },
    },
};

let build_schema = #{
    "type": "object",
    "required": ["flake_check_ok", "package_build_ok", "notes", "ran_package_build"],
    "properties": #{
        "flake_check_ok": #{ "type": "boolean" },
        "package_build_ok": #{ "type": "boolean" },
        "ran_package_build": #{ "type": "boolean" },
        "notes": #{ "type": "string" },
        "store_path": #{ "type": "string" },
        "commands": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
    },
};

let deploy_schema = #{
    "type": "object",
    "required": ["pushed", "lock_updated", "switched", "notes"],
    "properties": #{
        "pushed": #{ "type": "boolean" },
        "lock_updated": #{ "type": "boolean" },
        "switched": #{ "type": "boolean" },
        "notes": #{ "type": "string" },
        "generation": #{ "type": "string" },
        "kwin_path": #{ "type": "string" },
        "commands": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 16 },
    },
};

let report_schema = #{
    "type": "object",
    "required": ["summary", "landed", "session_tasks", "next"],
    "properties": #{
        "summary": #{ "type": "string" },
        "landed": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 16 },
        "session_tasks": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 20 },
        "next": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "commits": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 12 },
        "build_status": #{ "type": "string" },
        "deploy_status": #{ "type": "string" },
        "user_action_required": #{ "type": "string" },
    },
};

// ---------------------------------------------------------------------------
phase("Survey");
log("Survey kwin-tiling foundation on lea");

let survey_prompt = "";
survey_prompt += "Survey ~/projects/kwin-tiling for the shared layout foundation. Host=lea.\n";
survey_prompt += "Use tools. Do not edit. Do not switch the OS in this phase.\n";
survey_prompt += "1) git status -sb; branch; log --oneline -10\n";
survey_prompt += "2) Layout kinds/engines under pkgs/kwin-tiling/src/tiles/\n";
survey_prompt += "3) Pure suite list + bash pkgs/kwin-tiling/tests/run.sh\n";
survey_prompt += "4) Uncommitted work notes; duplication hotspots\n";
survey_prompt += "5) How luxusAi pins kwin-tiling (flake.nix inputs + lock rev)\n";
survey_prompt += "Architecture: StackColumn + pure math + thin engines; Scrolling isolated.\n";
survey_prompt += "KWin quality bar: clear ownership, no silent fallbacks, SPDX headers, tested pure paths.\n";

let survey = agent(survey_prompt, #{
    label: "survey",
    capability_mode: "execute",
    output_schema: survey_schema,
});

let survey_summary = "survey unavailable";
let survey_out = #{
    summary: survey_summary,
    branch: "main",
    dirty: true,
    engines: [],
    pure_tests: [],
    shared_primitives: [],
    duplication_hotspots: [],
    uncommitted_notes: "",
    blockers: ["survey agent failed"],
    luxusai_pin: "",
};
if survey != () && survey.success && survey.output != () {
    survey_out = survey.output;
    if survey.output.summary != () {
        survey_summary = survey.output.summary;
    }
    log("Survey: " + survey_summary);
} else {
    log("Survey failed — continuing with empty context");
}

// ---------------------------------------------------------------------------
phase("Plan");
log("Plan reuse-first foundation tasks");

let plan_prompt = "";
plan_prompt += "Concrete IMPLEMENTATION tasks for kwin-tiling layout foundation.\n";
plan_prompt += "Do not edit code. Prefer finishing uncommitted work first.\n";
plan_prompt += "Survey:\n" + json_encode(survey_out) + "\n";
plan_prompt += "Constraints: reuse StackColumn+pure math; no mega-engine; Scrolling separate;\n";
plan_prompt += "KWin-quality C++ (clear APIs, comments why not what, SPDX, no dead knobs);\n";
plan_prompt += "commit on main no PR; pure tests required.\n";
if include_layouts {
    plan_prompt += "include_layouts=true: may add thin Columns/Monocle after foundation.\n";
} else {
    plan_prompt += "include_layouts=false: no Columns/Monocle feature work.\n";
}
plan_prompt += "Return goal, tasks[], out_of_scope, success, architecture_rule.\n";

let plan = agent(plan_prompt, #{
    label: "plan",
    capability_mode: "read-only",
    output_schema: plan_schema,
});

let planned = #{
    goal: "Shared StackColumn + pure math foundation; thin engines; pure tests green; deployable on lea.",
    architecture_rule: "Compose StackColumn + pure headers; LayoutEngine base owns zoom/resize gates; Scrolling isolates viewport; never fork leaf lifecycle per engine.",
    out_of_scope: ["KineticWE packaging/borders", "mega multi-mode engine"],
    success: "pure suite + package build green; commits pushed; host switched if allow_switch.",
    tasks: [
        #{
            id: "T1",
            title: "Land pure foundation: leafcolumn, movefsm, dead-knob removal, controller wiring",
            why: "Uncommitted foundation must ship cleanly",
            files: ["pkgs/kwin-tiling/src/tiles/leafcolumn.h", "pkgs/kwin-tiling/src/tiling/movefsm.h", "pkgs/kwin-tiling/src/tiling/tilingcontroller.cpp"],
            acceptance: "tests/run.sh green; FloatAbove/StackedClass removed; movefsm drives finish",
            priority: "P0",
            pure_test: "leafcolumn_test movefsm_test",
        },
        #{
            id: "T2",
            title: "Document composition architecture (README + design.mdx)",
            why: "Prevent future engine copy-paste",
            files: ["pkgs/kwin-tiling/README.md", "website/src/content/docs/design.mdx"],
            acceptance: "All LayoutKinds listed; StackColumn rule explicit",
            priority: "P0",
            pure_test: "",
        },
        #{
            id: "T3",
            title: "Wire flake checks for new pure tests",
            why: "CI/flake check must cover leafcolumn and movefsm",
            files: ["flake.nix"],
            acceptance: "nix flake check includes new tests",
            priority: "P0",
            pure_test: "pure-suite",
        },
    ],
};

if include_layouts {
    planned.tasks.push(#{
        id: "T4",
        title: "Thin Columns layout on StackColumn",
        why: "Main layout gap vs KineticWE",
        files: ["pkgs/kwin-tiling/src/tiles/"],
        acceptance: "LayoutKind + pure policy tests",
        priority: "P2",
        pure_test: "column policy tests",
    });
}

let used_fallback_plan = true;
if plan != () && plan.success && plan.output.tasks != () && plan.output.tasks.len() > 0 {
    planned.goal = plan.output.goal;
    planned.tasks = plan.output.tasks;
    planned.success = plan.output.success;
    if plan.output.architecture_rule != () { planned.architecture_rule = plan.output.architecture_rule; }
    if plan.output.out_of_scope != () { planned.out_of_scope = plan.output.out_of_scope; }
    used_fallback_plan = false;
    log("Using planner task list");
} else {
    log("Using built-in fallback task list");
}

let plan_path = write_scratch_file("plan.json", json_encode(planned));
log("Plan: " + planned.tasks.len().to_string() + " tasks");

if dry_run {
    phase("Report");
    let dry = "# layout-foundation dry run\n\n" + planned.goal + "\n\n";
    for t in planned.tasks {
        dry += "- " + t.id + " " + t.title + "\n";
    }
    let p = write_scratch_file("report.md", dry);
    complete(#{ summary: "Dry run only", path: p, plan: plan_path, dry_run: true, used_fallback_plan: used_fallback_plan });
}

// ---------------------------------------------------------------------------
phase("Implement");
log("Implement at KWin quality; commit; pure tests");

let impl_prompt = "";
impl_prompt += "IMPLEMENT layout foundation in ~/projects/kwin-tiling at KDE/KWin quality.\n";
impl_prompt += "Host lea. You may edit, test, commit.\n\n";
impl_prompt += "QUALITY BAR (do not ship sloppy code):\n";
impl_prompt += "- Match existing style: SPDX, namespaces, Q_UNUSED, early returns, short why-comments.\n";
impl_prompt += "- Pure logic in headers with *_test.cpp twins; engines stay thin.\n";
impl_prompt += "- No mega multi-mode LayoutEngine. Scrolling stays isolated.\n";
impl_prompt += "- Prefer finishing dirty tree work over speculative refactors.\n";
impl_prompt += "- Small clear commits (why in message). No force-push. No PR.\n";
impl_prompt += "- After changes: bash pkgs/kwin-tiling/tests/run.sh MUST pass.\n";
if skip_commit {
    impl_prompt += "- skip_commit=true: do not git commit.\n";
} else {
    impl_prompt += "- Commit coherent groups when pure tests pass.\n";
}
if !include_layouts {
    impl_prompt += "- No Columns/Monocle LayoutKind in this run.\n";
}
impl_prompt += "\nPLAN:\n" + json_encode(planned) + "\n";
impl_prompt += "SURVEY:\n" + json_encode(survey_out) + "\n";
impl_prompt += "Return changed, tasks_done, tasks_skipped, notes, pure_tests_pass, commits, head_sha, files_touched, blockers.\n";

let impl = agent(impl_prompt, #{
    label: "implement",
    capability_mode: "all",
    output_schema: impl_schema,
});

let pure_ok = false;
let head_sha = "";
if impl != () && impl.success {
    pure_ok = impl.output.pure_tests_pass == true;
    if impl.output.head_sha != () { head_sha = impl.output.head_sha; }
    log("Implement: " + impl.output.notes);
} else {
    log("Implement agent failed — build will re-verify");
}

// ---------------------------------------------------------------------------
phase("Build");
log("Pure suite + package build");

let build_prompt = "";
build_prompt += "VERIFY ~/projects/kwin-tiling.\n";
build_prompt += "1) bash pkgs/kwin-tiling/tests/run.sh\n";
build_prompt += "2) nix flake check\n";
if skip_build {
    build_prompt += "3) skip_build=true: do not nix build package. ran_package_build=false.\n";
} else {
    build_prompt += "3) nix build .#kwin-tiling (long). Capture result path.\n";
}
build_prompt += "Do not switch OS here. Fix compile breaks if you introduced them; re-run pure tests.\n";

let build = agent(build_prompt, #{
    label: "build",
    capability_mode: "all",
    output_schema: build_schema,
});

let flake_ok = false;
let pkg_ok = false;
let ran_pkg = false;
let store_path = "";
if build != () && build.success {
    flake_ok = build.output.flake_check_ok == true;
    pkg_ok = build.output.package_build_ok == true;
    ran_pkg = build.output.ran_package_build == true;
    if build.output.store_path != () { store_path = build.output.store_path; }
    if flake_ok { pure_ok = true; }
    log("Build: " + build.output.notes);
} else {
    log("Build agent failed");
}

// ---------------------------------------------------------------------------
phase("Deploy");
let deployed = #{
    pushed: false,
    lock_updated: false,
    switched: false,
    notes: "deploy skipped",
    generation: "",
    kwin_path: "",
    commands: [],
};

if allow_switch && pure_ok && (pkg_ok || skip_build) {
    log("Deploy: push + luxusAi pin + nh os switch (allowed)");
    let dep_prompt = "";
    dep_prompt += "DEPLOY kwin-tiling foundation to host lea. PATH=/run/wrappers/bin first for nh/sudo.\n";
    dep_prompt += "Repo kwin-tiling: ~/projects/kwin-tiling on main.\n";
    dep_prompt += "Host flake: ~/projects/luxusAi (inputs.kwin-tiling = github:luxus/kwin-tiling).\n\n";
    if skip_push {
        dep_prompt += "skip_push=true: do not push or switch. Report what you would do.\n";
    } else {
        dep_prompt += "1) Ensure pure tests still pass.\n";
        dep_prompt += "2) If uncommitted changes remain, commit them (clear message) or stash only if unrelated.\n";
        dep_prompt += "3) git push origin main for kwin-tiling (no force).\n";
        dep_prompt += "4) In luxusAi: nix flake update kwin-tiling; commit flake.lock if changed\n";
        dep_prompt += "   (message: flake: bump kwin-tiling for layout foundation).\n";
        dep_prompt += "5) nh os switch . from luxusAi (or the host's usual entry). High timeout.\n";
        dep_prompt += "6) Report generation (nh os info or readlink /run/current-system) and\n";
        dep_prompt += "   which kwin binary path is active if discoverable.\n";
        dep_prompt += "Note: compositor may need session restart/relogin for new kwin — state that clearly.\n";
    }
    dep_prompt += "Return pushed, lock_updated, switched, notes, generation, kwin_path, commands.\n";

    let dep = agent(dep_prompt, #{
        label: "deploy",
        capability_mode: "all",
        output_schema: deploy_schema,
    });
    if dep != () && dep.success {
        deployed = dep.output;
        log("Deploy: " + dep.output.notes);
    } else {
        deployed.notes = "deploy agent failed";
        log("Deploy failed");
    }
} else if allow_switch {
    deployed.notes = "deploy blocked: pure/package not green (pure_ok=" + pure_ok.to_string()
        + " pkg_ok=" + pkg_ok.to_string() + ")";
    log(deployed.notes);
} else {
    deployed.notes = "allow_switch=false — package built only; user switches later";
    log(deployed.notes);
}

// ---------------------------------------------------------------------------
phase("Report");
log("Final report + session tasks");

let report_prompt = "";
report_prompt += "Final handoff for layout-foundation on lea. Confirm git log/status with tools.\n";
report_prompt += "survey=" + json_encode(survey_out) + "\n";
report_prompt += "plan=" + json_encode(planned) + "\n";
if impl != () && impl.success { report_prompt += "impl=" + json_encode(impl.output) + "\n"; }
if build != () && build.success { report_prompt += "build=" + json_encode(build.output) + "\n"; }
report_prompt += "deploy=" + json_encode(deployed) + "\n";
report_prompt += "Write summary, landed[], commits[], build_status, deploy_status,\n";
report_prompt += "session_tasks from pkgs/kwin-tiling/scripts/session-smoke.md (user runs in Plasma),\n";
report_prompt += "user_action_required (relogin if switch landed), next[].\n";

let report = agent(report_prompt, #{
    label: "report",
    capability_mode: "execute",
    output_schema: report_schema,
});

let md = "# layout-foundation\n\n";
if report != () && report.success {
    md += report.output.summary + "\n\n## Landed\n";
    if report.output.landed != () {
        for x in report.output.landed { md += "- " + x + "\n"; }
    }
    md += "\n## Commits\n";
    if report.output.commits != () {
        for c in report.output.commits { md += "- " + c + "\n"; }
    }
    md += "\n## Build\n";
    if report.output.build_status != () { md += report.output.build_status + "\n"; }
    md += "\n## Deploy\n";
    if report.output.deploy_status != () { md += report.output.deploy_status + "\n"; }
    else { md += deployed.notes + "\n"; }
    md += "\n## Session tests\n";
    if report.output.user_action_required != () { md += report.output.user_action_required + "\n\n"; }
    if report.output.session_tasks != () {
        for s in report.output.session_tasks { md += "- [ ] " + s + "\n"; }
    }
    md += "\n## Next\n";
    if report.output.next != () {
        for n in report.output.next { md += "- " + n + "\n"; }
    }
} else {
    md += "Report agent failed.\n\n";
    md += "pure_ok=" + pure_ok.to_string() + " pkg_ok=" + pkg_ok.to_string() + "\n";
    md += "deploy: " + deployed.notes + "\n";
    md += "\n## Session smoke\n";
    md += "- [ ] Meta+Shift+F flip master + focus geometry\n";
    md += "- [ ] Cross-monitor drag: no phantom tiles\n";
    md += "- [ ] Toggle tiling off/on\n";
    md += "- [ ] Meta+W float; Keep Above independent\n";
    md += "- [ ] Meta+Shift+R retile; Scrolling OK\n";
}

let report_path = write_scratch_file("report.md", md);
let summary_text = "layout-foundation complete";
if report != () && report.success && report.output.summary != () {
    summary_text = report.output.summary;
}

complete(#{
    summary: summary_text,
    path: report_path,
    plan: plan_path,
    pure_tests_pass: pure_ok,
    package_build_ok: pkg_ok,
    ran_package_build: ran_pkg,
    store_path: store_path,
    head_sha: head_sha,
    deployed: deployed,
    allow_switch: allow_switch,
});
