# DPopCleaner 0.4.17 Site and Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Package the preserved original 0.2.14 core plus verified 0.4.17 companion modules into `DPopCleaner_Setup_0.4.17.exe`, publish a clean website around the real application, and verify the live installer hash.

**Architecture:** A dedicated 0.4.17 Inno Setup definition consumes only the staged allowlisted payload. A Windows release workflow performs contracts, .NET tests, module smokes, silent installation, installed smokes, screenshot capture, installer SHA generation, GitHub Release publication, Pages deploy, and live download verification. Old 0.3.x publishers are manual-only before merge so they cannot overwrite the site.

**Tech Stack:** Inno Setup 6, GitHub Actions Windows 2022 + Ubuntu Pages deploy, HTML/CSS/vanilla JS website, PowerShell smoke, Python contract tests.

**Spec:** `docs/superpowers/specs/2026-08-25-dpopcleaner-0.4.17-original-0214-design.md`

## Global Constraints

- Release identity is exactly `DPopCleaner 0.4.17`.
- Installer is exactly `DPopCleaner_Setup_0.4.17.exe`.
- No `BETA`, `R`, or `Stage` appears in 0.4.17 public identity.
- Original 0.2.14 core is copied unchanged into the installed payload.
- Site must use a real screenshot/evidence image from the preserved core or verified companion UI, not a fabricated application mockup.
- Public download button is enabled only after install/runtime/module/rollback gates pass.
- Live verification must download the public installer and compare SHA-256 with published metadata.
- Historical 0.3.x auto-publish workflows must not be able to redeploy Pages after 0.4.17.

---

### Task 1: Add exact 0.4.17 release identity contract

**Files:**
- Create: `tests/test_dpop0417_release_contract.py`
- Create: `v0417/contracts/release.json`

**Interfaces:**
- Produces canonical identity consumed by installer/site/workflow tests.

- [ ] **Step 1: Add failing contract test**

```python
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]


def test_release_identity_is_plain_0417():
    release = json.loads((ROOT / "v0417/contracts/release.json").read_text(encoding="utf-8"))
    assert release == {
        "product": "DPopCleaner",
        "version": "0.4.17",
        "windows_file_version": "0.4.17.0",
        "installer": "DPopCleaner_Setup_0.4.17.exe",
        "tag": "v0.4.17"
    }
    forbidden = " ".join(release.values()).upper()
    assert "BETA" not in forbidden
    assert " R1" not in forbidden
    assert "STAGE" not in forbidden
```

- [ ] **Step 2: Run RED**

Expected: `release.json` missing.

- [ ] **Step 3: Add canonical JSON**

```json
{
  "product": "DPopCleaner",
  "version": "0.4.17",
  "windows_file_version": "0.4.17.0",
  "installer": "DPopCleaner_Setup_0.4.17.exe",
  "tag": "v0.4.17"
}
```

- [ ] **Step 4: Run GREEN and commit**

```bash
git add v0417/contracts/release.json tests/test_dpop0417_release_contract.py
git commit -m "test(0.4.17): freeze release identity"
```

### Task 2: Build the clean 0.4.17 Inno installer

**Files:**
- Create: `release/DPopCleaner_0.4.17.iss`
- Create: `release/RELEASE_NOTES_0.4.17.md`
- Modify: `tests/test_dpop0417_release_contract.py`

**Interfaces:**
- Consumes: `_release/0.4.17/stage` from `tools/dpop0417_stage.ps1`.
- Produces: `release/output/DPopCleaner_Setup_0.4.17.exe`.

- [ ] **Step 1: Extend release contract test to require exact Inno identity**

