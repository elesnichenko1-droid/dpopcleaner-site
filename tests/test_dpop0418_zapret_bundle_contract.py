from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREPARE = ROOT / "tools" / "dpop0418_prepare_zapret.ps1"
STAGE = ROOT / "tools" / "dpop0418_stage.ps1"
ALLOWLIST = ROOT / "v0418" / "stage-allowlist.txt"
NOTICES = ROOT / "v0418" / "third_party" / "THIRD_PARTY_NOTICES.txt"
WORKFLOW = ROOT / ".github" / "workflows" / "DPopCleaner_0.4.18_FOUNDATION.yml"

VERSION = "1.10.2"
ASSET = "zapret-discord-youtube-1.10.2.zip"
URL = "https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.2/zapret-discord-youtube-1.10.2.zip"
SIZE = "1508077"
SHA256 = "5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: Path) -> str:
    require(path.is_file(), f"Missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def main() -> None:
    prepare = read(PREPARE)
    for token in (VERSION, ASSET, URL, SIZE, SHA256, "Get-FileHash", "Expand-Archive"):
        require(token in prepare, f"Pinned Zapret prepare script missing token: {token}")

    hash_pos = prepare.find("Get-FileHash")
    expand_pos = prepare.find("Expand-Archive")
    size_pos = prepare.find(SIZE)
    require(size_pos >= 0 and hash_pos >= 0 and expand_pos >= 0,
            "Prepare script must expose size/hash/extract gates")
    require(size_pos < expand_pos and hash_pos < expand_pos,
            "Zapret ZIP size and SHA-256 must be verified before extraction")
    require("releases/latest" not in prepare and "/latest/" not in prepare,
            "0.4.18 must never follow a latest Zapret asset")
    require("Invoke-WebRequest" in prepare,
            "Pinned archive must be fetched during the build, not at application runtime")

    for token in (
        ".service/version.txt",
        "LICENSE.txt",
        "service.bat",
        "general.bat",
        "bin/winws.exe",
        "bin/WinDivert.dll",
        "bin/WinDivert64.sys",
        "lists",
        "discord.media",
        "--filter-tcp=",
        "443",
    ):
        require(token in prepare.replace("\\", "/"),
                f"Prepare script missing payload/patch verification token: {token}")

    allowlist = [line.strip() for line in read(ALLOWLIST).splitlines() if line.strip()]
    require("ThirdParty/Zapret/" in allowlist,
            "0.4.18 exact stage allowlist must require ThirdParty/Zapret/")

    stage = read(STAGE).replace("\\", "/")
    require("ThirdParty/Zapret" in stage,
            "Stage script must copy the verified bundled Zapret tree")
    require("THIRD_PARTY_NOTICES.txt" in stage,
            "Stage script must include third-party notices")

    notices = read(NOTICES)
    for token in ("Flowseal", "bol-van", "WinDivert", VERSION):
        require(token in notices, f"Third-party notices missing attribution: {token}")

    workflow = read(WORKFLOW)
    require("dpop0418_prepare_zapret.ps1" in workflow,
            "Windows candidate must rebuild the pinned Zapret payload")
    require("Verify bundled Zapret supply-chain contract" in workflow,
            "Windows candidate must run the static Zapret supply-chain contract")
    require("Prepare pinned Zapret 1.10.2 payload" in workflow,
            "Windows candidate must run the pinned payload preparation step")


if __name__ == "__main__":
    main()
