# DPopCleaner 0.3.1 BETA — Stage 3

DPopCleaner 0.3.1 is a Windows x64 maintenance utility reconstructed as a maintainable C++/Win32 project.

## What changed in 0.3.1

- Real installed-app enumeration from Windows uninstall registry (HKLM/HKCU, 64/32-bit views).
- Applications page with name, version, publisher and install location.
- Uses the application's own registered uninstaller/MSI command instead of pretending to remove software.
- After uninstall, performs a conservative leftover scan in Program Files, ProgramData, Local/Roaming AppData and Start Menu shortcuts.
- Leftovers are shown to the user and moved to the Recycle Bin only after a second confirmation.
- Full Inno Setup installer with Add/Remove Programs registration, Start Menu shortcut, optional desktop shortcut and upgrade support.
- Update check runs automatically after startup.
- Update package is downloaded over HTTPS and checked with SHA-256. Signed packages are verified with Authenticode.
- Unsigned BETA packages require an explicit warning/confirmation. Once a code-signing certificate is configured in GitHub secrets, signed updates use the normal automatic path.
- GitHub Actions builds DPopCleaner.exe, DPopUpdater.exe and DPopCleaner_Setup_0.3.1_BETA.exe and publishes v0.3.1-beta.

## Important limitation of leftover cleanup

No generic uninstaller can know every file an arbitrary third-party application ever created unless that application/installer provides a complete installation log. DPopCleaner therefore deliberately uses a conservative scan to avoid deleting unrelated user data. MSI/vendor uninstallers remain the primary removal mechanism.

## Build

Windows + Visual Studio 2022 Build Tools / CMake, or simply use the included GitHub Actions workflow.
