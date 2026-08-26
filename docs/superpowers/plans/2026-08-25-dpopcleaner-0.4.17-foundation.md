# DPopCleaner 0.4.17 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the non-reconstructed 0.4.17 foundation around the preserved original 0.2.14 EXE: exact-core contract, external language packs, deterministic `Shell`/`Documentation` folders, shared .NET Framework 4.8 support library, and a safe installer staging pipeline.

**Architecture:** `downloads/DPopCleaner_0.2.14_BETA.exe` remains byte-for-byte the legacy core. New functionality lives under `v0417/` as C#/.NET Framework 4.8 companion code and static payload; no reconstructed C++ application files are compiled or staged. Packaging copies the original core unchanged and builds only new companion binaries.

**Tech Stack:** Windows 10/11 x64, C#/.NET Framework 4.8, WinForms-compatible shared library, Python 3.12 contract tests, PowerShell CI, Inno Setup 6.

**Spec:** `docs/superpowers/specs/2026-08-25-dpopcleaner-0.4.17-original-0214-design.md`

## Global Constraints

- Product version is exactly `0.4.17`.
- No `BETA`, no `R1/R2`, no `Stage` in 0.4.17 product identity.
- Public installer name is exactly `DPopCleaner_Setup_0.4.17.exe`.
- Preserved core path is exactly `downloads/DPopCleaner_0.2.14_BETA.exe`.
- Preserved core Git blob SHA is `efd0eff1f4962319282363fa85595c25e0cebe11` and size is `389632` bytes.
- `0.3.x`, `v035_overlay`, recovered shell, and reconstructed C++ DPopCleaner sources are forbidden from the 0.4.17 runtime payload.
- New module localization comes only from `Languages/*.json`; one language is active at a time with Russian fallback.
- Installer creates `Languages`, `Shell`, `Documentation`, `Modules`, and `Resources` deterministically.
- `Documentation` data must survive ordinary upgrade/uninstall unless the user explicitly elects removal.

---

### Task 1: Freeze the original 0.2.14 core contract

**Files:**
- Create: `v0417/contracts/core.json`
- Create: `tests/test_dpop0417_core_contract.py`

**Interfaces:**
- Consumes: checked-in `downloads/DPopCleaner_0.2.14_BETA.exe`.
- Produces: `v0417/contracts/core.json` containing `path`, `git_blob_sha1`, `size`, and `staged_name`; release scripts consume this exact contract.

- [ ] **Step 1: Write the failing contract test**

```python
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_original_0214_core_is_frozen():
    contract = json.loads((ROOT / "v0417/contracts/core.json").read_text(encoding="utf-8"))
    core = ROOT / contract["path"]
    assert core.stat().st_size == 389632
    blob = subprocess.check_output(["git", "hash-object", str(core)], cwd=ROOT, text=True).strip()
    assert blob == "efd0eff1f4962319282363fa85595c25e0cebe11"
    assert contract["staged_name"] == "DPopCleaner.exe"
```

- [ ] **Step 2: Run the test and confirm RED**

Run: `python tests/test_dpop0417_core_contract.py -v`

Expected: failure because `v0417/contracts/core.json` does not exist.

- [ ] **Step 3: Add the immutable core contract**

```json
{
  "path": "downloads/DPopCleaner_0.2.14_BETA.exe",
  "git_blob_sha1": "efd0eff1f4962319282363fa85595c25e0cebe11",
  "size": 389632,
  "staged_name": "DPopCleaner.exe"
}
```

- [ ] **Step 4: Re-run the test**

Run: `python tests/test_dpop0417_core_contract.py -v`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add v0417/contracts/core.json tests/test_dpop0417_core_contract.py
git commit -m "test(0.4.17): freeze original 0.2.14 core"
```

### Task 2: Create the shared 0.4.17 support library without C++ reconstruction

**Files:**
- Create: `v0417/src/DPop.Common/DPop.Common.csproj`
- Create: `v0417/src/DPop.Common/AppIdentity.cs`
- Create: `v0417/src/DPop.Common/AppPaths.cs`
- Create: `v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj`
- Create: `v0417/tests/DPop.Common.Tests/AppPathsTests.cs`

**Interfaces:**
- Produces: `DPop.Common.AppIdentity.Version == "0.4.17"` and `DPop.Common.AppPaths` with `InstallRoot`, `LanguagesDirectory`, `ShellDirectory`, `DocumentationDirectory`, `ModulesDirectory`.
- Later plans consume `DPop.Common.dll` from both companion EXEs.

- [ ] **Step 1: Add a failing path/identity test**

```csharp
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common;
using System.IO;

