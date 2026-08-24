from __future__ import annotations

import importlib.util
import re
import shutil
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "dpop033_migrate.py"


def load_module():
    spec = importlib.util.spec_from_file_location("dpop033_migrate", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot create module spec")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RecoveryControlsTransformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.mkdtemp()
        self.root = Path(self.temp) / "v033"
        (self.root / "ui" / "pages").mkdir(parents=True)

        (self.root / "CMakeLists.txt").write_text(
            "project(DPopCleaner VERSION 0.3.2 LANGUAGES CXX RC)\n",
            encoding="utf-8",
        )
        (self.root / "Version.h").write_text(
            '#pragma once\nnamespace dpop::version {\n'
            'inline constexpr wchar_t kVersion[] = L"0.3.2";\n'
            'inline constexpr wchar_t kDisplayVersion[] = L"0.3.2 BETA R1";\n'
            'inline constexpr int kVersionCode = 3021;\n'
            'inline constexpr int kRevision = 1;\n}\n',
            encoding="utf-8",
        )
        (self.root / "version.rc.in").write_text(
            "FILEVERSION 0,3,2,1\n"
            "PRODUCTVERSION 0,3,2,1\n"
            'VALUE "FileVersion", "0.3.2.1\\0"\n'
            'VALUE "ProductVersion", "0.3.2 BETA R1\\0"\n',
            encoding="utf-8",
        )
        (self.root / "ui" / "RecoveryControls.h").write_text(
            "struct RecoveryFonts {\n"
            "    HFONT title = nullptr;\n"
            "    HFONT small = nullptr;\n"
            "};\n",
            encoding="utf-8",
        )
        (self.root / "ui" / "RecoveryControls.cpp").write_text(
            "bool RecoveryFonts::Create() noexcept {\n"
            "    small = CreateUiFont(9, FW_NORMAL);\n"
            "    if (!small) return false;\n"
            "    return true;\n"
            "}\n"
            "void RecoveryFonts::Destroy() noexcept {\n"
            "    for (HFONT* font : {&title, &small}) {\n"
            "        if (*font) DeleteObject(*font);\n"
            "    }\n"
            "}\n",
            encoding="utf-8",
        )
        (self.root / "ui" / "pages" / "ExamplePage.cpp").write_text(
            "void Draw(const RecoveryFonts& fonts) { UseFont(fonts.small); }\n",
            encoding="utf-8",
        )

    def tearDown(self):
        shutil.rmtree(self.temp)

    def test_transform_renames_recovered_small_font_identifier(self):
        m = load_module()
        m.transform_v033_overlay(self.root)

        for relative in (
            "ui/RecoveryControls.h",
            "ui/RecoveryControls.cpp",
            "ui/pages/ExamplePage.cpp",
        ):
            text = (self.root / relative).read_text(encoding="utf-8")
            self.assertIsNone(
                re.search(r"\bsmall\b", text),
                f"Windows SDK can macro-expand identifier 'small' in {relative}",
            )
            self.assertIn("smallFont", text)


if __name__ == "__main__":
    unittest.main()
