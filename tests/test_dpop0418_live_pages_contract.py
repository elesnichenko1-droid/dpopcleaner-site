from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.18.yml"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(WORKFLOW.is_file(), "0.4.18 publish workflow must exist")
    text = WORKFLOW.read_text(encoding="utf-8").lower()

    # deploy-pages can report success before every static object is globally visible.
    # The production gate must therefore retry each screenshot, cache-bust every
    # attempt, and prove exact bytes rather than accepting any non-small PNG.
    require("expectedshotsha" in text,
            "Live screenshot verifier must hash the candidate screenshot")
    require("liveshotsha" in text,
            "Live screenshot verifier must hash the downloaded screenshot")
    require("for ($shotattempt = 0; $shotattempt -lt 18; $shotattempt++)" in text,
            "Each live screenshot must receive a bounded propagation retry loop")
    require("'cache-control'='no-cache'" in text,
            "Live screenshot retries must bypass stale Pages/CDN cache entries")
    require("$env:github_run_id + '-shot-' + $shotattempt" in text,
            "Each live screenshot retry must use a unique cache-busting URL")
    require("$liveshotsha -eq $expectedshotsha" in text,
            "Live screenshot verification must require exact SHA-256 equality")
    require("start-sleep -seconds 10" in text,
            "Pages propagation retries must wait between attempts")


if __name__ == "__main__":
    main()
