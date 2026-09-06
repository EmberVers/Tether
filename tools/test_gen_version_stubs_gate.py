"""Contract tests for the gen_version_stubs.py --check gate scan.

The gate scan is the machine check that the hand-maintained TARGETS table (a
second source of truth for which UFUNCTIONs are 5.7-gated) stays in lockstep
with the `#if !UE_VERSION_OLDER_THAN(5, 7, 0)` blocks that actually wrap
those definitions in the main library .cpp files. These tests pin the
extraction logic on constructed cpp text so regressions in the depth-tracking
or definition regex surface here instead of as a silent CI green.
"""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parent / "gen_version_stubs.py"
SPEC = importlib.util.spec_from_file_location("gen_version_stubs", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Cannot load gen_version_stubs from {MODULE_PATH}")
gvs = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = gvs
SPEC.loader.exec_module(gvs)


CPP = """
#include "Foo.h"

void Helper() {}

#if !UE_VERSION_OLDER_THAN(5, 4, 0)
// unrelated older-version gate — must be ignored
FString UTetherFooLibrary::OldCompat(const FString& S) { return S; }
#endif

FString UTetherFooLibrary::PlainFunc(const FString& S) { return S; }

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
FString UTetherFooLibrary::GatedOne(
	const FString& A, const FString& B)
{
	return A + B;
}

TArray<FString> UTetherFooLibrary::GatedTwo(const FString& S)
{
	TArray<FString> Out;
	// indented qualified call — must NOT count as a member definition
	FDataTableEditorUtils::BroadcastPreChange(nullptr, {});
	// 5.4/5.7 inner fallback pair — must not terminate the outer gate
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
	const int32 V = 1;
#else
	const int32 V = 0;
#endif
	Out.Add(S);
	if (V) { Out.Add(S); }
	return Out;
}
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

bool UTetherFooLibrary::AfterGate(const FString& S) { return true; }

void Outer()
{
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
	// definitions inside a non-member function's gate — indentation still
	// rules them out; keep the depth tracker honest across a nested #if
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
	int Inner = 0;
#endif
#endif
}
"""


class ExtractGatedDefinitionsTests(unittest.TestCase):
    def test_extracts_only_gated_column0_definitions(self):
        got = gvs.extract_gated_definitions(CPP)
        self.assertEqual(
            got,
            {("TetherFooLibrary", "GatedOne"), ("TetherFooLibrary", "GatedTwo")},
        )

    def test_ignores_functions_outside_gate(self):
        got = gvs.extract_gated_definitions(CPP)
        names = {fn for _, fn in got}
        self.assertNotIn("PlainFunc", names)
        self.assertNotIn("OldCompat", names)
        self.assertNotIn("AfterGate", names)

    def test_indented_qualified_calls_not_counted(self):
        got = gvs.extract_gated_definitions(CPP)
        names = {fn for _, fn in got}
        self.assertNotIn("BroadcastPreChange", names)

    def test_nested_if_else_does_not_end_outer_gate(self):
        # GatedTwo appears AFTER the inner #if/#else/#endif pair; if the depth
        # tracker mistakenly closed the gate at the inner #endif, GatedTwo
        # would be missing.
        got = gvs.extract_gated_definitions(CPP)
        self.assertIn(("TetherFooLibrary", "GatedTwo"), got)

    def test_empty_text(self):
        self.assertEqual(gvs.extract_gated_definitions(""), set())

    def test_gate_with_odd_spacing(self):
        # Gate macro with a space after the opening paren (UE allows it; none
        # of the current repo files use it, but the scan tolerates it).
        text = (
            "#if ! UE_VERSION_OLDER_THAN(5, 7, 0)\n"
            "int32 UTetherBarLibrary::Spaced(int32 X) { return X; }\n"
            "#endif\n"
        )
        self.assertEqual(
            gvs.extract_gated_definitions(text),
            {("TetherBarLibrary", "Spaced")},
        )

    def test_repeated_gate_blocks_all_collected(self):
        text = (
            "#if !UE_VERSION_OLDER_THAN(5, 7, 0)\n"
            "int32 UTetherBarLibrary::First() { return 1; }\n"
            "#endif\n"
            "int32 UTetherBarLibrary::Never() { return 0; }\n"
            "#if !UE_VERSION_OLDER_THAN(5, 7, 0)\n"
            "int32 UTetherBarLibrary::Second() { return 2; }\n"
            "#endif\n"
        )
        self.assertEqual(
            gvs.extract_gated_definitions(text),
            {("TetherBarLibrary", "First"), ("TetherBarLibrary", "Second")},
        )


class CheckGateScanTests(unittest.TestCase):
    """check_gate_scan() against monkeypatched TARGETS/PRIVATE state."""

    def _run_with(self, tmp_cpp_text: str, targets: list) -> tuple[bool, str]:
        import contextlib
        import io
        from pathlib import Path
        from tempfile import TemporaryDirectory

        with TemporaryDirectory() as td:
            cpp = Path(td) / "TetherFooLibrary.cpp"
            cpp.write_text(tmp_cpp_text, encoding="utf-8")
            real_targets, real_private = gvs.TARGETS, gvs.PRIVATE
            gvs.TARGETS = targets
            gvs.PRIVATE = Path(td)
            try:
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    ok = gvs.check_gate_scan()
                return ok, buf.getvalue()
            finally:
                gvs.TARGETS, gvs.PRIVATE = real_targets, real_private

    def test_matching_targets_pass(self):
        targets = [{"name": "TetherFooLibrary", "scope": "functions",
                    "functions": ["GatedOne", "GatedTwo"]}]
        ok, out = self._run_with(CPP, targets)
        self.assertTrue(ok)
        self.assertIn("GATE ok", out)

    def test_missing_registration_fails(self):
        # GatedTwo was added to the .cpp but never registered in TARGETS —
        # exactly the drift this gate exists to catch (5.4 linker error).
        targets = [{"name": "TetherFooLibrary", "scope": "functions",
                    "functions": ["GatedOne"]}]
        ok, out = self._run_with(CPP, targets)
        self.assertFalse(ok)
        self.assertIn("GatedTwo", out)
        self.assertIn("NOT in TARGETS", out)

    def test_stale_registration_fails(self):
        # TARGETS still lists a function whose gate was removed/renamed.
        targets = [{"name": "TetherFooLibrary", "scope": "functions",
                    "functions": ["GatedOne", "GatedTwo", "Gone"]}]
        ok, out = self._run_with(CPP, targets)
        self.assertFalse(ok)
        self.assertIn("Gone", out)

    def test_single_function_scope_with_extra_cpp_gate_def_fails(self):
        # "function" scope stubs exactly one function, but the scan is still
        # bidirectional: a second definition inside the same .cpp gate that
        # was never registered would break 5.4 linking just the same.
        targets = [{"name": "TetherFooLibrary", "scope": "function",
                    "function": "GatedOne"}]
        ok, out = self._run_with(CPP, targets)
        self.assertFalse(ok)
        self.assertIn("GatedTwo", out)

    def test_single_function_scope_exact_match_passes(self):
        cpp = (
            "#if !UE_VERSION_OLDER_THAN(5, 7, 0)\n"
            "FString UTetherFooLibrary::GatedOne(const FString& S)\n"
            "{\n\treturn S;\n}\n"
            "#endif\n"
        )
        targets = [{"name": "TetherFooLibrary", "scope": "function",
                    "function": "GatedOne"}]
        ok, out = self._run_with(cpp, targets)
        self.assertTrue(ok)

    def test_all_scope_not_scanned(self):
        # "all" scope has no hand-maintained list — nothing to verify.
        targets = [{"name": "TetherFooLibrary", "scope": "all"}]
        ok, out = self._run_with(CPP, targets)
        self.assertTrue(ok)
        self.assertNotIn("GATE", out.replace("gate", ""))

    def test_missing_cpp_fails(self):
        targets = [{"name": "TetherNopeLibrary", "scope": "function",
                    "function": "Anything"}]
        ok, out = self._run_with(CPP, targets)
        self.assertFalse(ok)
        self.assertIn("missing", out)


if __name__ == "__main__":
    unittest.main()