```python
def test_inno_identity_and_documentation_preservation():
    text = (ROOT / "release/DPopCleaner_0.4.17.iss").read_text(encoding="utf-8")
    assert '#define MyAppVersion "0.4.17"' in text
    assert 'OutputBaseFilename=DPopCleaner_Setup_0.4.17' in text
    assert 'VersionInfoVersion=0.4.17.0' in text
    assert 'DPopCleaner.exe' in text
    assert 'Languages\\*' in text
    assert 'Shell\\*' in text
    assert 'Documentation\\*' in text
    assert 'Modules\\*' in text
    assert 'uninsneveruninstall' in text.lower()
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Add installer definition**

Use the existing DPopCleaner AppId to support upgrade continuity, but set display/version/output exactly to 0.4.17. Install executable/module/language/shell files normally. `Documentation\\History`, `Documentation\\Backups`, and `Documentation\\Logs` entries must use `Flags: uninsneveruninstall` so ordinary uninstall does not erase rollback data.

Create Start Menu shortcuts:

```text
DPopCleaner -> {app}\DPopCleaner.exe
DPopCleaner Disk Analyzer -> {app}\Modules\DiskAnalyzer.exe
DPopCleaner Restore Center -> {app}\Modules\RestoreCenter.exe
```

No reconstructed C++ executable is listed.

- [ ] **Step 4: Add release notes truthfully describing original 0.2.14 core + new modules**

Do not claim the old hardcoded window itself was fully localized or rewritten.

- [ ] **Step 5: Run contract GREEN and commit**

```bash
git add release/DPopCleaner_0.4.17.iss release/RELEASE_NOTES_0.4.17.md tests/test_dpop0417_release_contract.py
git commit -m "build(0.4.17): add clean installer"
```

### Task 3: Create a non-publishing package candidate workflow

**Files:**
- Create: `.github/workflows/DPopCleaner_0.4.17_PACKAGE_CANDIDATE.yml`
- Modify: `tests/test_dpop0417_release_contract.py`

**Interfaces:**
- Produces: candidate ZIP containing installer, installed screenshots, reports, release metadata.
- Does not create releases or deploy Pages.

- [ ] **Step 1: Add failing workflow-safety test**

```python
def test_package_candidate_cannot_publish():
    text = (ROOT / ".github/workflows/DPopCleaner_0.4.17_PACKAGE_CANDIDATE.yml").read_text(encoding="utf-8")
    assert "contents: read" in text
    assert "gh release" not in text
    assert "deploy-pages" not in text
    assert "DPopCleaner_Setup_0.4.17.exe" in text
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Add candidate job sequence**

Exact sequence: contracts → `dotnet test` Common/Disk → build Disk/Restore → stage allowlist → run unpackaged Disk/Restore smokes → Inno build → silent install to isolated target → verify original installed core Git blob equals `efd0eff1...` → run installed Disk/Restore smokes → capture original-core screenshot with `scripts/Capture-AppScreenshot.ps1` or a dedicated 0.4.17 capture wrapper → compute installer SHA-256 → upload artifact.

- [ ] **Step 4: Push and require GREEN**

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/DPopCleaner_0.4.17_PACKAGE_CANDIDATE.yml tests/test_dpop0417_release_contract.py
git commit -m "ci(0.4.17): verify installer candidate"
```

### Task 4: Redesign the site around the real 0.4.17 package

**Files:**
- Modify: `index.html`
- Modify: `styles.css`
- Modify: `script.js`
- Modify: `release-manifest.js`
- Create: `assets/dpopcleaner-0.4.17.png` from verified CI screenshot evidence
- Create: `tests/test_dpop0417_site_contract.py`

**Interfaces:**
- Site consumes `update/beta.json` only as the existing update endpoint name if changing endpoint would break legacy code; the payload itself identifies version `0.4.17` and contains no beta label.
- Primary button URL is exact GitHub Release asset URL for tag `v0.4.17` once published.

- [ ] **Step 1: Add failing site contract**

```python
def test_site_is_0417_and_has_no_035_marketing():
    html = (ROOT / "index.html").read_text(encoding="utf-8")
    js = (ROOT / "release-manifest.js").read_text(encoding="utf-8")
    assert "DPopCleaner 0.4.17" in html
    assert "Windows 10/11 x64" in html
    assert "Disk Analyzer" in html or "Анализатор диска" in html
    assert "Центр восстановления" in html
    assert "0.3.5" not in html
    assert "BETA" not in html.upper()
    assert "v0.4.17" in js
    assert "DPopCleaner_Setup_0.4.17.exe" in js
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement the page**

