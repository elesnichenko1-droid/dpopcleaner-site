from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.18.yml"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(WORKFLOW.is_file(), "0.4.18 publish workflow must exist")
    text = WORKFLOW.read_text(encoding="utf-8").lower()

    # A superseding main push must be able to stop a stuck publisher instead of
    # waiting behind it indefinitely.
    require("cancel-in-progress: true" in text,
            "Production concurrency must cancel an obsolete stuck publisher")
    require("timeout-minutes: 10" in text,
            "Publish job must have a hard upper time bound")

    # Pages remains responsible only for the live stable manifest. Every network
    # request to Pages must have an explicit timeout even inside a retry loop.
    require("for ($i = 0; $i -lt 18; $i++)" in text,
            "Live stable manifest must receive a bounded propagation retry loop")
    require("$manifesturl + '?t=' + $env:github_run_id" in text,
            "Manifest verification must use a cache-busting live Pages URL")
    require("'cache-control'='no-cache'" in text,
            "Live verification must bypass stale cache entries")
    require("invoke-restmethod" in text and "-timeoutsec 15" in text,
            "Pages manifest requests must have an explicit short timeout")

    # Generated screenshots are immutable GitHub Release assets. Verification
    # should use GitHub's release API/download path, not an unbounded web request
    # against each CDN object.
    require("gh release download" in text,
            "Published release assets must be downloaded through GitHub CLI")
    require("--pattern \"$name\"" in text,
            "Each screenshot release asset must be selected by exact name")
    require("--pattern \"$env:release_asset\"" in text,
            "Installer release asset must be selected by exact name")
    require("expectedshotsha" in text,
            "Release screenshot verifier must hash the candidate screenshot")
    require("liveshotsha" in text,
            "Release screenshot verifier must hash the downloaded release screenshot")
    require("$liveshotsha -eq $expectedshotsha" in text,
            "Release screenshot verification must require exact SHA-256 equality")
    require("live release screenshot mismatch" in text,
            "Production verifier must fail closed when a release screenshot differs")

    for name in (
        "dpopcleaner-0.4.18-overview.png",
        "dpopcleaner-0.4.18-zapret.png",
        "dpopcleaner-0.4.18-settings.png",
    ):
        require(name in text, f"Workflow must know release screenshot {name}")

    require("$base/assets/" not in text,
            "Production must not verify generated screenshots through GitHub Pages /assets")
    require("cp publish/assets/dpopcleaner-0.4.18-overview.png _site/assets/" not in text,
            "Generated screenshots must not depend on Pages asset propagation")


if __name__ == "__main__":
    main()
