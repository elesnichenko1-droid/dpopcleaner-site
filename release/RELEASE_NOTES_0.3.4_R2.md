# DPopCleaner 0.3.4 BETA R2

R2 is the functional-parity and safety pass on top of the independently verified 0.3.4 R1 release.

## Main changes

- Restored the compact 0.2.14-style sidebar UX with DPI-aware page geometry and no legacy `top=54` overlap pattern.
- Reworked Startup with application icons, classification, recommendations, safe enable/disable handling and protection for system/HKLM entries.
- Reworked DPopGuard around real Windows protection providers: DPopGuard heuristics, AMSI and Microsoft Defender where available.
- Reworked Disk into an interactive browser with navigation, icons, large-file analysis and protected-system-path warnings.
- Reworked Applications with icons, install path, uninstall entry, conservative leftovers discovery and WinGet update checks when available.
- Reworked Duplicates into explicit reference-file -> duplicate-copy groups, with protected reference/system/excluded paths and recycle-bin-only removal.
- Restored expanded Settings, real cleaning exclusions and non-destructive startup hooks.
- Added automatic DPopCleaner update check at startup with user choice; package verification remains HTTPS + size + SHA-256 + updater policy.
- Expanded Zapret Center to eight actions, including verified bundle update and `Исправление трансляций` for Discord/RTC recovery.
- Zapret update accepts only an official GitHub HTTPS release ZIP with release size and SHA-256 digest, validates the staged bundle and rolls back on install failure.
- RTC repair touches only the DPopCleaner bundled standalone `winws`, flushes DNS and reapplies the selected bundled strategy; it does not disable Defender or Windows Firewall.
- Reworked Windows maintenance layout and execution logging.

## Release identity

- Display version: `0.3.4 BETA R2`
- Version code: `3042`
- Revision: `2`
- Windows resource version: `0.3.4.2`
- Installer: `DPopCleaner_Setup_0.3.4_BETA_R2.exe`

R2 remains BETA. Destructive system actions continue to require explicit user action/confirmation where applicable.
