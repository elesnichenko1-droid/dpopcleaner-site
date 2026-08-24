from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "tools" / "dpop035_core.py"


def load_core():
    spec = importlib.util.spec_from_file_location("dpop035_core", CORE)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load dpop035_core")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MigrationBoundaryTests(unittest.TestCase):
    def test_035_identity_is_monotonic_and_exact(self):
        m = load_core()
        self.assertEqual(m.TARGET_VERSION, "0.3.5")
        self.assertEqual(m.TARGET_DISPLAY_VERSION, "0.3.5 BETA R1")
        self.assertEqual(m.TARGET_VERSION_CODE, "3051")
        self.assertEqual(m.TARGET_REVISION, "1")
        self.assertEqual(m.TARGET_RESOURCE_VERSION, "0.3.5.1")

    def test_backend_allowlist_cannot_replace_ui_host(self):
        m = load_core()
        self.assertEqual(set(m.MODERN_BACKEND_ROOTS), {"core", "modules", "update"})
        self.assertNotIn("ui", m.MODERN_BACKEND_ROOTS)
        self.assertNotIn("MainWindow.cpp", m.MODERN_BACKEND_ROOTS)

    def test_overlay_empty_is_safe(self):
        m = load_core()
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source = base / "overlay"
            target = base / "target"
            source.mkdir()
            target.mkdir()
            self.assertEqual(m.apply_overlay(source, target), [])

    def test_overlay_rejects_symlink(self):
        m = load_core()
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            source = base / "overlay"
            target = base / "target"
            source.mkdir()
            target.mkdir()
            real = source / "real.txt"
            real.write_text("x", encoding="utf-8")
            link = source / "link.txt"
            try:
                link.symlink_to(real)
            except OSError:
                self.skipTest("symlink creation unavailable on this runner")
            with self.assertRaisesRegex(ValueError, "symlink"):
                m.apply_overlay(source, target)

    def test_guarded_replace_rejects_donor_drift(self):
        m = load_core()
        with self.assertRaisesRegex(ValueError, "expected marker missing"):
            m.guarded_replace("abc", "missing", "x", "unit")


if __name__ == "__main__":
    unittest.main()
