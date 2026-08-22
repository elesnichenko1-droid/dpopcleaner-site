# DPopCleaner 0.2.15 BETA — reconstructed development base

Windows x64 C++/Win32 project reconstructed from observable behavior and strings of the supplied native binary. It is **not** the original source code.

## Update system

Manifest: `https://elesnichenko1-droid.github.io/dpopcleaner-site/update/beta.json`

The app checks the manifest shortly after startup and can also check manually. Downloads are HTTPS-only and SHA-256 verified. Unsigned packages are never auto-launched. Signed packages are additionally checked with Windows Authenticode before `DPopUpdater.exe` launches the installer.

## Build without local Visual Studio

Use `.github/workflows/build-dpopcleaner-release.yml` on GitHub Actions (`windows-2022`). The workflow builds both EXEs, creates an Inno Setup installer, uploads a prerelease and regenerates `update/beta.json` with the actual SHA-256 and size.

See `GITHUB_BUILD_RU.txt`.

## Important

The reconstructed modules are a safe development baseline, not a byte-for-byte restoration of the old application's internal algorithms.
