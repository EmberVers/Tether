#!/usr/bin/env python3
"""
Generate tether_manifest.json — single source of truth for AST preflight and
the kwargs-only wrapper module.

Two run modes (auto-detected by whether `import unreal` succeeds):

  CLI driver (outside UE):
      python tools/gen_manifest.py [--out PATH] [--tether PATH]
      Drives a running UE editor via tether.py to execute the in-UE half,
      captures the JSON output, and writes it to
      .claude/skills/tether/scripts/tether_manifest.json by default.

      python tools/gen_manifest.py --check
      Offline staleness gate: regex-extracts every UFUNCTION declaration from
      the Tether*Library.h headers and compares the (snake_cased) name sets
      against the manifest. No editor needed. Exits 1 on any drift — a C++
      function added/removed without regenerating the manifest shows up here
      before agents call a binding that no longer exists.

  In-UE reflection:
      tether.py exec-file tools/gen_manifest.py
      Walks every unreal.Tether*Library class plus every
      unreal.Tether* / unreal.ETether* enum, and prints the manifest as
      JSON to stdout.

Manifest schema:
    {
      "generated_at": "<ISO 8601 UTC>",
      "ue_version": "5.7.x",
      "libraries": {
        "TetherAssetLibrary": {
          "functions": {
            "search_assets": {
              "params": [
                {"name": "query", "type": "str", "default": null, "has_default": false},
                ...
              ],
              "returns": "tuple[list[SoftObjectPath], list[str]]",
              "doc": "Full-featured keyword search..."
            }
          }
        }
      },
      "enums": {
        "TetherAssetSearchScope": ["ALL_ASSETS", "PROJECT", "CUSTOM_PACKAGE_PATH"]
      }
    }
"""

import json
import keyword as _keyword
import os
import sys

try:
    import unreal  # noqa: F401
    _IN_UE = True
except ImportError:
    _IN_UE = False


def _apply_min_engine_stamps(manifest: dict) -> "tuple[dict, int]":
    """Stamp `"min_engine": "5.7"` on every stub-gated function entry.

    Shares the stub function list with tools/gen_version_stubs.py (single
    source of truth). On older engines those UFUNCTIONs resolve to _Stubs.cpp
    bodies that only log a warning and return a default value — the manifest
    stamp lets tether_preflight.py reject them deterministically instead.

    TARGETS/gen_version_stubs.py carry C++ UFUNCTION names (PascalCase);
    manifest keys are UE Python bindings (snake_case) — converted here.
    """
    import re as _re

    try:
        from gen_version_stubs import MIN_ENGINE_VERSION, stub_function_map
    except ImportError:
        # Standalone invocation from another cwd — retry relative to this file.
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        try:
            from gen_version_stubs import MIN_ENGINE_VERSION, stub_function_map
        except ImportError:
            print("WARN: gen_version_stubs.py not importable — min_engine stamps skipped",
                  file=sys.stderr)
            return manifest, 0

    def _to_snake(name: str) -> str:
        s = _re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
        s = _re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
        return s.lower()

    stubs = stub_function_map()
    stamped = 0
    for lib_name, fn_names in stubs.items():
        lib = manifest.get("libraries", {}).get(lib_name)
        if not lib:
            print(
                f"WARN: stub target {lib_name} not in manifest — check "
                "gen_version_stubs.py TARGETS",
                file=sys.stderr,
            )
            continue
        funcs = lib.get("functions", {})
        for fn_name in fn_names:
            py_name = _to_snake(fn_name)
            fn_meta = funcs.get(py_name)
            if fn_meta is None:
                # A stub target that isn't a UFUNCTION binding (renamed,
                # removed, or a non-UFUNCTION overload) — surface it so the
                # two generators don't silently drift apart.
                print(
                    f"WARN: stub target {lib_name}.{fn_name} "
                    f"(→ {py_name}) not in manifest",
                    file=sys.stderr,
                )
                continue
            fn_meta["min_engine"] = MIN_ENGINE_VERSION
            stamped += 1
    return manifest, stamped