namespace DPop.Common.Tests
{
    [TestClass]
    public class AppPathsTests
    {
        [TestMethod]
        public void IdentityAndFoldersAreDeterministic()
        {
            var root = Path.Combine("C:\\Program Files", "DPopCleaner");
            var paths = new AppPaths(root);
            Assert.AreEqual("0.4.17", AppIdentity.Version);
            Assert.AreEqual(Path.Combine(root, "Languages"), paths.LanguagesDirectory);
            Assert.AreEqual(Path.Combine(root, "Shell"), paths.ShellDirectory);
            Assert.AreEqual(Path.Combine(root, "Documentation"), paths.DocumentationDirectory);
            Assert.AreEqual(Path.Combine(root, "Modules"), paths.ModulesDirectory);
        }
    }
}
```

- [ ] **Step 2: Run and confirm RED**

Run: `dotnet test v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj -c Release`

Expected: project/type-not-found failure.

- [ ] **Step 3: Add SDK-style net48 projects and minimal implementation**

`DPop.Common.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net48</TargetFramework>
    <LangVersion>latest</LangVersion>
    <AssemblyName>DPop.Common</AssemblyName>
    <RootNamespace>DPop.Common</RootNamespace>
  </PropertyGroup>
</Project>
```

`AppIdentity.cs`:

```csharp
namespace DPop.Common
{
    public static class AppIdentity
    {
        public const string Version = "0.4.17";
        public const string ProductName = "DPopCleaner";
    }
}
```

`AppPaths.cs`:

```csharp
using System.IO;

namespace DPop.Common
{
    public sealed class AppPaths
    {
        public AppPaths(string installRoot) { InstallRoot = installRoot; }
        public string InstallRoot { get; }
        public string LanguagesDirectory => Path.Combine(InstallRoot, "Languages");
        public string ShellDirectory => Path.Combine(InstallRoot, "Shell");
        public string DocumentationDirectory => Path.Combine(InstallRoot, "Documentation");
        public string ModulesDirectory => Path.Combine(InstallRoot, "Modules");
        public string ResourcesDirectory => Path.Combine(InstallRoot, "Resources");
    }
}
```

- [ ] **Step 4: Add MSTest references and run GREEN**

Use package references `MSTest.TestAdapter` and `MSTest.TestFramework` in the test project, then run:

`dotnet test v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj -c Release`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add v0417/src/DPop.Common v0417/tests/DPop.Common.Tests
git commit -m "feat(0.4.17): add shared companion foundation"
```

### Task 3: Implement external language packs with Russian fallback

**Files:**
- Create: `v0417/src/DPop.Common/Localization/LanguageCatalog.cs`
- Create: `v0417/payload/Languages/ru.json`
- Create: `v0417/payload/Languages/en.json`
- Create: `v0417/tests/DPop.Common.Tests/LanguageCatalogTests.cs`

**Interfaces:**
- Produces: `LanguageCatalog.Load(string languagesDirectory, string requestedCode)` and `string Get(string key)`.
- Disk Analyzer and Restore Center consume only `Get(key)` for UI text.

- [ ] **Step 1: Add failing localization tests**

```csharp
[TestMethod]
public void MissingEnglishKeyFallsBackToRussian()
{
    var dir = TestLanguageFixture.Create(
        ru: "{\"common.ok\":\"ОК\",\"disk.scan\":\"Сканировать\"}",
        en: "{\"common.ok\":\"OK\"}");
    var catalog = LanguageCatalog.Load(dir, "en");
    Assert.AreEqual("OK", catalog.Get("common.ok"));
    Assert.AreEqual("Сканировать", catalog.Get("disk.scan"));
}

[TestMethod]
public void UnknownLanguageFallsBackToRussianPack()
{
    var dir = TestLanguageFixture.Create(ru: "{\"common.ok\":\"ОК\"}", en: "{}");
    var catalog = LanguageCatalog.Load(dir, "de");
    Assert.AreEqual("ОК", catalog.Get("common.ok"));
}
```

