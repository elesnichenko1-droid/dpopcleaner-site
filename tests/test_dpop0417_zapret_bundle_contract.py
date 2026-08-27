from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "downloads" / "DPopCleaner_0.2.14_BETA.exe"


def extract_core_zapret_strings():
    data = CORE.read_bytes()
    strings = []
    for raw in re.findall(rb"[\x20-\x7e]{4,}", data):
        strings.append(raw.decode("ascii", errors="ignore"))
    for raw in re.findall(rb"(?:[\x20-\x7e]\x00){4,}", data):
        strings.append(raw.decode("utf-16le", errors="ignore"))
    interesting = sorted({
        value for value in strings
        if any(token in value.lower() for token in (
            "zapret", "winws", "service.bat", "strategy", "strateg", "update", "updater"
        ))
    })
    return interesting


class DPop0417BundledZapretContractTests(unittest.TestCase):
    def test_frozen_core_zapret_expectations_are_visible_in_ci_log(self):
        values = extract_core_zapret_strings()
        print("FROZEN_CORE_ZAPRET_STRINGS_BEGIN")
        for value in values:
            print(repr(value))
        print("FROZEN_CORE_ZAPRET_STRINGS_END")
        self.assertTrue(values, "Frozen core should contain discoverable Zapret integration strings")

    def test_release_stages_real_zapret_runtime_not_only_screen_fix(self):
        allowlist = (ROOT / "v0417" / "stage-allowlist.txt").read_text(encoding="utf-8")
        stage = (ROOT / "tools" / "dpop0417_stage.ps1").read_text(encoding="utf-8")
        install_smoke = (ROOT / "tools" / "dpop0417_install_smoke.ps1").read_text(encoding="utf-8")
        publisher = (ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.17.yml").read_text(encoding="utf-8")

        self.assertIn("ZapretScreenFix.exe", allowlist)
        self.assertIn("ZapretScreenFix.exe", stage)
        self.assertIn("winws.exe", stage, "Stage must require the real Zapret runtime")
        self.assertIn("WinDivert64.sys", stage, "Stage must require the WinDivert driver")
        self.assertIn("service.bat", stage, "Stage must require Flowseal service management")
        self.assertIn("general.bat", stage, "Stage must require at least one real strategy")
        self.assertIn("winws.exe", install_smoke, "Fresh installed package must prove winws.exe exists")
        self.assertIn("WinDivert64.sys", install_smoke)
        self.assertIn("service.bat", install_smoke)
        self.assertIn("prepare_zapret", publisher.lower(), "Release workflow must prepare a pinned Zapret payload")


if __name__ == "__main__":
    unittest.main()
