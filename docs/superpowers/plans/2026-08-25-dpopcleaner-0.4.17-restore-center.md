# DPopCleaner 0.4.17 Restore Center Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a standalone `RestoreCenter.exe` that records supported reversible actions under `Documentation`, preserves backups, and can safely restore only records that are actually reversible.

**Architecture:** A shared history/backup model is added to `DPop.Common`; Restore Center is a separate C#/.NET Framework 4.8 WinForms executable. History is append-only JSON, backups are immutable once referenced, and rollback uses typed providers rather than executing arbitrary command strings. The original 0.2.14 core remains untouched.

**Tech Stack:** C#/.NET Framework 4.8, WinForms, `Microsoft.Win32.Registry`, JSON serialization with framework APIs, MSTest, PowerShell smoke.

**Spec:** `docs/superpowers/specs/2026-08-25-dpopcleaner-0.4.17-original-0214-design.md`

## Global Constraints

- Restore is exposed only when enough prior state was captured.
- TEMP/cache deletion, Recycle Bin emptying, arbitrary third-party uninstallers, and uncaptured external changes are not claimed reversible.
- A failed rollback must keep the backup and write diagnostics.
- Before restoring, current state is captured as a new history record when technically possible.
- Runtime history lives under `Documentation\\History`; backups under `Documentation\\Backups`; diagnostics under `Documentation\\Logs`.
- No arbitrary script/command execution is read from history files.
- UI text uses external language packs only.

---

### Task 1: Define append-only history records

**Files:**
- Create: `v0417/src/DPop.Common/History/HistoryRecord.cs`
- Create: `v0417/src/DPop.Common/History/HistoryStore.cs`
- Create: `v0417/tests/DPop.Common.Tests/HistoryStoreTests.cs`

**Interfaces:**
- `HistoryStore.Append(HistoryRecord record)` writes one immutable record file.
- `IReadOnlyList<HistoryRecord> HistoryStore.ReadAll()` reads records newest-first.
- Record fields: `Id`, `TimestampUtc`, `OperationId`, `Description`, `Target`, `BeforeState`, `AfterState`, `BackupReference`, `RollbackAvailable`, `RollbackStatus`.

- [ ] **Step 1: Add failing append/read test**

```csharp
[TestMethod]
public void HistoryIsAppendOnlyAndNewestFirst()
{
    using (var fixture = HistoryFixture.Create())
    {
        var store = new HistoryStore(fixture.HistoryDirectory);
        store.Append(HistoryRecord.Create("one", "A", rollbackAvailable: true));
        store.Append(HistoryRecord.Create("two", "B", rollbackAvailable: false));
        var records = store.ReadAll();
        Assert.AreEqual(2, records.Count);
        Assert.AreEqual("two", records[0].OperationId);
        Assert.AreEqual("one", records[1].OperationId);
    }
}
```

- [ ] **Step 2: Run RED**

Run: `dotnet test v0417/tests/DPop.Common.Tests/DPop.Common.Tests.csproj -c Release`

- [ ] **Step 3: Implement one-file-per-record storage**

File name format:

```csharp
var fileName = record.TimestampUtc.ToString("yyyyMMdd-HHmmssfff") + "-" + record.Id.ToString("N") + ".json";
```

Write to `*.tmp`, flush/close, then `File.Move(tmp, final)` so half-written history files are not exposed.

- [ ] **Step 4: Run GREEN and commit**

```bash
git add v0417/src/DPop.Common/History v0417/tests/DPop.Common.Tests/HistoryStoreTests.cs
git commit -m "feat(0.4.17): add append-only history store"
```

### Task 2: Implement immutable backup storage

**Files:**
- Create: `v0417/src/DPop.Common/History/BackupStore.cs`
- Create: `v0417/tests/DPop.Common.Tests/BackupStoreTests.cs`

**Interfaces:**
- `string BackupStore.SaveBytes(string category, byte[] data)` returns a relative reference such as `Registry\\<guid>.bin`.
- `byte[] BackupStore.ReadBytes(string reference)` rejects path traversal.