- [ ] **Step 2: Run RED**

Run: `dotnet test v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj -c Release`

Expected: `LanguageCatalog` missing.

- [ ] **Step 3: Implement one-dictionary-at-a-time loading**

Use `System.Web.Script.Serialization.JavaScriptSerializer` from `System.Web.Extensions`; load `ru.json` first as fallback, then requested pack into a second dictionary. `Get(key)` must return requested value, then Russian value, then `[` + key + `]`.

Core method:

```csharp
public string Get(string key)
{
    string value;
    if (_active.TryGetValue(key, out value)) return value;
    if (_fallback.TryGetValue(key, out value)) return value;
    return "[" + key + "]";
}
```

- [ ] **Step 4: Add real pack keys used by planned modules**

Both files must define at least:

```json
{
  "app.version": "0.4.17",
  "disk.title": "Анализатор диска",
  "disk.scan": "Сканировать",
  "disk.stop": "Стоп",
  "restore.title": "Центр восстановления",
  "restore.rollback": "Восстановить",
  "common.close": "Закрыть",
  "common.error": "Ошибка"
}
```

English values are the direct English equivalents.

- [ ] **Step 5: Run GREEN and commit**

Run: `dotnet test v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj -c Release`

Expected: PASS.

```bash
git add v0417/src/DPop.Common/Localization v0417/payload/Languages v0417/tests/DPop.Common.Tests/LanguageCatalogTests.cs
git commit -m "feat(0.4.17): externalize companion languages"
```

### Task 4: Define real Shell and Documentation payload structure

**Files:**
- Create: `v0417/payload/Shell/shell.json`
- Create: `v0417/payload/Shell/commands/disk-analyzer.json`
- Create: `v0417/payload/Shell/commands/restore-center.json`
- Create: `v0417/payload/Documentation/README.txt`
- Create: `v0417/payload/Documentation/History/.gitkeep`
- Create: `v0417/payload/Documentation/Backups/Settings/.gitkeep`
- Create: `v0417/payload/Documentation/Backups/Registry/.gitkeep`
- Create: `v0417/payload/Documentation/Backups/System/.gitkeep`
- Create: `v0417/payload/Documentation/Logs/.gitkeep`
- Create: `tests/test_dpop0417_layout_contract.py`

**Interfaces:**
- Produces: static `Shell` manifest entries whose executable paths are fixed relative names under `Modules`, never arbitrary user-provided commands.
- Produces: installed rollback folder skeleton consumed by Restore Center.

- [ ] **Step 1: Add a failing structure/security contract**

```python
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]


def test_shell_and_documentation_layout_is_real_and_safe():
    base = ROOT / "v0417/payload"
    shell = json.loads((base / "Shell/shell.json").read_text(encoding="utf-8"))
    assert shell["version"] == "0.4.17"
    assert shell["commands"] == ["disk-analyzer", "restore-center"]
    for rel in [
        "Documentation/History/.gitkeep",
        "Documentation/Backups/Settings/.gitkeep",
        "Documentation/Backups/Registry/.gitkeep",
        "Documentation/Backups/System/.gitkeep",
        "Documentation/Logs/.gitkeep",
    ]:
        assert (base / rel).is_file()
```

- [ ] **Step 2: Run RED**

Run: `python tests/test_dpop0417_layout_contract.py -v`

Expected: missing payload files.

- [ ] **Step 3: Add static manifests**

`shell.json`:

```json
{
  "version": "0.4.17",
  "commands": ["disk-analyzer", "restore-center"]
}
```

`disk-analyzer.json`:

```json
{
  "id": "disk-analyzer",
  "executable": "Modules\\DiskAnalyzer.exe",
  "arguments": []
}
```

`restore-center.json` uses `Modules\\RestoreCenter.exe` and no free-form command string.

- [ ] **Step 4: Run GREEN and commit**

Run: `python tests/test_dpop0417_layout_contract.py -v`

Expected: PASS.

