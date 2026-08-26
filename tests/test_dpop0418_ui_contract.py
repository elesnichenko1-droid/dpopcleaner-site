from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "v0418" / "core" / "MainWindow.cpp"
CLIENT = ROOT / "v0418" / "core" / "UpdateClient.cpp"
VERSION = ROOT / "v0418" / "core" / "Version.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(MAIN.is_file(), "v0418/core/MainWindow.cpp must exist")
    text = MAIN.read_text(encoding="utf-8")
    client = CLIENT.read_text(encoding="utf-8")
    version = VERSION.read_text(encoding="utf-8")

    require("Автообновление: ВКЛ" in text, "Settings must expose enabled auto-update action")
    require("Автообновление: ВЫКЛ" in text, "Settings must expose disabled auto-update action")
    require("Проверить обновления сейчас" in text, "Settings must expose manual update check")
    require("settings.ini" in text, "Main app must persist settings.ini")
    require("SaveSettingsAtomic" in text and "LoadSettings" in text,
            "Settings page must use the isolated settings store")

    close_pos = text.find("case WM_CLOSE:")
    require(close_pos >= 0, "Main window must handle WM_CLOSE explicitly")
    close_block = text[close_pos:close_pos + 1800]
    hide_pos = close_block.find("ShowWindow(hwnd, SW_HIDE)")
    destroy_pos = close_block.find("DestroyWindow(hwnd)")
    require(hide_pos >= 0, "WM_CLOSE path must hide the window")
    require(destroy_pos >= 0, "WM_CLOSE path must destroy the window")
    require(hide_pos < destroy_pos, "Window must hide before DestroyWindow")
    require("gShuttingDown" in close_block, "WM_CLOSE must mark shutdown state")
    require(".join(" not in close_block, "WM_CLOSE must not join background workers")
    require(".join(" not in text, "MainWindow must not block shutdown on thread joins")

    require("PostMessageW" in text, "Background update work must notify UI asynchronously")
    require("gShuttingDown.load" in text, "Worker/result path must guard against shutdown")
    require("DPOP0418_TEST_SLOW_UPDATE_MS" in client,
            "Close smoke must use deterministic slow-update test hook")
    require("0.4.18" in version and "kVersionCode = 418" in version and "kRevision = 1" in version,
            "Native core identity must be 0.4.18 / 418 / revision 1")


if __name__ == "__main__":
    main()
