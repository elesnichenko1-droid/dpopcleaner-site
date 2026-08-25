from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "index.html"
SCRIPT = ROOT / "script.js"
MANIFEST_JS = ROOT / "release-manifest.js"
PUBLISH = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.3.5.yml"
OLD_PUBLISH = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.3.4.yml"


class PublishContractTests(unittest.TestCase):
    def test_site_describes_035_not_034(self):
        index = INDEX.read_text(encoding="utf-8")
        for marker in (
            "DPopCleaner 0.3.5 BETA R1",
            "TreeSize",
            "settings.json",
            "3051 / rev 1",
            "assets/dpopcleaner-0.3.5-r1.png",
            "горизонталь",
        ):
            self.assertIn(marker, index)
        for stale in ("0.3.4 BETA R2", "Disk browser", "Sidebar держит"):
            self.assertNotIn(stale, index)

    def test_browser_manifest_accepts_only_035_r1(self):
        manifest = MANIFEST_JS.read_text(encoding="utf-8")
        for marker in (
            "v0\\.3\\.5-beta-r1",
            "DPopCleaner_Setup_0\\.3\\.5_BETA_R1\\.exe",
            "m.version==='0.3.5'",
            "Number(m.version_code)===3051",
            "Number(m.revision)===1",
        ):
            self.assertIn(marker, manifest)
        self.assertNotIn("0.3.4", manifest)

    def test_site_script_uses_035_fallback_labels(self):
        script = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("0.3.5 BETA R1", script)
        self.assertIn("0.3.5 R1", script)
        self.assertNotIn("0.3.4", script)

    def test_publish_workflow_builds_then_publishes_and_live_verifies(self):
        workflow = PUBLISH.read_text(encoding="utf-8")
        for marker in (
            "push:\n    branches: [main]",
            "pull_request:\n    branches: [main]",
            "contents: write",
            "pages: write",
            "RELEASE_TAG: v0.3.5-beta-r1",
            "RELEASE_ASSET: DPopCleaner_Setup_0.3.5_BETA_R1.exe",
            "python tools/dpop035_migrate.py",
            "dpop035_settings_visual_smoke.ps1",
            "dpop035_disk_smoke.ps1",
            "DPopCleaner_0.3.5_R1.iss",
            "gh release",
            "actions/deploy-pages@v5",
            "version_code = 3051",
            "revision = 1",
            "curl -fL",
            "sha256sum --check",
        ):
            self.assertIn(marker, workflow)
        self.assertIn("if: github.event_name == 'push'", workflow)

    def test_old_034_publisher_no_longer_runs_on_main_push(self):
        old = OLD_PUBLISH.read_text(encoding="utf-8")
        self.assertNotIn("\n  push:\n", old)
        self.assertIn("workflow_dispatch:", old)
        self.assertIn("pull_request:", old)


if __name__ == "__main__":
    unittest.main()
