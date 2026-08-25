from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "release" / "DPopCleaner_0.3.5_R1.iss"
NOTES = ROOT / "release" / "RELEASE_NOTES_0.3.5_R1.md"
WORKFLOW = ROOT / ".github" / "workflows" / "DPopCleaner_0.3.5_PACKAGE_CANDIDATE.yml"


class ReleaseCandidateContractTests(unittest.TestCase):
    def test_installer_identity_is_exact(self):
        text = INSTALLER.read_text(encoding="utf-8")
        for marker in (
            '#define MyAppVersion "0.3.5 BETA R1"',
            "VersionInfoVersion=0.3.5.1",
            "VersionInfoProductVersion=0.3.5.1",
            "VersionInfoDescription=DPopCleaner 0.3.5 BETA R1 Setup",
            "OutputBaseFilename=DPopCleaner_Setup_0.3.5_BETA_R1",
            "UninstallDisplayName=DPopCleaner 0.3.5 BETA R1",
        ):
            self.assertIn(marker, text)
        self.assertNotIn("0.3.4", text)

    def test_release_notes_lock_035_identity_and_scope(self):
        text = NOTES.read_text(encoding="utf-8")
        for marker in (
            "DPopCleaner 0.3.5 BETA R1",
            "Version code: `3051`",
            "Revision: `1`",
            "Windows resource version: `0.3.5.1`",
            "DPopCleaner_Setup_0.3.5_BETA_R1.exe",
            "TreeSize",
            "settings.json",
        ):
            self.assertIn(marker, text)

    def test_package_candidate_builds_and_installs_without_publishing(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        for marker in (
            "permissions:\n  contents: read",
            "DPopCleaner_Setup_0.3.5_BETA_R1.exe",
            "python tools/dpop035_migrate.py",
            "DPopCleaner_0.3.5_R1.iss",
            "dpop035_settings_visual_smoke.ps1",
            "dpop035_disk_smoke.ps1",
            "Start-Process",
            "Get-FileHash",
            "release-proof.json",
            "dpopcleaner-0.3.5-r1-installer-candidate",
        ):
            self.assertIn(marker, text)
        for forbidden in (
            "contents: write",
            "pages: write",
            "gh release",
            "deploy-pages",
            "actions/deploy-pages",
        ):
            self.assertNotIn(forbidden, text)


if __name__ == "__main__":
    unittest.main()
