from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
RELEASE_BASE = "https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.4.18/"
SHOTS = (
    "dpopcleaner-0.4.18-overview.png",
    "dpopcleaner-0.4.18-zapret.png",
    "dpopcleaner-0.4.18-settings.png",
)


class ReleaseScreenshotAssetsTests(unittest.TestCase):
    def test_site_uses_release_assets_not_pages_asset_paths(self):
        index = (ROOT / "index.html").read_text(encoding="utf-8")
        for name in SHOTS:
            self.assertIn(RELEASE_BASE + name, index)
            self.assertNotIn(f'src="assets/{name}"', index)

    def test_publisher_uploads_and_verifies_exact_screenshot_release_assets(self):
        workflow = (ROOT / ".github/workflows/publish-dpopcleaner-0.4.18.yml").read_text(encoding="utf-8")
        lower = workflow.lower()
        self.assertIn("publish github release installer and screenshots", lower)
        self.assertIn("verify live release screenshots and installer sha256", lower)
        self.assertIn("get-filehash", lower)
        self.assertIn("release_tag: v0.4.18", lower)
        self.assertIn("gh release download", lower)
        for name in SHOTS:
            self.assertIn(f"publish/assets/{name}", workflow)
            self.assertIn(name, workflow)
        self.assertIn('--pattern "$name"', lower)
        self.assertNotIn('cp publish/assets/dpopcleaner-0.4.18-overview.png _site/assets/', lower)
        self.assertNotIn('$base/assets/$name', lower)


if __name__ == "__main__":
    unittest.main()