- [ ] **Step 1: Add failing path-safety test**

```csharp
[TestMethod]
public void BackupReferenceCannotEscapeDocumentationRoot()
{
    var store = new BackupStore(fixture.BackupsDirectory);
    Assert.ThrowsException<InvalidDataException>(() => store.ReadBytes("..\\..\\outside.bin"));
}
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement category allowlist**

Only `Settings`, `Registry`, and `System` are valid categories. Resolve with `Path.GetFullPath`, then require the result to start with the normalized backups root plus directory separator.

- [ ] **Step 4: Run GREEN and commit**

```bash
git add v0417/src/DPop.Common/History/BackupStore.cs v0417/tests/DPop.Common.Tests/BackupStoreTests.cs
git commit -m "feat(0.4.17): add safe backup store"
```

### Task 3: Add typed reversible providers for file settings and HKCU registry values

**Files:**
- Create: `v0417/src/DPop.Common/Restore/IRestoreProvider.cs`
- Create: `v0417/src/DPop.Common/Restore/FileStateProvider.cs`
- Create: `v0417/src/DPop.Common/Restore/HkcuRegistryValueProvider.cs`
- Create: `v0417/tests/DPop.Common.Tests/FileStateProviderTests.cs`
- Create: `v0417/tests/DPop.Common.Tests/HkcuRegistryValueProviderTests.cs`

**Interfaces:**
- `IRestoreProvider.Capture(string target)` returns serializable before-state bytes.
- `IRestoreProvider.Restore(string target, byte[] state)` restores and verifies.
- `IRestoreProvider.CanHandle(string operationId)` selects provider by fixed operation ids.

- [ ] **Step 1: Add failing file round-trip test**

```csharp
[TestMethod]
public void FileStateRoundTripRestoresOriginalBytes()
{
    var file = fixture.CreateFile("settings.json", "before");
    var provider = new FileStateProvider();
    var state = provider.Capture(file);
    File.WriteAllText(file, "after");
    provider.Restore(file, state);
    Assert.AreEqual("before", File.ReadAllText(file));
}
```

- [ ] **Step 2: Add failing HKCU round-trip test under a disposable key**

Use `HKCU\\Software\\DPopCleanerTests\\<guid>` only; create value `Mode=before`, capture, change to `after`, restore, verify `before`, then delete the disposable test key in `finally`.

- [ ] **Step 3: Run RED**

- [ ] **Step 4: Implement providers**

Registry target format is fixed JSON containing only `subKey`, `valueName`, and captured `RegistryValueKind`; provider always opens `Registry.CurrentUser`, never accepts a hive name from history input.

- [ ] **Step 5: Run GREEN and commit**

```bash
git add v0417/src/DPop.Common/Restore v0417/tests/DPop.Common.Tests/*ProviderTests.cs
git commit -m "feat(0.4.17): add typed rollback providers"
```

### Task 4: Implement transactional rollback coordinator

**Files:**
- Create: `v0417/src/DPop.Common/Restore/RestoreCoordinator.cs`
- Create: `v0417/src/DPop.Common/Restore/RestoreResult.cs`
- Create: `v0417/tests/DPop.Common.Tests/RestoreCoordinatorTests.cs`

**Interfaces:**
- `RestoreResult RestoreCoordinator.Restore(Guid historyRecordId)`.
- Coordinator consumes `HistoryStore`, `BackupStore`, and a fixed provider list.

- [ ] **Step 1: Add failing success test**

```csharp
[TestMethod]
public void RestoreCapturesCurrentStateThenRestoresBackup()
{
    var result = fixture.Coordinator.Restore(fixture.ReversibleRecordId);
    Assert.IsTrue(result.Success);
    Assert.AreEqual("before", File.ReadAllText(fixture.TargetFile));
    Assert.IsTrue(fixture.Store.ReadAll().Any(x => x.OperationId == "restore.prestate"));
}
```

- [ ] **Step 2: Add failing non-reversible test**

```csharp
[TestMethod]
public void NonReversibleRecordIsRejectedWithoutSideEffects()
{
    var result = fixture.Coordinator.Restore(fixture.NonReversibleRecordId);
    Assert.IsFalse(result.Success);
    StringAssert.Contains(result.Message, "Откат недоступен");
}
```

- [ ] **Step 3: Run RED**

- [ ] **Step 4: Implement transaction order**

Exact order: load history record → require `RollbackAvailable` and backup reference → resolve typed provider → validate/read backup → capture current state → save current-state backup → append `restore.prestate` history → provider restore → provider recapture/verify → append success/failure result. Never delete the original backup.

- [ ] **Step 5: Run GREEN and commit**

```bash
git add v0417/src/DPop.Common/Restore/RestoreCoordinator.cs v0417/src/DPop.Common/Restore/RestoreResult.cs v0417/tests/DPop.Common.Tests/RestoreCoordinatorTests.cs
git commit -m "feat(0.4.17): add transactional restore coordinator"
```

### Task 5: Build Restore Center WinForms UI

**Files:**
- Create: `v0417/src/RestoreCenter/RestoreCenter.csproj`
- Create: `v0417/src/RestoreCenter/Program.cs`
- Create: `v0417/src/RestoreCenter/UI/RestoreCenterForm.cs`
- Create: `v0417/src/RestoreCenter/UI/HistoryRow.cs`

**Interfaces:**
- Form constructor: `RestoreCenterForm(LanguageCatalog language, HistoryStore store, RestoreCoordinator coordinator)`.
- Required columns: `Дата | Действие | Объект | Состояние | Откат`.

- [ ] **Step 1: Add compile-first RED by referencing missing `RestoreCenter.csproj` from the Windows workflow**

Expected: build fails because project does not exist.

- [ ] **Step 2: Create net48 WinForms project referencing `DPop.Common`**

Main form uses one `DataGridView`, details text box, `Обновить`, `Восстановить`, and `Закрыть`. The Restore button is disabled whenever selected `HistoryRecord.RollbackAvailable == false`.

- [ ] **Step 3: Wire external language keys**

No hardcoded Russian/English labels in controls. Use keys `restore.title`, `restore.rollback`, `restore.unavailable`, `restore.column.date`, `restore.column.action`, `restore.column.target`, `restore.column.status`, `restore.column.rollback` added to both language packs.

- [ ] **Step 4: Build**

Run: `dotnet build v0417/src/RestoreCenter/RestoreCenter.csproj -c Release`

Expected: success.

- [ ] **Step 5: Commit**

```bash
git add v0417/src/RestoreCenter v0417/payload/Languages
git commit -m "feat(0.4.17): add restore center UI"
```

### Task 6: Add installed rollback smoke

**Files:**
- Create: `tools/dpop0417_restore_smoke.ps1`
- Create: `tests/test_dpop0417_restore_smoke_contract.py`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- Smoke creates isolated `Documentation`, writes a reversible file-state history record through a test CLI mode, changes target, launches/restores, verifies original bytes, then creates a non-reversible record and verifies the UI/report says rollback unavailable.

- [ ] **Step 1: Add failing smoke contract**

```python
def test_restore_smoke_checks_both_reversible_and_nonreversible_paths():
    text = (ROOT / "tools/dpop0417_restore_smoke.ps1").read_text(encoding="utf-8")
    assert "reversible-roundtrip" in text
    assert "nonreversible" in text
    assert "restore-smoke-report.json" in text
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement deterministic test CLI switches**

`RestoreCenter.exe --smoke-create-file-record <docRoot> <target>` creates a reversible record; `--smoke-restore-latest <docRoot>` restores it; these switches are accepted only for local file-state fixtures and never arbitrary command execution.

- [ ] **Step 4: Run smoke in Windows CI and require report fields**

Report must contain:

```json
{
  "reversible_roundtrip": true,
  "nonreversible_restore_exposed": false,
  "backup_preserved": true
}
```

- [ ] **Step 5: Commit**

```bash
git add tools/dpop0417_restore_smoke.ps1 tests/test_dpop0417_restore_smoke_contract.py .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "ci(0.4.17): verify restore round-trip"
```