```bash
git add v0417/payload tests/test_dpop0417_layout_contract.py
git commit -m "feat(0.4.17): add shell and documentation layout"
```

### Task 5: Stage a 0.4.17 payload that never includes reconstructed C++ runtime code

**Files:**
- Create: `tools/dpop0417_stage.ps1`
- Create: `tests/test_dpop0417_stage_contract.py`
- Create: `v0417/stage-allowlist.txt`

**Interfaces:**
- Consumes: immutable core, `v0417/payload`, and compiled `v0417/src/*/bin/Release` companion outputs.
- Produces: `_release/0.4.17/stage/` with only approved runtime files.

- [ ] **Step 1: Add failing allowlist contract**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_0417_stage_allowlist_forbids_reconstructed_application_sources():
    text = (ROOT / "v0417/stage-allowlist.txt").read_text(encoding="utf-8")
    assert "DPopCleaner.exe" in text
    assert "Languages/" in text
    assert "Shell/" in text
    assert "Documentation/" in text
    assert "Modules/" in text
    for forbidden in ["v035_overlay", "MainWindow.cpp", "PageBase.cpp", "FullCore.cpp"]:
        assert forbidden not in text
```

- [ ] **Step 2: Run RED**

Run: `python tests/test_dpop0417_stage_contract.py -v`

Expected: allowlist missing.

- [ ] **Step 3: Add explicit allowlist and staging script**

Allowlist:

```text
DPopCleaner.exe
Languages/
Shell/
Documentation/
Modules/DPop.Common.dll
Modules/DiskAnalyzer.exe
Modules/RestoreCenter.exe
Resources/
```

Core staging logic must copy the original bytes directly:

```powershell
$contract = Get-Content v0417/contracts/core.json -Raw | ConvertFrom-Json
Copy-Item -LiteralPath $contract.path -Destination (Join-Path $Stage 'DPopCleaner.exe') -Force
$blob = git hash-object (Join-Path $Stage 'DPopCleaner.exe')
if ($blob -ne $contract.git_blob_sha1) { throw 'Original 0.2.14 core changed during staging.' }
```

The script must copy only `v0417/payload/*` and named companion build outputs; it must not recursively copy repository roots or any `v0xx_overlay` directory.

- [ ] **Step 4: Run GREEN**

Run: `python tests/test_dpop0417_stage_contract.py -v`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/dpop0417_stage.ps1 v0417/stage-allowlist.txt tests/test_dpop0417_stage_contract.py
git commit -m "build(0.4.17): stage only original core and companions"
```

### Task 6: Add a foundation-only Windows CI gate

**Files:**
- Create: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`
- Create: `tests/test_dpop0417_workflow_contract.py`

**Interfaces:**
- Produces: a Windows artifact containing the immutable original core, language packs, Shell, Documentation, and `DPop.Common.dll`; Disk/Restore EXEs may be absent until their plans land.

- [ ] **Step 1: Add failing workflow policy test**

```python
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_foundation_workflow_never_builds_reconstructed_cpp_app():
    text = (ROOT / ".github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml").read_text(encoding="utf-8")
    assert "dotnet test v0417/tests/DPop.Common.Tests" in text
    assert "test_dpop0417_core_contract.py" in text
    assert "dpop0417_stage.ps1" in text
    assert "cmake" not in text.lower()
    assert "v035_overlay" not in text
```

- [ ] **Step 2: Run RED**

Run: `python tests/test_dpop0417_workflow_contract.py -v`

Expected: workflow missing.

- [ ] **Step 3: Add Windows workflow**

The job sequence must be exactly: checkout → Python → core/layout/stage contracts → .NET test/build → run staging → verify staged core Git blob → upload artifact. It must have `contents: read` only and no release or Pages permissions.

- [ ] **Step 4: Run local contracts and push**

Run:

```powershell
python tests/test_dpop0417_core_contract.py -v
python tests/test_dpop0417_layout_contract.py -v
python tests/test_dpop0417_stage_contract.py -v
python tests/test_dpop0417_workflow_contract.py -v
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml tests/test_dpop0417_workflow_contract.py
git commit -m "ci(0.4.17): verify original-core foundation"
```
