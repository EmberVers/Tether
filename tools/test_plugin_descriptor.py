"""Contract tests for the Tether plugin descriptor."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


DESCRIPTOR_PATH = (
    Path(__file__).resolve().parents[1]
    / "Plugin"
    / "Tether"
    / "Tether.uplugin"
)


class PluginDescriptorTests(unittest.TestCase):
    def test_tether_module_is_excluded_from_commandlets(self):
        descriptor = json.loads(DESCRIPTOR_PATH.read_text(encoding="utf-8"))
        tether_modules = [
            module
            for module in descriptor["Modules"]
            if module.get("Name") == "Tether"
        ]

        self.assertEqual(len(tether_modules), 1)
        self.assertEqual(tether_modules[0].get("Type"), "EditorNoCommandlet")


if __name__ == "__main__":
    unittest.main()
