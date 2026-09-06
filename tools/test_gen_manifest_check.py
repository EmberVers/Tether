"""Contract tests for the gen_manifest.py offline --check staleness gate.

--check regex-extracts every UFUNCTION declaration from the Tether*Library.h
headers (via gen_version_stubs.parse_header) and compares the snake_cased
name sets against tether_manifest.json. These tests pin the extraction and
name-conversion logic on constructed header/manifest samples so drift
detection regressions surface here instead of as silent CI green.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory


HERE = Path(__file__).resolve().parent


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {name} from {path}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


gm = _load("gen_manifest_check", HERE / "gen_manifest.py")


def _header_text(lib: str, names: list) -> str:
    decls = "\n".join(
        f"\tUFUNCTION(BlueprintCallable, Category = \"Tether\")\n"
        f"\tstatic bool {n}(const FString& S);\n" for n in names
    )
    class_body = lib[len("Tether"):]
    return (
        f"UCLASS()\nclass TETHER_API UTether{class_body} : public UBlueprintFunctionLibrary\n"
        f"{{\n\tGENERATED_BODY()\npublic:\n{decls}\n}};\n"
    )


class UeSnakeTests(unittest.TestCase):
    def test_basic_pascal_case(self):
        self.assertEqual(gm._ue_snake("SearchThings"), "search_things")
        self.assertEqual(gm._ue_snake("GetLumenDiagnostics"), "get_lumen_diagnostics")
        self.assertEqual(gm._ue_snake("ParseTraceToSummary"), "parse_trace_to_summary")

    def test_digit_capital_boundary_uses_ue_rule(self):
        # UE binds PlaySound2D as play_sound2_d, not the naive play_sound2d.
        self.assertEqual(gm._ue_snake("PlaySound2D"), "play_sound2_d")
        self.assertEqual(gm._ue_snake("SamplePointsPoissonDisk3D"),
                         "sample_points_poisson_disk3_d")

    def test_acronyms(self):
        self.assertEqual(gm._ue_snake("GetUMGWidgetInfo"), "get_umg_widget_info")
        self.assertEqual(gm._ue_snake("ParseNetTraceToSummary"), "parse_net_trace_to_summary")


class HeaderUfunctionSetTests(unittest.TestCase):
    def test_extracts_names_and_strips_u_prefix(self):
        with TemporaryDirectory() as td:
            hdr = Path(td) / "TetherFooLibrary.h"
            hdr.write_text(_header_text("TetherFooLibrary",
                                        ["SearchThings", "PlaySound2D",
                                         "SetNodeComment"]),
                           encoding="utf-8")
            got = gm._header_ufunction_sets(td)
            self.assertEqual(
                got,
                {"TetherFooLibrary": {"search_things", "play_sound2_d",
                                      "set_node_comment"}},
            )

    def test_multi_line_signature_and_params(self):
        text = (
            "UCLASS()\n"
            "class TETHER_API UTetherBarLibrary : public UBlueprintFunctionLibrary\n"
            "{\n\tGENERATED_BODY()\npublic:\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"Tether\")\n"
            "\tstatic TArray<FString> GetRowsFiltered(const FString& Query,\n"
            "\t\tint32 TopN = 10, bool bDescending = true);\n"
            "};\n"
        )
        with TemporaryDirectory() as td:
            (Path(td) / "TetherBarLibrary.h").write_text(text, encoding="utf-8")
            got = gm._header_ufunction_sets(td)
            self.assertEqual(got, {"TetherBarLibrary": {"get_rows_filtered"}})

    def test_header_without_class_is_skipped(self):
        with TemporaryDirectory() as td:
            (Path(td) / "TetherNoClassLibrary.h").write_text(
                "// no UBlueprintFunctionLibrary class here\n", encoding="utf-8")
            self.assertEqual(gm._header_ufunction_sets(td), {})

    def test_only_tether_library_headers_scanned(self):
        with TemporaryDirectory() as td:
            (Path(td) / "TetherFooLibrary.h").write_text(
                _header_text("TetherFooLibrary", ["One"]), encoding="utf-8")
            (Path(td) / "OtherLibrary.h").write_text(
                _header_text("Other", ["NotTether"]), encoding="utf-8")
            got = gm._header_ufunction_sets(td)
            self.assertEqual(got, {"TetherFooLibrary": {"one"}})

    def test_real_repo_headers_parse_cleanly(self):
        # Smoke test against the actual repo: 26 libraries, every class name
        # starts with UTether and ends with Library.
        got = gm._header_ufunction_sets()
        self.assertGreaterEqual(len(got), 20)
        for lib in got:
            self.assertTrue(lib.startswith("Tether") and lib.endswith("Library"), lib)


class CheckDriftTests(unittest.TestCase):
    """_check_drift() with injected header sets + a constructed manifest."""

    def _run(self, header_funcs: dict, manifest_funcs: dict) -> tuple[int, str]:
        """header_funcs: {lib: [C++ PascalCase names]}; manifest_funcs: {lib: set}."""
        with TemporaryDirectory() as td:
            manifest_path = Path(td) / "tether_manifest.json"
            manifest_path.write_text(json.dumps({
                "libraries": {
                    lib: {"functions": {fn: {"params": []} for fn in sorted(fns)}}
                    for lib, fns in manifest_funcs.items()
                }
            }), encoding="utf-8")
            header_sets = {
                lib: {gm._ue_snake(n) for n in names}
                for lib, names in header_funcs.items()
            }
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                code = gm._check_drift(str(manifest_path), header_sets)
            return code, buf.getvalue()

    def test_identical_sets_pass(self):
        code, out = self._run(
            {"TetherFooLibrary": ["SearchThings", "PlaySound2D"]},
            {"TetherFooLibrary": {"search_things", "play_sound2_d"}},
        )
        self.assertEqual(code, 0)
        self.assertIn("matches the headers", out)

    def test_missing_function_fails(self):
        # C++ gained SetNodeComment but the manifest was not regenerated.
        code, out = self._run(
            {"TetherFooLibrary": ["SearchThings", "SetNodeComment"]},
            {"TetherFooLibrary": {"search_things"}},
        )
        self.assertEqual(code, 1)
        self.assertIn("set_node_comment", out)
        self.assertIn("missing from manifest", out)

    def test_extra_manifest_function_fails(self):
        # C++ renamed/removed a function the manifest still lists.
        code, out = self._run(
            {"TetherFooLibrary": ["SearchThings"]},
            {"TetherFooLibrary": {"search_things", "gone_function"}},
        )
        self.assertEqual(code, 1)
        self.assertIn("gone_function", out)
        self.assertIn("manifest-only", out)

    def test_library_only_in_headers_fails(self):
        code, out = self._run(
            {"TetherFooLibrary": ["SearchThings"], "TetherBarLibrary": ["DoBar"]},
            {"TetherFooLibrary": {"search_things"}},
        )
        self.assertEqual(code, 1)
        self.assertIn("TetherBarLibrary", out)
        self.assertIn("headers but NOT in manifest", out)

    def test_library_only_in_manifest_fails(self):
        code, out = self._run(
            {"TetherFooLibrary": ["SearchThings"]},
            {"TetherFooLibrary": {"search_things"},
             "TetherGhostLibrary": {"boo"}},
        )
        self.assertEqual(code, 1)
        self.assertIn("TetherGhostLibrary", out)
        self.assertIn("manifest but NOT in headers", out)

    def test_snake_case_binding_mismatch_reported_as_drift(self):
        # The real-world PlaySound2D case: UE binds it as play_sound2_d while
        # a manifest generated under a different naming rule carries
        # play_sound2d — both sides show a one-item diff and the check fails.
        code, out = self._run(
            {"TetherFooLibrary": ["PlaySound2D"]},
            {"TetherFooLibrary": {"play_sound2d"}},
        )
        self.assertEqual(code, 1)
        self.assertIn("play_sound2_d", out)
        self.assertIn("play_sound2d", out)

    def test_missing_manifest_file_fails(self):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(io.StringIO()):
            code = gm._check_drift("Z:/definitely/not/here.json", {})
        self.assertEqual(code, 1)

    def test_empty_manifest_functions_entry_ok(self):
        # A library with an empty functions dict on both sides is not drift.
        code, out = self._run(
            {"TetherFooLibrary": []},
            {"TetherFooLibrary": set()},
        )
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
