from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'


def load_module():
    spec = importlib.util.spec_from_file_location('dpop034_migrate', MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


DONOR_CMAKE = r'''cmake_minimum_required(VERSION 3.24)
project(DPopCleaner VERSION 0.3.4 LANGUAGES CXX RC)
add_executable(DPopCleaner WIN32
  src/ui/Layout.cpp
  src/ui/Theme.cpp
  src/ui/pages/ZapretPage.cpp
  src/ui/pages/WorkspacePage.cpp
)
if(BUILD_TESTING)
  add_executable(LayoutTests tests/v034/LayoutTests.cpp src/ui/Layout.cpp)
  target_include_directories(LayoutTests PRIVATE src)
  add_test(NAME LayoutTests COMMAND LayoutTests)
endif()
'''

ZAPRET_CMAKE = r'''add_executable(DPopCleaner WIN32
  src/modules/DPopGuard.cpp
  src/modules/ZapretManager.cpp
  src/modules/ZapretPolicy.cpp
  src/modules/Applications.cpp
)
if(BUILD_TESTING)
  add_executable(ZapretPolicyTests tests/ZapretPolicyTests.cpp src/modules/ZapretPolicy.cpp)
  target_include_directories(ZapretPolicyTests PRIVATE src)
  add_test(NAME ZapretPolicyTests COMMAND ZapretPolicyTests)
endif()
'''


class CMakeIntegrationTests(unittest.TestCase):
    def test_page_layout_is_compiled_into_app_and_registered_in_ctest(self):
        mod = load_module()
        updated = mod.transform_cmake_for_page_layout(DONOR_CMAKE)
        self.assertIn('src/ui/PageLayout.cpp', updated)
        self.assertEqual(updated.count('src/ui/PageLayout.cpp'), 2)
        self.assertIn('add_executable(PageLayoutTests', updated)
        self.assertIn('tests/v034/PageLayoutTests.cpp', updated)
        self.assertIn('add_test(NAME PageLayoutTests COMMAND PageLayoutTests)', updated)

    def test_cmake_transform_is_idempotent(self):
        mod = load_module()
        once = mod.transform_cmake_for_page_layout(DONOR_CMAKE)
        twice = mod.transform_cmake_for_page_layout(once)
        self.assertEqual(once, twice)

    def test_zapret_center_model_is_compiled_into_app_and_ctest(self):
        mod = load_module()
        updated = mod.transform_cmake_for_zapret_center(ZAPRET_CMAKE)
        self.assertIn('src/modules/ZapretCenterModel.cpp', updated)
        self.assertEqual(updated.count('src/modules/ZapretCenterModel.cpp'), 2)
        self.assertIn('add_executable(ZapretCenterModelTests', updated)
        self.assertIn('tests/v034/ZapretCenterModelTests.cpp', updated)
        self.assertIn('add_test(NAME ZapretCenterModelTests COMMAND ZapretCenterModelTests)', updated)
        self.assertEqual(updated, mod.transform_cmake_for_zapret_center(updated))

    def test_zapret_page_layout_is_compiled_into_app_and_ctest(self):
        mod = load_module()
        updated = mod.transform_cmake_for_zapret_page_layout(DONOR_CMAKE)
        self.assertIn('src/ui/pages/ZapretPageLayout.cpp', updated)
        self.assertEqual(updated.count('src/ui/pages/ZapretPageLayout.cpp'), 2)
        self.assertIn('add_executable(ZapretPageLayoutTests', updated)
        self.assertIn('tests/v034/ZapretPageLayoutTests.cpp', updated)
        self.assertIn('add_test(NAME ZapretPageLayoutTests COMMAND ZapretPageLayoutTests)', updated)
        self.assertEqual(updated, mod.transform_cmake_for_zapret_page_layout(updated))


class PrepareScriptTests(unittest.TestCase):
    def test_prepare_034_script_copies_identity_ui_modules_and_tests(self):
        mod = load_module()
        script = mod._prepare_034_script_text()
        self.assertIn("scripts/Prepare-R3Source.ps1", script)
        self.assertIn("$resolvedV034", script)
        for marker in (
            "'CMakeLists.txt'",
            "'MainWindow.cpp'",
            "'Version.h'",
            "'version.rc.in'",
            "'ui'",
            "'modules'",
            "'tests'",
            "'tests/v034'",
        ):
            self.assertIn(marker, script)
        self.assertIn('Prepared DPopCleaner 0.3.4 source', script)


if __name__ == '__main__':
    unittest.main()
