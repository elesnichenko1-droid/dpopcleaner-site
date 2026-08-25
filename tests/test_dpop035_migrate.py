from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "tools" / "dpop035_core.py"
CMAKE_OVERLAY = ROOT / "v035_overlay" / "CMakeLists.txt"
UI_SMOKE = ROOT / "tools" / "dpop033_ui_smoke.ps1"


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

    def test_version_overlay_may_inherit_core_and_update_from_prepared_baseline(self):
        m = load_core()
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            donor = base / "v034"
            target = base / "v035"
            (donor / "modules").mkdir(parents=True)
            target.mkdir()
            (donor / "modules" / "FullCore.cpp").write_text("modern", encoding="utf-8")
            copied = m.copy_modern_backend(donor, target)
            self.assertEqual(copied, ["modules"])
            self.assertEqual((target / "modules" / "FullCore.cpp").read_text(encoding="utf-8"), "modern")
            self.assertFalse((target / "core").exists())
            self.assertFalse((target / "update").exists())

    def test_modern_zapret_backend_is_linked_into_recovered_cmake(self):
        m = load_core()
        donor = """add_executable(DPopCleaner WIN32\n  src/ui/Controls.cpp\n  src/ui/pages/WorkspacePage.cpp\n  src/modules/FullCore.cpp\n  src/modules/ZapretManager.cpp\n)\nif(BUILD_TESTING)\nendif()\n"""
        transformed = m._transform_cmake_for_disk(donor)
        self.assertIn("src/modules/ZapretCenterModel.cpp", transformed)

    def test_settings_runtime_sources_are_compiled_and_executed_by_ctest(self):
        m = load_core()
        donor = """add_executable(DPopCleaner WIN32\n  src/ui/Shell.cpp\n  src/modules/FullCore.cpp\n)\nif(BUILD_TESTING)\nendif()\n"""
        transformed = m._transform_cmake_for_settings(donor)
        self.assertIn("src/modules/SettingsStore.cpp", transformed)
        self.assertIn("src/ui/settings/SettingsController.cpp", transformed)
        self.assertIn("add_executable(SettingsStoreTests", transformed)
        self.assertIn("tests/v035/SettingsStoreTests.cpp", transformed)
        self.assertIn("add_test(NAME SettingsStoreTests", transformed)
        self.assertIn("add_executable(SettingsControllerTests", transformed)
        self.assertIn("tests/v035/SettingsControllerTests.cpp", transformed)
        self.assertIn("add_test(NAME SettingsControllerTests", transformed)

        overlay = CMAKE_OVERLAY.read_text(encoding="utf-8")
        self.assertIn("src/ui/TrayIcon.cpp", overlay)
        self.assertIn("src/modules/SettingsStore.cpp", overlay)
        self.assertIn("src/ui/settings/SettingsController.cpp", overlay)

    def test_shared_ui_smoke_forces_exit_close_behavior_in_isolated_settings_root(self):
        smoke = UI_SMOKE.read_text(encoding="utf-8-sig")
        self.assertIn("DPOP_SETTINGS_ROOT", smoke)
        self.assertIn('"tray_enabled": false', smoke)
        self.assertIn('"close_behavior": 0', smoke)
        self.assertIn("Remove-Item Env:DPOP_SETTINGS_ROOT", smoke)

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
