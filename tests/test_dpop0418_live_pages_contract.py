from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.18.yml"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(WORKFLOW.is_file(), "0.4.18 publish workflow must exist")
    text = WORKFLOW.read_text(encoding="utf-8").lower()

    # Pages remains responsible only for the stable manifest. The generated
    # screenshots are immutable release assets, uploaded together with the
    # installer and verified byte-for-byte from the v0.4.18 release URLs.
    require("for ($i = 0; $i -lt 18; $i++)" in text,
            "Live stable manifest must receive a bounded Pages propagation retry loop")
    require("$manifesturl + '?t=' + $env:github_run_id" in text,
            "Manifest verification must use a cache-busting live Pages URL")
    require("'cache-control'='no-cache'" in text,
            "Live verification must bypass stale cache entries")

    for name in (
        "dpopcleaner-0.4.18-overview.png",
        "dpopcleaner-0.4.18-zapret.png",
        "dpopcleaner-0.4.18-settings.png",
    ):
        require(f"releases/download/v0.4.18/{name}" in text,
                f"{name} must be verified from the v0.4.18 GitHub Release")

    require("expectedshotsha" in text,
            "Release screenshot verifier must hash the candidate screenshot")
    require("liveshotsha" in text,
            "Release screenshot verifier must hash the downloaded release screenshot")
    require("$liveshotsha -eq $expectedshotsha" in text,
            "Release screenshot verification must require exact SHA-256 equality")
    require("for ($shotattempt = 0; $shotattempt -lt 12; $shotattempt++)" in text,
            "Each release screenshot must receive a bounded availability retry loop")
    require("start-sleep -seconds 5" in text,
            "Release asset availability retries must wait between attempts")
    require("live release screenshot mismatch" in text,
            "Production verifier must fail closed when a release screenshot differs")

    require("$base/assets/" not in text,
            "Production must not verify generated screenshots through GitHub Pages /assets")
    require("cp publish/assets/dpopcleaner-0.4.18-overview.png _site/assets/" not in text,
            "Generated screenshots must not depend on Pages asset propagation")


if __name__ == "__main__":
    main()
