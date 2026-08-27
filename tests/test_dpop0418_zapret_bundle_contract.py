import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREPARE = ROOT / "tools" / "dpop0418_prepare_zapret.ps1"
STAGE = ROOT / "tools" / "dpop0418_stage.ps1"
ALLOWLIST = ROOT / "v0418" / "stage-allowlist.txt"
NOTICES = ROOT / "v0418" / "third_party" / "THIRD_PARTY_NOTICES.txt"
FLOWSEAL_META = ROOT / "v0418" / "third_party" / "flowseal-1.10.2"
FLOWSEAL_LICENSE = FLOWSEAL_META / "LICENSE.txt"
FLOWSEAL_VERSION = FLOWSEAL_META / "version.txt"
WORKFLOW = ROOT / ".github" / "workflows" / "DPopCleaner_0.4.18_FOUNDATION.yml"

VERSION = "1.10.2"
ASSET = "zapret-discord-youtube-1.10.2.zip"
URL = "https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.2/zapret-discord-youtube-1.10.2.zip"
SIZE = "1508077"
SHA256 = "5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5"
LICENSE_SHA256 = "fe3983a1e91206ad1a530bcfae01fad207020cb61882edd62c1e3cb5f8d5d430"
VERSION_SHA256 = "34d597db43ca53b2fd72ccbdd1af7a0fe238c2c0b8321dad8f43a1613143fc62"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: Path) -> str:
    require(path.is_file(), f"Missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    require(path.is_file(), f"Missing pinned upstream metadata: {path.relative_to(ROOT)}")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    prepare = read(PREPARE)
    for token in (
        VERSION,
        ASSET,
        URL,
        SIZE,
        SHA256,
        LICENSE_SHA256,
        VERSION_SHA256,
        "Get-FileHash",
        "Expand-Archive",
    ):
        require(token in prepare, f"Pinned Zapret prepare script missing token: {token}")

    require(sha256(FLOWSEAL_LICENSE) == LICENSE_SHA256,
            "Vendored Flowseal 1.10.2 LICENSE.txt must match the exact upstream bytes")
    require(sha256(FLOWSEAL_VERSION) == VERSION_SHA256,
            "Vendored Flowseal 1.10.2 version marker must match the exact upstream bytes")
    require(FLOWSEAL_VERSION.read_bytes() == b"1.10.2",
            "Vendored Flowseal version marker must be exactly 1.10.2 with no hidden suffix")

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
    require("flowseal-1.10.2" in prepare,
            "Prepare script must source exact vendored metadata for the pinned tag")

    normalized_prepare = prepare.replace("\\", "/")
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
        require(token in normalized_prepare,
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
