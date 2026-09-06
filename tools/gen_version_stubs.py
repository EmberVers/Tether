#!/usr/bin/env python3
"""Generate 5.4-stub .cpp files for UFUNCTIONs whose real implementations are
gated to UE 5.7+ in their main library .cpp.

UHT does not allow `UCLASS`/`USTRUCT`/`UFUNCTION`/`UPROPERTY` inside arbitrary
preprocessor blocks (only `WITH_EDITORONLY_DATA` is excepted), so we cannot
hide UFUNCTION declarations on 5.4 with `#if UE_VERSION_OLDER_THAN(...)`.
Instead:

  * The .h declares everything unconditionally — UHT is happy.
  * The main .cpp has its body inside `#if !UE_VERSION_OLDER_THAN(5, 7, 0)`,
    so on 5.4 it compiles to an empty TU.
  * This generator emits a sibling `<Name>_Stubs.cpp` whose body is inside
    `#if UE_VERSION_OLDER_THAN(5, 7, 0)`. On 5.4 it provides empty stub
    bodies (UE_LOG warning + default return) so the linker is satisfied.
    On 5.7+ the file compiles to an empty TU.

Run:
    python tools/gen_version_stubs.py             # generate
    python tools/gen_version_stubs.py --check     # exit 1 if any stubs file missing/stale,
                                                  # or if TARGETS drifted from the .cpp gates

The --check gate scan cross-references the second source of truth: for every
"function"/"functions" TARGETS entry, the U<Library>::<Function> definitions
that actually live inside `#if !UE_VERSION_OLDER_THAN(5, 7, 0)` blocks in the
main .cpp are extracted and compared against the hardcoded TARGETS list. A
5.7-gated function that was never registered in TARGETS would otherwise only
surface as a linker error on UE 5.4 (no stub body).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PUBLIC = REPO_ROOT / "Plugin" / "Tether" / "Source" / "Tether" / "Public"
PRIVATE = REPO_ROOT / "Plugin" / "Tether" / "Source" / "Tether" / "Private"

# Each entry: header stem, scope ("all" wraps every UFUNCTION in the class;
# "function" wraps just one named UFUNCTION; "functions" wraps every name in
# the supplied list — used when a library has a handful of 5.7-gated funcs
# alongside many version-stable ones).
TARGETS: list[dict] = [
    {"name": "TetherNiagaraLibrary",       "scope": "all"},
    {"name": "TetherRigLibrary",            "scope": "all"},
    {"name": "TetherStateTreeLibrary",      "scope": "all"},
    {"name": "TetherSmartObjectLibrary",    "scope": "all"},
    {"name": "TetherChooserLibrary",        "scope": "all"},
    {"name": "TetherPoseSearchLibrary",     "scope": "all"},
    {"name": "TetherMaterialLibrary",       "scope": "all"},
    {"name": "TetherNavigationLibrary",     "scope": "all"},
    {"name": "TetherGeometryLibrary",       "scope": "all"},
    {"name": "TetherPCGLibrary",             "scope": "all"},
    {"name": "TetherDataTableLibrary",      "scope": "function", "function": "CopyDataTableRows"},
    {"name": "TetherBlueprintLibrary",      "scope": "function", "function": "AddAsyncActionNode"},
    {"name": "TetherGameplayAbilityLibrary","scope": "function", "function": "AddAbilityTaskNode"},
    {"name": "TetherPerfLibrary",           "scope": "functions",
        "functions": [
            "GetLumenDiagnostics", "GetNaniteStats",
            # M4-5 + M5/M6/M7/M8: every UFUNCTION inside the
            # `#if !UE_VERSION_OLDER_THAN(5, 7, 0)` block in
            # TetherPerfLibrary.cpp lines 3089-4437. Functions outside
            # that block (BeginAutoHitchCapture / EndAutoHitchCapture /
            # GetAutoHitchState / GetFrameTimePercentiles) compile on every
            # supported version and don't need stubs.
            "ParseTraceToSummary",
            "ParseAllocTraceToSummary",
            "ParseNetTraceToSummary",
            "ParseCookTraceToSummary",
            "GetTextureStreamingResidency",
            "GetRenderTargetMemory",
            "GetPerPassGpuTimings",
            "AnalyzeAllMaterials",
            "ComparePerfSnapshots",
            "BeginInsightsForTrace",
        ]},
]

# Class line: `class [TETHER_API] UFoo : public UBlueprintFunctionLibrary`.
UCLASS_RE = re.compile(r"\bclass\s+(?:\w+_API\s+)?(\w+)\s*:\s*public\s+UBlueprintFunctionLibrary\b")


# Shared declaration for consumers that need the stub surface as data.
#
# gen_manifest.py imports this to stamp `"min_engine": "5.7"` on every
# stub-gated function in tether_manifest.json, and tether_preflight.py uses
# those manifest entries to reject the calls when the discovered editor's
# engine version is older (a stub call there logs a warning and returns a
# default value — a silent failure agents can't see otherwise).
#
# A missing header or an empty resolved function list means no gating is
# reported for that library (falls back to "function works everywhere").
MIN_ENGINE_VERSION = "5.7"


def stub_function_map(targets: "list[dict] | None" = None,
                      public: "Path | None" = None) -> dict:
    """Resolve TARGETS into {library_name: {function_name, ...}}.

    "all" scope resolves to every UFUNCTION found in the header, so the map
    stays correct when functions are added/removed without editing TARGETS.
    Read-only (no files written); safe to import from other tools.
    """
    resolved: dict = {}
    public = public or PUBLIC
    for target in (targets if targets is not None else TARGETS):
        name = target["name"]
        h_path = public / f"{name}.h"
        if not h_path.is_file():
            continue
        _, funcs = parse_header(h_path)
        scope = target["scope"]
        if scope == "all":
            wanted = {f["name"] for f in funcs}
        elif scope == "function":
            wanted = {target["function"]}
        elif scope == "functions":
            wanted = set(target["functions"])
        else:
            continue
        # "function" scope that no longer matches anything (renamed/removed
        # UFUNCTION) means the gate no longer exists — report nothing rather
        # than phantom entries.
        if not wanted:
            continue
        resolved[name] = wanted
    return resolved

# UFUNCTION(...)\nstatic <ret> <name>(<params>); — one level of nested parens.
UFUNCTION_RE = re.compile(
    r"UFUNCTION\("
    r"(?:[^()]|\([^()]*\))*"
    r"\)\s*"
    r"static\s+"
    r"(?P<rt>[\w:*&\s<>,]+?)\s+"
    r"(?P<name>\w+)\s*"
    r"\("
    r"(?P<params>(?:[^()]|\([^()]*\))*)"
    r"\)\s*;",
    re.DOTALL,
)

# Gate scan: `#if !UE_VERSION_OLDER_THAN(5, 7, 0)` (whitespace tolerated).
GATE_OPEN_RE = re.compile(r"^#if\s+!\s*UE_VERSION_OLDER_THAN\s*\(\s*5\s*,\s*7\s*,\s*0\s*\)")
# Out-of-line member definition at column 0 (UE source style: return type on
# the same line, no leading whitespace — indented matches are local lambdas or
# qualified calls like `FDataTableEditorUtils::BroadcastPreChange`, not
# library member definitions).
GATED_DEF_RE = re.compile(r"^[\w:*&<>, \t]*?\bUTether(\w+)Library::(\w+)\s*\(")


def extract_gated_definitions(text: str) -> set:
    """Return {"TetherXxxLibrary"}-normalized {(library, function)} pairs for
    every out-of-line `UTetherXxxLibrary::Func(` member definition inside a
    top-level `#if !UE_VERSION_OLDER_THAN(5, 7, 0)` ... matching `#endif` block.

    Tracks preprocessor nesting depth so inner `#if/#else` pairs (e.g. the
    `#else` fallbacks inside TetherPerfLibrary.cpp's big gate) do not end the
    outer gate early, and only definitions whose closing `#endif` matches the
    gate's own depth count.
    """
    results: set = set()
    in_gate = False
    gate_depth = 0
    depth = 0
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("#if"):
            depth += 1
            if not in_gate and GATE_OPEN_RE.match(stripped):
                in_gate = True
                gate_depth = depth
        elif stripped.startswith("#endif"):
            if in_gate and depth == gate_depth:
                in_gate = False
            depth -= 1
        elif in_gate:
            m = GATED_DEF_RE.match(line)
            if m:
                results.add((f"Tether{m.group(1)}Library", m.group(2)))
    return results


def check_gate_scan() -> bool:
    """Compare TARGETS "function"/"functions" entries against the actual
    5.7-gate blocks in the main .cpp files. Returns True when they match.

    "all"-scope targets are skipped: their whole class body is inside one gate
    block, so the gate boundary IS the class and there is no second list to
    drift. Only named-function scopes carry a hand-maintained list to verify.
    """
    ok = True
    for target in TARGETS:
        scope = target["scope"]
        if scope not in ("function", "functions"):
            continue
        name = target["name"]
        cpp_path = PRIVATE / f"{name}.cpp"
        if not cpp_path.is_file():
            print(f"  GATE {name} — main .cpp missing: {cpp_path}")
            ok = False
            continue
        if scope == "function":
            declared = {(name, target["function"])}
        else:
            declared = {(name, f) for f in target["functions"]}
        actual = extract_gated_definitions(cpp_path.read_text(encoding="utf-8"))
        cpp_only = actual - declared
        targets_only = declared - actual
        if cpp_only or targets_only:
            ok = False
            print(f"  GATE MISMATCH {name}:")
            for _, fn in sorted(cpp_only):
                print(f"    in .cpp gate but NOT in TARGETS: {fn}")
            for _, fn in sorted(targets_only):
                print(f"    in TARGETS but NOT in .cpp gate: {fn}")
        else:
            print(f"  GATE ok   {name} — {len(declared)} gated function(s) match TARGETS")
    return ok


def split_params(s: str) -> list[str]:
    out, cur, depth = [], "", 0
    for c in s:
        if c in "(<":
            depth += 1
            cur += c
        elif c in ")>":
            depth -= 1
            cur += c
        elif c == "," and depth == 0:
            cur_s = cur.strip()
            if cur_s:
                out.append(cur_s)
            cur = ""
        else:
            cur += c
    cur_s = cur.strip()
    if cur_s:
        out.append(cur_s)
    return out


def strip_default(p: str) -> str:
    return p.split("=", 1)[0].strip() if "=" in p else p


def normalize_ws(s: str) -> str:
    return " ".join(s.split())


def parse_header(h_path: Path) -> tuple[str | None, list[dict]]:
    text = h_path.read_text(encoding="utf-8")
    cm = UCLASS_RE.search(text)
    if not cm:
        return None, []
    class_name = cm.group(1)
    funcs = []
    for m in UFUNCTION_RE.finditer(text, cm.end()):
        rt = normalize_ws(m.group("rt"))
        name = m.group("name").strip()
        params_raw = m.group("params")
        params_list = [strip_default(p) for p in split_params(params_raw)]
        params_clean = ", ".join(normalize_ws(p) for p in params_list)
        funcs.append({"return_type": rt, "name": name, "params": params_clean})
    return class_name, funcs


def stub_body(rt: str) -> str:
    if rt == "void":
        return ""
    return f"\treturn {rt}{{}};\n"


def render_stub(class_name: str, func: dict) -> str:
    rt = func["return_type"]
    name = func["name"]
    params = func["params"]
    log = (
        f'\tUE_LOG(LogTemp, Warning, '
        f'TEXT("{class_name}::{name} requires UE 5.7+ — call ignored on this engine version"));\n'
    )
    if class_name == "UTetherStateTreeLibrary" and name == "GetLastStateTreeError":
        body = '\treturn TEXT("StateTree authoring API requires Unreal Engine 5.7+");\n'
    elif class_name == "UTetherSmartObjectLibrary" and name == "GetLastSmartObjectError":
        body = '\treturn TEXT("Smart Object API requires Unreal Engine 5.7+");\n'
    elif class_name == "UTetherRigLibrary" and name == "GetLastRigError":
        body = '\treturn TEXT("Control Rig / IK Rig authoring API requires Unreal Engine 5.7+");\n'
    elif class_name == "UTetherNiagaraLibrary" and name == "GetLastNiagaraError":
        body = '\treturn TEXT("Niagara authoring API requires Unreal Engine 5.7+");\n'
    elif class_name == "UTetherMaterialLibrary" and name == "RefreshTextureResource":
        body = (
            '\tFTetherTextureRefreshResult Result;\n'
            '\tResult.Error = TEXT("Texture resource refresh requires Unreal Engine 5.7+");\n'
            '\treturn Result;\n'
        )
    else:
        body = stub_body(rt)
    return f"{rt} {class_name}::{name}({params})\n{{\n{log}{body}}}\n\n"


def render_file(header_stem: str, class_name: str, funcs: list[dict]) -> str:
    parts: list[str] = []
    parts.append("// Auto-generated by tools/gen_version_stubs.py — DO NOT EDIT MANUALLY.\n")
    parts.append("//\n")
    parts.append("// Provides 5.4 stub bodies for UFUNCTIONs whose real implementations are\n")
    parts.append("// gated to UE 5.7+ in the corresponding library .cpp. On 5.7+ this entire\n")
    parts.append("// file compiles to an empty TU. See docs/version-compatibility.md.\n")
    parts.append("\n")
    parts.append(f'#include "{header_stem}.h"\n')
    parts.append('#include "Misc/EngineVersionComparison.h"\n')
    parts.append("\n")
    parts.append("#if UE_VERSION_OLDER_THAN(5, 7, 0)\n")
    parts.append("\n")
    for f in funcs:
        parts.append(render_stub(class_name, f))
    parts.append("#endif // UE_VERSION_OLDER_THAN(5, 7, 0)\n")
    return "".join(parts)


def process_target(target: dict, dry_run: bool) -> tuple[bool, int]:
    name = target["name"]
    scope = target["scope"]
    h_path = PUBLIC / f"{name}.h"
    out_path = PRIVATE / f"{name}_Stubs.cpp"
    class_name, funcs = parse_header(h_path)
    if not class_name:
        print(f"  SKIP {name} — no UBlueprintFunctionLibrary class found in {h_path}")
        return False, 0
    if scope == "function":
        wanted = target["function"]
        funcs = [f for f in funcs if f["name"] == wanted]
        if not funcs:
            print(f"  SKIP {name} — wanted UFUNCTION '{wanted}' not found")
            return False, 0
    elif scope == "functions":
        wanted_set = set(target["functions"])
        funcs = [f for f in funcs if f["name"] in wanted_set]
        missing = wanted_set - {f["name"] for f in funcs}
        if missing:
            print(f"  WARN {name} — wanted UFUNCTIONs not found: {sorted(missing)}")
    if not funcs:
        print(f"  SKIP {name} — no UFUNCTIONs to stub")
        return False, 0
    new_text = render_file(name, class_name, funcs)
    if out_path.exists():
        old_text = out_path.read_text(encoding="utf-8")
        if old_text == new_text:
            print(f"  ok   {name}_Stubs.cpp ({len(funcs)} stubs, unchanged)")
            return False, len(funcs)
    if not dry_run:
        out_path.write_text(new_text, encoding="utf-8")
    rel = out_path.relative_to(REPO_ROOT)
    print(f"  {'WOULD WRITE' if dry_run else 'WROTE'} {rel}  ({len(funcs)} stubs, class={class_name})")
    return True, len(funcs)


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate _Stubs.cpp files for 5.7-gated libraries.")
    ap.add_argument("--check", action="store_true",
                    help="dry run; exit 1 if changes needed or TARGETS/gate drift detected")
    args = ap.parse_args()
    changed_any = False
    total = 0
    for t in TARGETS:
        changed, n = process_target(t, args.check)
        changed_any = changed_any or changed
        total += n
    print(f"\n{total} stubs across {len(TARGETS)} targets")
    if args.check and changed_any:
        print("(--check: changes would be needed)")
        return 1
    if args.check:
        print("gate scan: TARGETS vs .cpp #if !UE_VERSION_OLDER_THAN(5, 7, 0) blocks")
        if not check_gate_scan():
            print("(--check: TARGETS drift from .cpp gates — register or remove the "
                  "functions above so stubs and gates stay in lockstep)")
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
