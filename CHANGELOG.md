# Changelog

## 0.3.1 BETA — Stage 3

### Applications
- Added real installed application discovery from Windows registry.
- Added real vendor/MSI uninstaller launching.
- Added post-uninstall leftover scanning.
- Added conservative cleanup of app-named folders and Start Menu shortcuts.
- Cleanup moves candidates to Recycle Bin after explicit confirmation.

### Installer
- Full Inno Setup installer.
- Registers DPopCleaner in Windows Installed Apps.
- Supports upgrade-over-existing installation.
- Includes DPopUpdater.exe.
- Adds Start Menu shortcut and optional desktop shortcut.

### Updates
- Checks beta.json automatically shortly after startup.
- Downloads updates via HTTPS.
- Verifies SHA-256 before launch.
- Verifies Authenticode when release is marked signed.
- Restarts DPopCleaner after successful update installation.

### Security
- No runtime PowerShell downloader.
- No UPX/custom executable packer.
- No Defender bypass logic.
- Optional Authenticode signing hook in GitHub Actions.
