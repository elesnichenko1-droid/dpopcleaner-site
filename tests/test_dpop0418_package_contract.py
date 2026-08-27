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
    icon_smoke = ROOT / "tools" / "dpop0418_icon_smoke.ps1"
    capture = ROOT / "tools" / "dpop0418_capture_screenshots.ps1"
    installer = ROOT / "release" / "DPopCleaner_0.4.18.iss"
    resource = ROOT / "v0418" / "resources" / "version.rc.in"
    resource_header = ROOT / "v0418" / "resources" / "resource.h"
    cmake = ROOT / "v0418" / "CMakeLists.txt"

    expected = [
        "DPopCleaner.exe", "DPopUpdater.exe", "Languages/", "Shell/", "Documentation/",
        "Modules/DPop.Common.dll", "Modules/DiskAnalyzer.exe", "Modules/RestoreCenter.exe",
        "Modules/ZapretScreenFix.exe", "ThirdParty/Zapret/", "Resources/",
    ]
    actual = [line.strip() for line in read(allowlist).splitlines() if line.strip()]
    require(actual == expected, "0.4.18 stage allowlist must be exact")

    stage_text = read(stage)
    for token in (
        "build0418/bin/Release/DPopCleaner.exe", "build0418/bin/Release/DPopUpdater.exe", "v0417/payload",
        "Modules/DiskAnalyzer.exe", "Modules/RestoreCenter.exe", "Modules/ZapretScreenFix.exe",
        "ThirdParty/Zapret", "THIRD_PARTY_NOTICES.txt", "0.4.18.2",
    ):
        require(token in stage_text, f"Stage script missing contract token: {token}")
    require("downloads/DPopCleaner_0.2.14_BETA.exe" not in stage_text,
            "0.4.18 must not stage the preserved 0.2.14 binary as its runtime core")

    iss = read(installer)
    for token in (
        '#define MyAppVersion "0.4.18"', "DPopCleaner_Setup_0.4.18", "DPopUpdater.exe",
        "Modules\\DiskAnalyzer.exe", "Modules\\RestoreCenter.exe", "Modules\\ZapretScreenFix.exe",
        "ThirdParty\\Zapret", "{#StageRoot}\\ThirdParty\\Zapret\\*", "ZapretBackup", "*-user.txt",
        "BackupZapretUserLists", "RestoreZapretUserLists", "CurStepChanged", "ssInstall", "ssPostInstall",
        "VersionInfoVersion=0.4.18.2", "VersionInfoProductVersion=0.4.18.2",
        "SetupIconFile=..\\dpopcleaner.ico", 'IconFilename: "{app}\\DPopCleaner.exe"',
    ):
        require(token in iss, f"Installer contract missing: {token}")
    require('Type: filesandordirs; Name: "{app}\\ThirdParty\\Zapret"' in iss,
            "Installer must replace the program-owned bundled Zapret tree after user-list backup")

    require(read(resource_header).strip().endswith("#define IDI_APP_ICON 101"), "Shared icon resource id must be stable")
    rc = read(resource)
    require('IDI_APP_ICON ICON "dpopcleaner.ico"' in rc, "Native executables must embed the DPopCleaner icon")
    require("FILEVERSION 0,4,18,2" in rc, "Native file version must be 0.4.18.2")
    require("PRODUCTVERSION 0,4,18,2" in rc, "Native product version must be 0.4.18.2")
    require('VALUE "ProductVersion", "0.4.18 rev.2\\0"' in rc,
            "Native ProductVersion text must identify 0.4.18 rev.2")

    cmake_text = read(cmake)
    for token in ("resources/version.rc.in", "generated/version.rc", "generated/dpopcleaner.ico", "generated/resource.h", "DPopCleaner DPopUpdater"):
        require(token in cmake_text, f"CMake icon/version contract missing: {token}")

    require("ExtractIconExW" in read(icon_smoke), "Icon smoke must inspect compiled PE icon resources")
    capture_text = read(capture)
    for token in ("PrintWindow", "1000", "1004", "1007", "dpopcleaner-0.4.18-overview.png", "dpopcleaner-0.4.18-zapret.png", "dpopcleaner-0.4.18-settings.png"):
        require(token in capture_text, f"Screenshot capture contract missing: {token}")

    smoke_text = read(smoke)
    for token in (
        "DPopCleaner_Setup_0.4.18.exe", "DPopUpdater.exe", "Modules\\DiskAnalyzer.exe", "Modules\\RestoreCenter.exe",
        "Modules\\ZapretScreenFix.exe", "ThirdParty\\Zapret\\LICENSE.txt", "ThirdParty\\Zapret\\service.bat",
        "ThirdParty\\Zapret\\general.bat", "ThirdParty\\Zapret\\bin\\winws.exe", "ThirdParty\\Zapret\\bin\\WinDivert.dll",
        "ThirdParty\\Zapret\\bin\\WinDivert64.sys", "ThirdParty\\Zapret\\.service\\version.txt",
        "Documentation\\THIRD_PARTY_NOTICES.txt", "list-general-user.txt", "ZapretBackup", "zapret_user_list_preserved",
        "0.4.18.2", "dpop0418_close_smoke.ps1", "dpop0418_icon_smoke.ps1",
    ):
        require(token in smoke_text, f"Install smoke missing verification: {token}")


if __name__ == "__main__":
    main()