# ── Offline staleness check (no editor needed) ──────────────────────────────

# UE's Python binding generator turns a trailing digit+capital in the C++
# name into `<digit>_<lower>` (PlaySound2D → play_sound2_d), while the naive
# snake_caser produces `<digit><lower>` (play_sound2d). Map the handful of
# Tether names that hit the rule; anything unmapped falls back to naive.
_UE_SNAKE_EXCEPTIONS = {
    "PlaySound2D": "play_sound2_d",
    "SamplePointsPoissonDisk2D": "sample_points_poisson_disk2_d",
    "SamplePointsPoissonDisk3D": "sample_points_poisson_disk3_d",
}


def _ue_snake(name: str) -> str:
    """C++ UFUNCTION name → expected UE Python binding name (snake_case).

    Handles the digit+capital boundary the same way UE's generator does
    (via the exceptions table above).
    """
    import re as _re
    if name in _UE_SNAKE_EXCEPTIONS:
        return _UE_SNAKE_EXCEPTIONS[name]
    s = _re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    s = _re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
    return s.lower()


def _header_ufunction_sets(public_dir: "str | None" = None) -> "dict[str, set[str]]":
    """Scan every Tether*Library.h and return {library: {snake_case fn}}.

    Reuses gen_version_stubs.parse_header so the UFUNCTION regex stays a
    single source of truth (nested-paren tolerating, class-anchored).
    Class name comes back as `UTetherFooLibrary`; manifest keys are
    `TetherFooLibrary`, so the leading `U` is stripped here.
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pathlib import Path as _Path
    from gen_version_stubs import parse_header  # noqa: PLC0415

    if public_dir is None:
        public_dir = (
            _Path(__file__).resolve().parent.parent
            / "Plugin" / "Tether" / "Source" / "Tether" / "Public"
        )
    out: dict = {}
    for h_path in sorted(_Path(public_dir).glob("Tether*Library.h")):
        class_name, funcs = parse_header(h_path)
        if not class_name or not class_name.startswith("U"):
            continue
        lib = class_name[1:]
        out[lib] = {_ue_snake(f["name"]) for f in funcs}
    return out


def _check_drift(
    manifest_path: "str | None" = None,
    header_sets: "dict[str, set[str]] | None" = None,
) -> int:
    """Offline manifest staleness gate. Returns process exit code.

    `header_sets` and `manifest_path` are injectable for tests; production
    resolves them from the repo layout.
    """
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    if manifest_path is None:
        manifest_path = os.path.join(
            repo, ".claude", "skills", "tether", "scripts", "tether_manifest.json"
        )
    if not os.path.isfile(manifest_path):
        print(f"ERROR: manifest not found at {manifest_path}", file=sys.stderr)
        return 1

    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)
    mf_libs = {
        lib: set(entry.get("functions", {}))
        for lib, entry in manifest.get("libraries", {}).items()
    }
    hdr_libs = _header_ufunction_sets() if header_sets is None else header_sets

    hdr_only_libs = sorted(set(hdr_libs) - set(mf_libs))
    mf_only_libs = sorted(set(mf_libs) - set(hdr_libs))

    fn_diffs = []
    for lib in sorted(set(hdr_libs) & set(mf_libs)):
        h, m = hdr_libs[lib], mf_libs[lib]
        if h != m:
            fn_diffs.append((lib, sorted(h - m), sorted(m - h)))

    total_missing = sum(len(a) for _, a, _ in fn_diffs)
    total_extra = sum(len(b) for _, _, b in fn_diffs)

    print(f"manifest: {len(mf_libs)} libraries, "
          f"{sum(len(f) for f in mf_libs.values())} functions")
    print(f"headers:  {len(hdr_libs)} libraries, "
          f"{sum(len(f) for f in hdr_libs.values())} UFUNCTION declarations")

    if hdr_only_libs:
        print(f"libraries in headers but NOT in manifest: {hdr_only_libs}")
    if mf_only_libs:
        print(f"libraries in manifest but NOT in headers: {mf_only_libs}")
    for lib, missing, extra in fn_diffs:
        if missing:
            print(f"{lib}: UFUNCTION(s) missing from manifest (C++ added, "
                  f"manifest not regenerated): {missing}")
        if extra:
            print(f"{lib}: manifest-only function(s) (C++ removed/renamed, "
                  f"manifest not regenerated): {extra}")

    if hdr_only_libs or mf_only_libs or total_missing or total_extra:
        print(
            "\n--check: manifest is STALE — run `python tools/gen_manifest.py` "
            "against a running editor to regenerate.",
            file=sys.stderr,
        )
        return 1
    print("--check: manifest UFUNCTION set matches the headers")
    return 0


# ── In-UE half: reflect the live Tether* surface ─────────────────────

def _build_manifest_in_ue() -> dict:
    """Walk unreal.Tether*Library classes + Tether* enums; return a manifest dict."""
    # All Tether*Library classes inherit from BlueprintFunctionLibrary →
    # UObject → _ObjectBase, which contributes ~50 generic helpers (cast,
    # get_class, call_method, get_editor_property, …). Those are NOT tether
    # functions; subtract them so the manifest only carries our UFUNCTIONs.
    inherited_names = _collect_inherited_method_names()

    libraries = {}
    for name in sorted(dir(unreal)):
        if not name.startswith("Tether") or not name.endswith("Library"):
            continue
        cls = getattr(unreal, name, None)
        if cls is None or not isinstance(cls, type):
            continue
        funcs = {}
        for fn_name in sorted(dir(cls)):
            if fn_name.startswith("_"):
                continue
            if fn_name in inherited_names:
                continue
            fn = getattr(cls, fn_name, None)
            if fn is None or not callable(fn):
                continue
            entry = _introspect_function(fn, fn_name)
            if entry is not None:
                funcs[fn_name] = entry
        if funcs:
            libraries[name] = {"functions": funcs}

    enums = {}
    for name in sorted(dir(unreal)):
        # UE Python strips the `E` prefix on enums but the user's reference docs
        # also use `TetherXxx` form — keep both names if both surface.
        if not (name.startswith("Tether") or name.startswith("ETether")):
            continue
        cls = getattr(unreal, name, None)
        if cls is None or not isinstance(cls, type):
            continue
        members = _enum_members(cls)
        if members:
            enums[name] = members

    structs = _collect_struct_fields(enums)

    return {
        "generated_at": _utc_now(),
        "ue_version": _ue_version_string(),
        "project_path": _project_path(),
        "libraries": libraries,
        "enums": enums,
        "structs": structs,
    }


# Inherited method set on every UE Python USTRUCT (FStructBase). Keep in sync if
# UE adds new wrapper methods — easy to spot: `dir(unreal.Vector)` and subtract
# the Vector-specific fields. None of these are tether-struct fields.
_USTRUCT_INHERITED_METHODS = {
    "assign", "cast", "copy", "export_text", "get_editor_property",
    "import_text", "set_editor_properties", "set_editor_property",
    "static_struct", "to_tuple",
}


def _collect_struct_fields(enums: dict) -> dict:
    """For every `unreal.Tether*` USTRUCT, list its field names by subtracting the
    inherited UE Python wrapper methods from `dir(cls)`. Used by preflight to
    catch attribute-confusion errors on tether struct returns (e.g. agent does
    `summary.parent_class_name` when the field is `parent_class_path`)."""
    structs = {}
    struct_base = getattr(unreal, "StructBase", None)
    if struct_base is None:
        return structs
    for name in sorted(dir(unreal)):
        if not name.startswith("Tether"):
            continue
        if name in enums:
            continue  # Tether enums share the prefix; skip them
        cls = getattr(unreal, name, None)
        if cls is None or not isinstance(cls, type):
            continue
        try:
            if not issubclass(cls, struct_base):
                continue
        except TypeError:
            continue
        # Field names = direct attributes minus inherited methods
        fields = sorted([
            a for a in dir(cls)
            if not a.startswith("_") and a not in _USTRUCT_INHERITED_METHODS
        ])
        if fields:
            structs[name] = fields
    return structs


def _project_path() -> str:
    """Absolute path to the loaded .uproject file (for wrapper mirroring)."""
    try:
        proj_dir = unreal.SystemLibrary.get_project_directory()
        proj_name = unreal.SystemLibrary.get_game_name()
        return f"{proj_dir.rstrip('/')}/{proj_name}.uproject"
    except Exception:
        return ""


def _collect_inherited_method_names() -> set:
    """Return the set of method names contributed by UE's generic Python
    base classes (BlueprintFunctionLibrary, Object, _ObjectBase, …).

    Strategy: take the dir() of BlueprintFunctionLibrary itself — every
    Tether*Library inherits from it. Names present on the bare base
    class are NOT tether functions and should not appear in the manifest.
    """
    base_names = set()
    base = getattr(unreal, "BlueprintFunctionLibrary", None)
    if base is not None and isinstance(base, type):
        base_names.update(n for n in dir(base) if not n.startswith("_"))
    # Also subtract anything on Object / _WrapperBase / _ObjectBase if reachable.
    for cand in ("Object",):
        c = getattr(unreal, cand, None)
        if c is not None and isinstance(c, type):
            base_names.update(n for n in dir(c) if not n.startswith("_"))
    return base_names


def _introspect_function(fn, name: str):
    """Extract param names + types + defaults + return type from a UE-bound callable.

    Strategy: try `inspect.signature` first (works for some bindings), fall
    back to parsing __doc__ which UE generates as
        `X.foo(arg1, arg2, ...) -> RetType -- summary`
    Returns None when the callable isn't shaped like a UFUNCTION binding (e.g.
    inherited Python builtins like __init_subclass__).
    """
    import inspect
    import re

    doc = (getattr(fn, "__doc__", "") or "").strip()
    summary = doc.split("\n", 1)[0] if doc else ""

    # --- Path 1: inspect.signature (rare for native UE bindings, but try) ---
    try:
        sig = inspect.signature(fn)
        params = []
        for pname, p in sig.parameters.items():
            if pname in ("self", "cls"):
                continue
            params.append({
                "name": pname,
                "type": _type_repr(p.annotation, p.empty),
                "default": _default_repr(p.default) if p.default is not p.empty else None,
                "has_default": p.default is not p.empty,
            })
        returns = _type_repr(sig.return_annotation, sig.empty)
        if params or returns or summary:
            return {"params": params, "returns": returns, "doc": summary}
    except (ValueError, TypeError):
        pass  # Fall through to docstring parser

    # --- Path 2: parse the UE-generated docstring ---
    if not doc:
        return None

    # Match a leading signature line. UE forms:
    #   X.foo(arg1, arg2=default) -> RetType -- summary
    #   foo(arg1) -> RetType -- summary
    first_line = doc.split("\n", 1)[0].strip()
    m = re.match(
        r"(?:[A-Za-z_]\w*\.)?(\w+)\s*\(([^)]*)\)\s*(?:->\s*([^-\n]+?))?\s*(?:--\s*(.*))?$",
        first_line,
    )
    if not m:
        return None
    _matched_name, arg_str, ret_str, summary_str = m.groups()
    ret_str = (ret_str or "").strip()
    summary_str = (summary_str or summary or "").strip()

    params = []
    for raw in _split_top_level(arg_str):
        raw = raw.strip()
        if not raw:
            continue
        if "=" in raw:
            n, d = raw.split("=", 1)
            params.append({
                "name": n.strip(),
                "type": "",
                "default": d.strip(),
                "has_default": True,
            })
        else:
            params.append({"name": raw, "type": "", "default": None, "has_default": False})

    return {"params": params, "returns": ret_str, "doc": summary_str}


def _split_top_level(s: str):
    """Split a comma-separated arg list, respecting balanced (), [], {}."""
    parts, buf, depth = [], [], 0
    for ch in s:
        if ch in "([{":
            depth += 1
            buf.append(ch)
        elif ch in ")]}":
            depth -= 1
            buf.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(ch)
    if buf:
        parts.append("".join(buf))
    return parts


def _enum_members(cls) -> list:
    """Heuristically detect enum members on a UE-bound enum class.

    UE Python convention: enum members are UPPER_CASE attributes whose value
    has an integer-like `.value` or is itself an int. Filter out non-members
    aggressively to avoid mistaking metaclass attrs for enum entries.
    """
    members = []
    for m in dir(cls):
        if m.startswith("_"):
            continue
        if not (m.isupper() or "_" in m and m.replace("_", "").isupper()):
            continue
        try:
            val = getattr(cls, m)
        except Exception:
            continue
        # Members are typically Enum instances or ints.
        if isinstance(val, int):
            members.append(m)
            continue
        if hasattr(val, "value"):
            try:
                int(val.value)
                members.append(m)
                continue
            except (TypeError, ValueError):
                pass
        # Exact-name comparison — Enum instance repr equals its name in UE Python
        if hasattr(val, "name") and getattr(val, "name", None) == m:
            members.append(m)
    return members


def _type_repr(annotation, empty_sentinel) -> str:
    """Stringify a parameter annotation; empty when unannotated."""
    if annotation is empty_sentinel:
        return ""
    try:
        if hasattr(annotation, "__name__"):
            return annotation.__name__
        return str(annotation)
    except Exception:
        return ""


def _default_repr(d) -> str:
    try:
        return repr(d)
    except Exception:
        return "<unrepr>"


def _utc_now() -> str:
    import datetime
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def _ue_version_string() -> str:
    try:
        return str(unreal.SystemLibrary.get_engine_version())
    except Exception:
        return "unknown"


# ── CLI driver: only fires when not running inside UE ──────────────────────

def _cli() -> int:
    import argparse
    import subprocess

    parser = argparse.ArgumentParser(
        description="Generate tether_manifest.json by introspecting a running UE editor."
    )
    parser.add_argument("--check", action="store_true",
                        help="offline staleness gate: compare header UFUNCTION sets "
                             "against the existing manifest (no editor needed)")
    parser.add_argument("--out", help="Output path (default: <repo>/.claude/skills/tether/scripts/tether_manifest.json)")
    parser.add_argument("--wrapper-out", help="Wrapper module output path (default: <repo>/Plugin/Tether/Content/Python/tether.py)")
    parser.add_argument("--no-wrapper", action="store_true", help="Skip generating the kwargs-only wrapper module")
    parser.add_argument("--tether", help="Path to tether.py (default: auto-detect relative to this script)")
    parser.add_argument("--timeout", type=int, default=60, help="Tether call timeout in seconds (default: 60)")
    args = parser.parse_args()

    if args.check:
        return _check_drift()

    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)  # tools/ → repo root
    tether = args.tether or os.path.join(
        repo, ".claude", "skills", "tether", "scripts", "tether.py"
    )
    out = args.out or os.path.join(
        repo, ".claude", "skills", "tether", "scripts", "tether_manifest.json"
    )

    if not os.path.isfile(tether):
        print(f"ERROR: tether.py not found at {tether}", file=sys.stderr)
        return 1

    cmd = [sys.executable, tether, "--json", "exec-file", os.path.abspath(__file__)]
    try:
        # Force UTF-8 + replace on decode errors. `text=True` alone defaults to
        # the active locale (GBK on zh-CN Windows), which dies on the UTF-8
        # bytes UE Python emits for non-ASCII docstrings or log lines.
        res = subprocess.run(cmd, capture_output=True, text=True,
                             encoding="utf-8", errors="replace",
                             timeout=args.timeout)
    except subprocess.TimeoutExpired:
        print(f"ERROR: tether call timed out after {args.timeout}s", file=sys.stderr)
        return 1

    if res.returncode != 0:
        print(f"ERROR: tether call failed (exit {res.returncode})", file=sys.stderr)
        if res.stderr:
            print(res.stderr, file=sys.stderr)
        if res.stdout:
            print(res.stdout, file=sys.stderr)
        return 1

    try:
        outer = json.loads(res.stdout)
    except json.JSONDecodeError:
        print(f"ERROR: tether returned non-JSON:\n{res.stdout[:500]}", file=sys.stderr)
        return 1

    if not outer.get("success"):
        print(f"ERROR: in-UE script failed:\n{outer.get('error', '?')}", file=sys.stderr)
        return 1

    manifest_text = (outer.get("output") or "").strip()
    if not manifest_text:
        print("ERROR: in-UE script printed no output", file=sys.stderr)
        return 1

    # The script may print log lines BEFORE the JSON. Take the last line that
    # parses as JSON (the manifest is a single-line dump).
    last_json = None
    for line in reversed(manifest_text.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            last_json = json.loads(line)
            break
        except json.JSONDecodeError:
            continue
    if last_json is None:
        print(f"ERROR: no JSON line in script output:\n{manifest_text[:500]}", file=sys.stderr)
        return 1

    # Stamp stub-gated functions with their minimum engine version (shared
    # list with gen_version_stubs.py) before writing either artifact.
    last_json, n_stamped = _apply_min_engine_stamps(last_json)

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(last_json, f, indent=2, ensure_ascii=False, sort_keys=True)
        f.write("\n")

    n_libs = len(last_json.get("libraries", {}))
    n_funcs = sum(len(L.get("functions", {})) for L in last_json.get("libraries", {}).values())
    n_enums = len(last_json.get("enums", {}))
    print(f"Wrote {out}")
    print(f"  {n_libs} libraries, {n_funcs} functions, {n_enums} enums")
    print(f"  {n_stamped} stub-gated functions stamped min_engine")
    print(f"  UE: {last_json.get('ue_version', '?')}, generated: {last_json.get('generated_at', '?')}")

    _validate_curated_configs(repo, last_json)

    if not args.no_wrapper:
        wrapper_out = args.wrapper_out or os.path.join(
            repo, "Plugin", "Tether", "Content", "Python", "tether.py"
        )
        wrapper_src, stats = _generate_wrapper(last_json)
        os.makedirs(os.path.dirname(wrapper_out), exist_ok=True)
        with open(wrapper_out, "w", encoding="utf-8") as f:
            f.write(wrapper_src)
        print(f"Wrote {wrapper_out}")
        print(f"  {stats['classes']} classes, {stats['methods']} methods, "
              f"{stats['skipped']} skipped (Python keyword in param name)")

        # UE auto-loads Python from the target project's Plugins/Tether/
        # Content/Python/, not the source repo. Mirror the wrapper there so a
        # plain `import tether` inside UE just works after regen.
        proj_uproject = (last_json.get("project_path") or "").strip()
        if proj_uproject:
            mirror = os.path.join(
                os.path.dirname(proj_uproject), "Plugins", "Tether",
                "Content", "Python", "tether.py",
            )
            try:
                os.makedirs(os.path.dirname(mirror), exist_ok=True)
                with open(mirror, "w", encoding="utf-8") as f:
                    f.write(wrapper_src)
                print(f"Mirrored to {mirror}")
            except OSError as e:
                print(f"WARN: could not mirror wrapper to project ({e})", file=sys.stderr)
        else:
            print("WARN: project_path missing from manifest — wrapper not mirrored to live editor",
                  file=sys.stderr)

    return 0


# ── Curated-config validation (offline; cross-checks hand-maintained JSON) ──

def _validate_curated_configs(repo: str, manifest: dict) -> None:
    """Warn about dead keys in the hand-curated JSON configs next to the manifest.

    tether_return_types.json function_returns keys and every
    `unreal.TetherXxxLibrary.fn(...)` call mentioned in a tether_redirects.json
    tether_replacement must resolve against the freshly generated manifest.
    Dead keys silently stop firing (they can never match a call site), so
    surface them at generation time instead. Best-effort: missing/unreadable
    config files are skipped silently.
    """
    import re

    libraries = manifest.get("libraries", {})
    scripts_dir = os.path.join(repo, ".claude", "skills", "tether", "scripts")

    def _load_json(name: str):
        path = os.path.join(scripts_dir, name)
        if not os.path.isfile(path):
            return None
        try:
            with open(path, encoding="utf-8") as f:
                return json.load(f)
        except (OSError, json.JSONDecodeError):
            return None

    return_types = _load_json("tether_return_types.json")
    if return_types:
        for key in return_types.get("function_returns", {}):
            if "." not in key:
                continue
            lib_name, fn_name = key.split(".", 1)
            if fn_name not in libraries.get(lib_name, {}).get("functions", {}):
                print(
                    f"WARN: tether_return_types.json key {key!r} does not exist "
                    "in the generated manifest (dead key — remove or fix it)",
                    file=sys.stderr,
                )

    redirects = _load_json("tether_redirects.json")
    if redirects:
        for entry in redirects.get("redirects", []):
            replacement = entry.get("tether_replacement") or ""
            for lib_name, fn_name in re.findall(
                r"(?:unreal\.)?(Tether\w*Library)\.(\w+)\s*\(", replacement
            ):
                if fn_name not in libraries.get(lib_name, {}).get("functions", {}):
                    print(
                        f"WARN: tether_redirects.json entry {entry.get('id')!r} "
                        f"references {lib_name}.{fn_name} which is not in the "
                        "generated manifest (dead redirect)",
                        file=sys.stderr,
                    )


# ── Wrapper module generation (offline; reads manifest, emits Python) ──────

def _generate_wrapper(manifest: dict) -> "tuple[str, dict]":
    """Emit the kwargs-only wrapper module source from the manifest.

    Each unreal.TetherXxxLibrary becomes a class `Xxx` whose
    @staticmethods mirror the tether functions with kwargs-only signatures.
    """
    out = []
    out.append('"""')
    out.append("Auto-generated kwargs-only wrapper for Tether*Library functions.")
    out.append("")
    out.append("Regenerate after C++ header changes:")
    out.append("    python tools/gen_manifest.py")
    out.append("")
    out.append("Usage from a script sent via the tether:")
    out.append("    from tether import Asset, Level")
    out.append("    paths, _ = Asset.search_assets_in_all_content(query='Hero', max_results=20)")
    out.append("    info = Level.get_actor_info(actor_path='/Persistent/Player')")
    out.append("")
    out.append("Why kwargs-only? Positional-arg-order is the #1 source of model")
    out.append("hallucinations against tether APIs — kwargs make the contract")
    out.append("structural rather than mnemonic.")
    out.append('"""')
    out.append("")
    out.append("import unreal")
    out.append("")
    out.append(f"_GENERATED_AT = {manifest.get('generated_at', '?')!r}")
    out.append(f"_UE_VERSION = {manifest.get('ue_version', '?')!r}")
    out.append("")

    n_classes, n_methods, n_skipped = 0, 0, 0

    for lib_name in sorted(manifest.get("libraries", {}).keys()):
        lib = manifest["libraries"][lib_name]
        short = _short_name(lib_name)  # TetherAssetLibrary → Asset
        out.append(f"class {short}:")
        out.append(f'    """Wraps unreal.{lib_name} (kwargs-only)."""')
        out.append("")
        n_classes += 1

        funcs = lib.get("functions", {})
        if not funcs:
            out.append("    pass")
            out.append("")
            continue

        for fn_name in sorted(funcs.keys()):
            fn = funcs[fn_name]
            params = fn.get("params", [])

            # Skip if any param name is a Python keyword — wrapping it would
            # produce invalid syntax (def foo(*, class=...) is a SyntaxError).
            bad = [p["name"] for p in params if _keyword.iskeyword(p["name"])]
            if bad:
                out.append(f"    # SKIPPED {fn_name}: param name(s) are Python keywords: {bad}")
                out.append(f"    # Call directly: unreal.{lib_name}.{fn_name}(...)")
                out.append("")
                n_skipped += 1
                continue

            sig_parts, call_parts = [], []
            for p in params:
                pname = p["name"]
                if p.get("has_default") and p.get("default") is not None:
                    sig_parts.append(f"{pname}={p['default']}")
                else:
                    sig_parts.append(pname)
                call_parts.append(pname)

            doc = (fn.get("doc") or "").replace('"""', "'''")
            # Auto-append known traps to wrapper docstrings. References go
            # under-read — these hints are the load-bearing channel: agents
            # see them via inspect.signature, IDE auto-complete, and direct
            # wrapper-module Read. Keep each hint terse (<200 chars) so the
            # signature line stays scannable.
            returns = fn.get("returns") or ""
            extra_notes = []

            # Return-type traps:
            if "SoftObjectPath" in returns:
                extra_notes.append(
                    "Note: SoftObjectPath does NOT stringify usefully — call "
                    ".export_text() for the '/Game/Foo.Foo' path (or .to_tuple()[0]). "
                    "See tether-asset-api.md.")

            # Function-name traps (matched by lib + name; a single tether-X
            # function shouldn't have more than one trap so this stays linear):
            qualname = f"{lib_name}.{fn_name}"
            if qualname == "TetherChooserLibrary.set_chooser_cell_raw":
                extra_notes.append(
                    "Trap: BoolColumn cells use bare enum text ('MatchTrue'/'MatchFalse'/"
                    "'MatchAny'), NOT a struct like '(Value=True)'. EnumColumn cells need "
                    "explicit '(Comparison=MatchAny)' for wildcards — default '()' compares "
                    "against int 0. See tether-chooser-api.md cell-format table.")
            elif fn_name.startswith("add_chooser_column"):
                extra_notes.append(
                    "If this is a freshly-created chooser (empty ContextData), call "
                    "set_chooser_context_object_class FIRST — otherwise the editor binding "
                    "widget shows 'NoPropertyBound' on every column. See tether-chooser-api.md "
                    "step 0.")
            elif qualname == "TetherAnimLibrary.get_anim_node_details":
                extra_notes.append(
                    "Index-based addressing is fragile + top-level AnimGraph only. For "
                    "state-machine interiors / transition rules / sub-graphs, use "
                    "get_anim_node_details_by_guid(abp_path, graph_name, node_guid).")

            if extra_notes:
                doc = doc.rstrip() + "  " + " ".join(extra_notes)

            out.append("    @staticmethod")
            if params:
                out.append(f"    def {fn_name}(*, {', '.join(sig_parts)}):")
            else:
                out.append(f"    def {fn_name}():")
            if doc:
                out.append(f'        """{doc}"""')
            out.append(f"        return unreal.{lib_name}.{fn_name}({', '.join(call_parts)})")
            out.append("")
            n_methods += 1

        out.append("")

    return "\n".join(out), {
        "classes": n_classes,
        "methods": n_methods,
        "skipped": n_skipped,
    }


def _short_name(lib_name: str) -> str:
    """TetherAssetLibrary → Asset; TetherUMGLibrary → UMG."""
    s = lib_name
    if s.startswith("Tether"):
        s = s[len("Tether"):]
    if s.endswith("Library"):
        s = s[: -len("Library")]
    return s or lib_name


# ── Entry point ────────────────────────────────────────────────────────────

# Runs only when executed as a script (CLI driver, or via tether.py exec-file
# inside UE, which also runs the file as __main__). Importing the module as a
# library (tests, other tools) stays side-effect free.
if __name__ == "__main__":
    if _IN_UE:
        print(json.dumps(_build_manifest_in_ue(), ensure_ascii=False))
    else:
        sys.exit(_cli())
