from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: Path) -> str:
    require(path.is_file(), f"Missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def main() -> None:
    allowlist = ROOT / "v0418" / "stage-allowlist.txt"
    stage = ROOT / "tools" / "dpop0418_stage.ps1"
    smoke = ROOT / "tools" / "dpop0418_install_smoke.ps1"
    installer = ROOT / "release" / "DPopCleaner_0.4.18.iss"
    resource = ROOT / "v0418" / "resources" / "version.rc.in"
    cmake = ROOT / "v0418" / "CMakeLists.txt"

    expected = [
        "DPopCleaner.exe",
        "DPopUpdater.exe",
        "Languages/",
        "Shell/",
        "Documentation/",
        "Modules/DPop.Common.dll",
        "Modules/DiskAnalyzer.exe",
        "Modules/RestoreCenter.exe",
        "Modules/ZapretScreenFix.exe",
        "Resources/",
    ]
    actual = [line.strip() for line in read(allowlist).splitlines() if line.strip()]
    require(actual == expected, "0.4.18 stage allowlist must be exact")

    stage_text = read(stage)
    for token in (
        "build0418/bin/Release/DPopCleaner.exe",
        "build0418/bin/Release/DPopUpdater.exe",
        "v0417/payload",
        "Modules/DiskAnalyzer.exe",
        "Modules/RestoreCenter.exe",
        "Modules/ZapretScreenFix.exe",
    ):
        require(token in stage_text, f"Stage script missing contract token: {token}")
    require("downloads/DPopCleaner_0.2.14_BETA.exe" not in stage_text,
            "0.4.18 must not stage the preserved 0.2.14 binary as its runtime core")

    iss = read(installer)
    for token in (
        '#define MyAppVersion "0.4.18"',
        "DPopCleaner_Setup_0.4.18",
        "DPopUpdater.exe",
        "Modules\\DiskAnalyzer.exe",
        "Modules\\RestoreCenter.exe",
        "Modules\\ZapretScreenFix.exe",
        "VersionInfoVersion=0.4.18.1",
    ):
        require(token in iss, f"Installer contract missing: {token}")

    rc = read(resource)
    require("FILEVERSION 0,4,18,1" in rc, "Native file version must be 0.4.18.1")
    require("PRODUCTVERSION 0,4,18,1" in rc, "Native product version must be 0.4.18.1")
    require('VALUE "ProductVersion", "0.4.18 rev.1\\0"' in rc,
            "Native ProductVersion text must identify 0.4.18 rev.1")

    cmake_text = read(cmake)
    require("resources/version.rc.in" in cmake_text,
            "CMake must generate native version resources")
    require("generated/version.rc" in cmake_text,
            "CMake must compile generated version resources")
    require("DPopCleaner DPopUpdater" in cmake_text,
            "Both native executables must receive the generated version resource")

    smoke_text = read(smoke)
    for token in (
        "DPopCleaner_Setup_0.4.18.exe",
        "DPopUpdater.exe",
        "Modules\\DiskAnalyzer.exe",
        "Modules\\RestoreCenter.exe",
        "Modules\\ZapretScreenFix.exe",
        "0.4.18.1",
        "dpop0418_close_smoke.ps1",
    ):
        require(token in smoke_text, f"Install smoke missing verification: {token}")


if __name__ == "__main__":
    main()