Required visual structure: dark/green hero, real 0.2.14 screenshot, one large `Скачать DPopCleaner 0.4.17` button, compact trust row with Windows 10/11 x64 + installer size + SHA-256, then four feature sections: original core, Disk Analyzer, Restore Center, `Languages/Shell/Documentation` architecture. No fake counters and no 0.3.x sidebar screenshots.

- [ ] **Step 4: Make download fail closed before release**

If manifest says `available !== true`, button stays disabled and text says `Релиз проходит проверку`. Never fall back to a stale 0.3.x URL.

- [ ] **Step 5: Run site contract and commit**

```bash
git add index.html styles.css script.js release-manifest.js tests/test_dpop0417_site_contract.py
git commit -m "site(0.4.17): redesign around original core"
```

The real screenshot binary file is staged from the verified package artifact in the release workflow; do not fabricate it in source.

### Task 5: Disable legacy automatic publishers before 0.4.17 merge

**Files:**
- Modify: `.github/workflows/publish-dpopcleaner-0.3.5.yml`
- Modify any remaining `.github/workflows/*0.3*.yml` that still has `push: main` publication/Pages behavior
- Modify: `.github/workflows/static.yml` if it still listens to an old release workflow
- Create: `tests/test_dpop0417_legacy_publishers.py`

**Interfaces:**
- Historical workflows remain manual (`workflow_dispatch`) or PR verification only.

- [ ] **Step 1: Add failing legacy-publisher contract**

```python
def test_no_legacy_workflow_can_publish_on_main_push():
    for path in (ROOT / ".github/workflows").glob("*.yml"):
        text = path.read_text(encoding="utf-8")
        if "0.4.17" in text:
            continue
        if "gh release" in text or "deploy-pages" in text:
            assert "push:\n    branches: [main]" not in text
            assert "workflows: [\"Build, verify and release DPopCleaner 0.3.1 R3\"]" not in text
```

- [ ] **Step 2: Run RED against current main-derived branch**

- [ ] **Step 3: Remove only automatic publication triggers; keep historical manual workflows for recovery**

- [ ] **Step 4: Run GREEN and commit**

```bash
git add .github/workflows tests/test_dpop0417_legacy_publishers.py
git commit -m "ci: prevent legacy releases from replacing 0.4.17"
```

### Task 6: Add production publisher and live SHA verification

**Files:**
- Create: `.github/workflows/publish-dpopcleaner-0.4.17.yml`
- Modify: `tests/test_dpop0417_release_contract.py`

**Interfaces:**
- On PR: build/package verification only; `publish` and `verify-live` skipped.
- On push to main: package → GitHub Release `v0.4.17` → Pages → live verify.

- [ ] **Step 1: Add failing production policy assertions**

```python
def test_production_workflow_publishes_only_from_main_push():
    text = (ROOT / ".github/workflows/publish-dpopcleaner-0.4.17.yml").read_text(encoding="utf-8")
    assert "v0.4.17" in text
    assert "DPopCleaner_Setup_0.4.17.exe" in text
    assert "github.event_name == 'push'" in text
    assert "Verify live 0.4.17 site, manifest and installer SHA-256" in text
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Add `build-package`, `publish`, and `verify-live` jobs**

`build-package` repeats all candidate gates and writes publication JSON with `version`, `url`, `sha256`, `size`, `available=true`. `publish` uses the verified artifact only, creates/updates tag `v0.4.17`, uploads exact installer, stages Pages with the generated manifest and real screenshot. `verify-live` polls the Pages URL until it sees `0.4.17`, downloads the live manifest, then downloads the public installer URL and compares `Get-FileHash -Algorithm SHA256` to manifest SHA.

- [ ] **Step 4: PR-run and verify publication jobs are skipped**

- [ ] **Step 5: Merge only after candidate + production-PR gates are GREEN**

- [ ] **Step 6: After main publication, verify GitHub Release asset metadata and live site manually in addition to workflow success**

- [ ] **Step 7: Commit**

```bash
git add .github/workflows/publish-dpopcleaner-0.4.17.yml tests/test_dpop0417_release_contract.py
git commit -m "ci(0.4.17): publish verified release and site"
```
