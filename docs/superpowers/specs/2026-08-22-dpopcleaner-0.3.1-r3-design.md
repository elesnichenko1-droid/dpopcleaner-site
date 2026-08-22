# DPopCleaner 0.3.1 BETA R3 Design

## Goal

Publish a testable Windows x64 release named `DPopCleaner 0.3.1 BETA R3` that contains the application, its native updater, and a pinned official Zapret distribution; requests administrator rights at application startup; carries the DPopCleaner icon throughout Windows; exits normally when the user closes the main window; and is represented honestly on the public website.

## Release identity

- Display version: `0.3.1 BETA R3`.
- Application version: `0.3.1`.
- Internal version code: `3013`.
- Revision: `3`.
- Git tag: `v0.3.1-beta-r3`.
- Installer asset: `DPopCleaner_Setup_0.3.1_BETA_R3.exe`.
- Existing `v0.3.1-beta` and `v0.2.14-clean-r1` releases remain untouched.
- The site remains fail-closed until the R3 artifact exists and its generated metadata passes validation.

## Confirmed problems in the existing 0.3.1 release

1. The released installer contains only `DPopCleaner.exe` and `DPopUpdater.exe`; it contains no Zapret files.
2. `ZapretManager` only detects a service/process and opens a detected folder. It cannot launch the bundled strategy or open the upstream service manager because no bundle is present.
3. The published `DPopCleaner.exe` contains the generic Windows executable icon. The release workflow overwrote `resources/app.rc` without an icon resource.
4. The application manifest requests `asInvoker`, so launching DPopCleaner does not request administrator rights.
5. `DPopUpdater` accepts `--restart` and unconditionally launches `DPopCleaner.exe` after a successful installation. A pending updater can therefore make an ordinary-looking close lead to an installation and a new application process.
6. The automatic update result handler can enter the interactive update flow even when the check was started in background mode.
7. The later Stage 3 R2 workflow never produced a replacement release because `SystemInfo.cpp` is missing the namespace-closing brace and fails with MSVC error `C1075`.
8. The website hero is an HTML/CSS illustration and explicitly is not a real application screenshot.

## Chosen approach

Create one complete, reproducible installer. GitHub Actions downloads the official Flowseal Zapret `1.10.1` ZIP at build time, validates its pinned SHA-256, stages the complete distribution, adds the upstream license, and includes that tree under `{app}\zapret`.

Pinned upstream inputs:

- Archive URL: `https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.1/zapret-discord-youtube-1.10.1.zip`.
- Archive SHA-256: `F748D61FEC75E4EDC992CB5B09D554E914197C68C690384ACEB61F143D8F76C9`.
- License URL: `https://raw.githubusercontent.com/Flowseal/zapret-discord-youtube/1.10.1/LICENSE.txt`.
- License SHA-256: `FE3983A1E91206AD1A530BCFAE01FAD207020CB61882EDD62C1E3CB5F8D5D430`.

No runtime code downloads or replaces Zapret silently. Upstream update checks remain visible in Zapret's own `service.bat`, with `utils\check_updates.enabled` included. DPopCleaner only launches the bundled tools after an explicit user action.

## Application lifecycle

### Administrator rights

`DPopCleaner.exe` embeds a Windows application manifest with `requestedExecutionLevel="requireAdministrator"`. The icon and manifest are linked through the resource file rather than generated ad hoc inside the release workflow. `DPopUpdater.exe` remains `asInvoker` and requests elevation only when it launches the installer through `runas`.

### Single instance

DPopCleaner creates a named mutex scoped to the current Windows session. A second launch locates and activates the existing main window, then exits. It does not create another background updater or application process.

### Closing

Closing the main window destroys the window, stops the startup update timer, ignores late background update results, posts `WM_QUIT`, and exits. Normal close never launches `DPopUpdater`, the installer, or DPopCleaner.

### Updating

1. A background check may fetch `update/beta.json` through native WinHTTP after startup.
2. Background mode only stores and displays update availability; it never shows an installation prompt, downloads a package, closes the application, or launches another process.
3. The user starts the installation flow from the Updates page.
4. The package is accepted only over HTTPS, after size and SHA-256 validation and, when declared signed, Authenticode validation.
5. An unsigned beta requires explicit confirmation after hash validation.
6. `DPopUpdater` installs the selected package after the main process exits.
7. R3 does not pass `--restart`; the updater never relaunches DPopCleaner automatically. The user starts the updated application from the shortcut.
8. The R3 manifest uses version code `3013`, so R3 never offers itself as an update.

The update request is compiled native C++/WinHTTP code. No Python `requests` package or PowerShell downloader is added to DPopCleaner. The installed `DPopUpdater.exe`, the public `update/beta.json`, and the Zapret `service.bat` update checker are the visible update components.

## Zapret integration

The installer places the verified upstream tree at `{app}\zapret`, including:

- `bin\winws.exe`, `WinDivert.dll`, `WinDivert64.sys`, `cygwin1.dll`, and upstream binary payloads;
- `lists\` and all included strategies named `general*.bat`;
- `service.bat`;
- `utils\check_updates.enabled`;
- `LICENSE.txt` copied from the pinned tag.

Zapret Center exposes four user actions:

1. **Refresh status** — queries the `zapret` service and bundled `winws.exe` processes.
2. **Run default strategy** — launches `{app}\zapret\general.bat` after explicit confirmation. DPopCleaner does not synthesize or hide command-line arguments.
3. **Open service manager** — launches `{app}\zapret\service.bat`, allowing the upstream menu to install, remove, diagnose, and update its service.
4. **Open Zapret folder** — opens the exact bundled folder.

Status detection accepts only a `winws.exe` located inside the bundled Zapret directory when reporting the bundled standalone strategy. It does not terminate unrelated VPN, GoodbyeDPI, Zapret, or WinDivert processes automatically.

## Icon and visual identity

- `dpopcleaner.ico` is compiled as resource `101` into DPopCleaner and DPopUpdater.
- The window class sets both `hIcon` and `hIconSm` from resource `101`.
- Inno Setup uses the same ICO for the installer and installed shortcuts.
- Build verification extracts the associated icon and rejects the generic Windows executable icon.
- The public site uses an actual screenshot captured from the final R3 binary. The current hand-built application mockup and its “not an exact screenshot” caption are removed.

## Installer

The Inno Setup installer requires administrator rights and contains exactly these top-level installed components:

- `DPopCleaner.exe`;
- `DPopUpdater.exe`;
- `zapret\` with the verified official distribution and license.

The installer creates Start Menu and optional desktop shortcuts. It does not install or start the Zapret service automatically. Service installation remains an explicit action in Zapret Center. Uninstall removes the application and bundled files but does not silently delete a running third-party service; the user is told to remove the Zapret service through its manager before uninstalling.

## Website and release metadata

- The hero, release card, FAQ, and download button display `0.3.1 BETA R3`.
- The features list names the bundled Zapret `1.10.1`, native update checking, administrator requirement, and manual service activation.
- `version.json` and `update/beta.json` use version code `3013` and revision `3`.
- The download URL must exactly match the R3 tag and asset name.
- The website enables the button only when `available=true`, the URL is exact, the SHA-256 is 64 hexadecimal characters, and the size is positive.
- If build, package validation, release upload, Defender scan, or live download verification fails, the public R3 manifest remains unavailable and the previous working download remains visible.

## Build and publication pipeline

The Windows workflow performs these gates in order:

1. Restore the tracked source tree without generating replacement C++ source in YAML.
2. Run policy and behavior tests.
3. Download and hash-check Zapret `1.10.1` and its license.
4. Configure and compile DPopCleaner and DPopUpdater with MSVC.
5. Run C++ tests.
6. Verify embedded application manifests, icons, file versions, and version code.
7. Stage the two application binaries and the verified Zapret tree.
8. Build the Inno Setup installer.
9. Optionally Authenticode-sign application binaries and installer when repository signing secrets exist.
10. Generate and validate the R3 release manifest from the final installer bytes.
11. Create or update prerelease `v0.3.1-beta-r3` and upload the installer plus build evidence.
12. Publish the R3 manifest and deploy the allowlisted Pages site.

## Tests and verification

Automated tests cover:

- background update checks cannot launch, download, close, or restart;
- an equal or lower version code is not offered;
- installer launch is possible only from the explicit interactive path;
- updater arguments omit automatic restart;
- the single-instance guard refuses a second process;
- Zapret path resolution stays under `{app}\zapret`;
- missing `service.bat`, `general.bat`, WinDivert, license, or update flag fails package validation;
- modified Zapret archive or license hashes fail the workflow;
- the installer definition includes only the two application executables and the verified Zapret tree;
- the R3 site manifest fails closed on wrong tag, asset, revision, size, or hash.

Release verification additionally checks:

- GitHub Actions build and Pages deployment succeed;
- the live site and JSON endpoints return HTTP 200;
- the downloaded installer size and SHA-256 match the live manifest;
- Defender scans the final downloaded installer and expanded installed payload with no detected threats;
- closing the final application process produces no new DPopCleaner, DPopUpdater, or installer process;
- the screenshot shown on the site comes from the same final binary.

## Security constraints

- No Defender disabling, exclusions, obfuscation, packers, or detection-evasion changes.
- No hidden PowerShell downloader in DPopCleaner.
- No automatic Zapret service installation or removal.
- No claims that an unsigned binary can never trigger SmartScreen or future antivirus heuristics.
- The release remains unsigned unless valid `WINDOWS_CERT_PFX_BASE64` and `WINDOWS_CERT_PASSWORD` secrets are configured.

## Acceptance criteria

The release is complete only when a user can download the R3 installer from the public site, install it with UAC, see the correct DPopCleaner icon, find the full `zapret` folder and update components, open Zapret Center and launch the bundled upstream tools explicitly, close DPopCleaner without any relaunch, and independently match the installer SHA-256 shown on the website.
