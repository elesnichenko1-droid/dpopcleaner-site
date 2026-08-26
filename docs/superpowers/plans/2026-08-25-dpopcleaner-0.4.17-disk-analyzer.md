# DPopCleaner 0.4.17 Disk Analyzer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a standalone TreeSize-style `DiskAnalyzer.exe` companion to the preserved original 0.2.14 DPopCleaner core.

**Architecture:** The analyzer is a new C#/.NET Framework 4.8 WinForms executable under `v0417/src/DiskAnalyzer`; it references `DPop.Common.dll`, uses external `Languages/*.json`, and never modifies the original core. Scanning runs on a background task with cancellation, builds a testable tree model, and presents a flattened hierarchical `DataGridView` with custom parent-percentage bars.

**Tech Stack:** C#/.NET Framework 4.8, WinForms, P/Invoke for allocated-size query, MSTest, PowerShell UI smoke on Windows.

**Spec:** `docs/superpowers/specs/2026-08-25-dpopcleaner-0.4.17-original-0214-design.md`

## Global Constraints

- Runtime base remains the untouched original 0.2.14 EXE.
- Disk Analyzer is a separate `Modules\\DiskAnalyzer.exe`.
- Required columns: `Имя | Размер | Занято | Файлы | Папки | % родителя | Изменено`.
- Reparse points are not followed by default.
- Unknown allocated size is shown as `—`, never replaced with logical size.
- Scan must be asynchronous and cancellable.
- No arbitrary delete button exists in the analyzer.
- New UI text comes only from `Languages/*.json` through `LanguageCatalog`.

---

### Task 1: Build the scanner model and unknown-allocation semantics

**Files:**
- Create: `v0417/src/DiskAnalyzer/DiskAnalyzer.csproj`
- Create: `v0417/src/DiskAnalyzer/Model/DiskNode.cs`
- Create: `v0417/src/DiskAnalyzer/Scanning/IAllocationSizeProvider.cs`
- Create: `v0417/src/DiskAnalyzer/Scanning/WindowsAllocationSizeProvider.cs`
- Create: `v0417/src/DiskAnalyzer/Scanning/DiskScanner.cs`
- Create: `v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj`
- Create: `v0417/tests/DiskAnalyzer.Tests/DiskScannerTests.cs`

**Interfaces:**
- Produces: `Task<DiskNode> DiskScanner.ScanAsync(string rootPath, CancellationToken token, Action<ScanProgress> progress = null)`.
- `DiskNode` exposes `LogicalBytes`, nullable `AllocatedBytes`, `AllocatedComplete`, `FileCount`, `FolderCount`, `Children`, `ModifiedUtc`.

- [ ] **Step 1: Add failing fixture tests**

```csharp
[TestMethod]
public async Task FolderTotalsAreRecursiveAndUnknownAllocationPropagates()
{
    using (var fixture = DiskFixture.Create())
    {
        fixture.File("a.bin", 100);
        fixture.File("sub/b.bin", 300);
        var provider = new FakeAllocationProvider(new Dictionary<string, long?>
        {
            [fixture.PathOf("a.bin")] = 4096,
            [fixture.PathOf("sub/b.bin")] = null
        });
        var scanner = new DiskScanner(provider);
        var root = await scanner.ScanAsync(fixture.Root, CancellationToken.None);
        Assert.AreEqual(400L, root.LogicalBytes);
        Assert.IsFalse(root.AllocatedComplete);
        Assert.IsNull(root.AllocatedBytes);
        Assert.AreEqual(2L, root.FileCount);
        Assert.AreEqual(1L, root.FolderCount);
    }
}

[TestMethod]
public async Task ReparseDirectoriesAreNotFollowedByDefault()
{
    // Fixture marks a directory entry as reparse through the injectable file-system metadata seam.
    var root = await scanner.ScanAsync(fixture.Root, CancellationToken.None);
    Assert.AreEqual(0L, root.Children.Single(x => x.Name == "link").FileCount);
}
```

- [ ] **Step 2: Run RED**

Run: `dotnet test v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj -c Release`

Expected: missing scanner/model types.

- [ ] **Step 3: Implement model and scanner**

`DiskNode` core:

```csharp
public sealed class DiskNode
{
    public string Name { get; set; }
    public string FullPath { get; set; }
    public bool IsDirectory { get; set; }
    public long LogicalBytes { get; set; }
    public long? AllocatedBytes { get; set; }
    public bool AllocatedComplete { get; set; } = true;
    public long FileCount { get; set; }
    public long FolderCount { get; set; }
    public DateTime ModifiedUtc { get; set; }
    public List<DiskNode> Children { get; } = new List<DiskNode>();
}
```

Directory aggregation rule:

```csharp
if (child.AllocatedComplete && child.AllocatedBytes.HasValue && node.AllocatedComplete)
    allocatedTotal += child.AllocatedBytes.Value;
else
    node.AllocatedComplete = false;

node.AllocatedBytes = node.AllocatedComplete ? (long?)allocatedTotal : null;
```

`WindowsAllocationSizeProvider` must call `GetCompressedFileSizeW`; when it returns `INVALID_FILE_SIZE` and `GetLastWin32Error() != 0`, return `null`.

- [ ] **Step 4: Run GREEN**

Run: `dotnet test v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj -c Release`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add v0417/src/DiskAnalyzer v0417/tests/DiskAnalyzer.Tests
git commit -m "feat(0.4.17): add recursive disk scanner"
```

### Task 2: Add cancellation and progressive scan reporting

**Files:**
- Create: `v0417/src/DiskAnalyzer/Scanning/ScanProgress.cs`
- Modify: `v0417/src/DiskAnalyzer/Scanning/DiskScanner.cs`
- Modify: `v0417/tests/DiskAnalyzer.Tests/DiskScannerTests.cs`

**Interfaces:**
- Produces: `ScanProgress` with `CurrentPath`, `FilesScanned`, `FoldersScanned`, `LogicalBytesFound`.

- [ ] **Step 1: Add failing progress/cancellation tests**

```csharp
[TestMethod]
public async Task ScanReportsProgressAndHonorsCancellation()
{
    using (var fixture = DiskFixture.CreateManyFiles(200))
    {
        var seen = new List<ScanProgress>();
        var cts = new CancellationTokenSource();
        var scanner = new DiskScanner(new FakeAllocationProvider());
        await Assert.ThrowsExceptionAsync<OperationCanceledException>(async () =>
            await scanner.ScanAsync(fixture.Root, cts.Token, p =>
            {
                seen.Add(p);
                if (p.FilesScanned >= 10) cts.Cancel();
            }));
        Assert.IsTrue(seen.Count > 0);
    }
}
```

- [ ] **Step 2: Run RED**

Expected: no progress type/overload.

- [ ] **Step 3: Implement progress snapshots and `token.ThrowIfCancellationRequested()` before every directory enumeration and file iteration**

Report no more often than every 100 entries or 100 ms to avoid flooding UI.

- [ ] **Step 4: Run GREEN and commit**

Run: `dotnet test v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj -c Release`

```bash
git add v0417/src/DiskAnalyzer/Scanning v0417/tests/DiskAnalyzer.Tests/DiskScannerTests.cs
git commit -m "feat(0.4.17): make disk scan cancellable and progressive"
```

### Task 3: Build the compact WinForms analyzer UI

**Files:**
- Create: `v0417/src/DiskAnalyzer/Program.cs`
- Create: `v0417/src/DiskAnalyzer/UI/DiskAnalyzerForm.cs`
- Create: `v0417/src/DiskAnalyzer/UI/DiskGridRow.cs`
- Create: `v0417/src/DiskAnalyzer/UI/SizeFormatter.cs`
- Create: `v0417/tests/DiskAnalyzer.Tests/SizeFormatterTests.cs`

**Interfaces:**
- Form constructor: `DiskAnalyzerForm(LanguageCatalog language, DiskScanner scanner)`.
- `SizeFormatter.AllocatedText(DiskNode node)` returns `—` if `!AllocatedComplete || !AllocatedBytes.HasValue`.

- [ ] **Step 1: Add failing formatter contract**

```csharp
[TestMethod]
public void UnknownAllocatedSizeIsDash()
{
    var node = new DiskNode { AllocatedComplete = false, AllocatedBytes = null };
    Assert.AreEqual("—", SizeFormatter.AllocatedText(node));
}
```

- [ ] **Step 2: Run RED**

Run: `dotnet test v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj -c Release`

- [ ] **Step 3: Implement form layout**

Use one top toolbar (`Назад`, `C:\\`, `Выбрать каталог`, `Сканировать`, `Стоп`, `Обновить`, `Крупные файлы`, `Проводник`), one path textbox, one status label, and one `DataGridView` with exactly seven columns. Name-cell values use indentation text (`new string(' ', depth * 2) + name`) rather than overlapping child controls. Long work uses `Task.Run`/`await`; all UI changes marshal back via `BeginInvoke`.

- [ ] **Step 4: Implement percentage cell painting**

In `CellPainting`, fill a rectangle proportional to `row.ParentPercent` before drawing text. Root percent is exactly `100.0`; for a child with parent logical size zero, percent is `0.0`.

- [ ] **Step 5: Run build/tests**

Run:

```powershell
dotnet test v0417/tests/DiskAnalyzer.Tests/DiskAnalyzer.Tests.csproj -c Release
dotnet build v0417/src/DiskAnalyzer/DiskAnalyzer.csproj -c Release
```

Expected: PASS/build success.

- [ ] **Step 6: Commit**

```bash
git add v0417/src/DiskAnalyzer/UI v0417/src/DiskAnalyzer/Program.cs v0417/tests/DiskAnalyzer.Tests
git commit -m "feat(0.4.17): add TreeSize-style analyzer UI"
```

### Task 4: Add numeric sorting, large-file view, and Explorer action

**Files:**
- Create: `v0417/src/DiskAnalyzer/UI/DiskGridSorter.cs`
- Create: `v0417/src/DiskAnalyzer/Model/DiskTreeQuery.cs`
- Create: `v0417/tests/DiskAnalyzer.Tests/DiskTreeQueryTests.cs`
- Modify: `v0417/src/DiskAnalyzer/UI/DiskAnalyzerForm.cs`

**Interfaces:**
- `DiskTreeQuery.LargestFiles(DiskNode root, int limit)` returns files ordered by `LogicalBytes` descending.
- `DiskGridSorter` compares raw numeric row properties, not formatted strings.

- [ ] **Step 1: Add failing numeric-order test**

```csharp
[TestMethod]
public void LargestFilesSortsNumericallyNotLexically()
{
    var files = DiskTreeQuery.LargestFiles(FixtureTree.WithFiles(900, 12000, 1000), 3);
    CollectionAssert.AreEqual(new long[] { 12000, 1000, 900 }, files.Select(x => x.LogicalBytes).ToArray());
}
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement query and sorting; wire `Крупные файлы` to a flat file-only grid and `Проводник` to `explorer.exe /select,"<path>"` for files or open directory path for folders**

Do not add any delete API or delete button.

- [ ] **Step 4: Run GREEN and commit**

```bash
git add v0417/src/DiskAnalyzer v0417/tests/DiskAnalyzer.Tests
git commit -m "feat(0.4.17): add large-file and explorer actions"
```

### Task 5: Add Windows UI smoke on a controlled fixture

**Files:**
- Create: `tools/dpop0417_disk_smoke.ps1`
- Create: `tests/test_dpop0417_disk_smoke_contract.py`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- Smoke accepts `-ExePath` and `-OutputDir`, creates a tiny fixture, starts analyzer with `--root <fixture>`, waits for scan completion, captures screenshot and `disk-smoke-report.json`.

- [ ] **Step 1: Add failing smoke-policy test**

```python
def test_disk_smoke_requires_fixture_and_no_delete_action():
    text = (ROOT / "tools/dpop0417_disk_smoke.ps1").read_text(encoding="utf-8")
    assert "dpop0417-disk-fixture" in text
    assert "disk-smoke-report.json" in text
    assert "DeleteFile" not in text
    assert "Remove-Item $fixture" in text
```

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Implement smoke and add CI step after .NET build**

Fixture contains exactly two files and one subfolder; expected report values are asserted in script. The screenshot must show all seven columns and visible toolbar labels at 1200×850.

- [ ] **Step 4: Push and require Windows workflow GREEN**

- [ ] **Step 5: Commit**

```bash
git add tools/dpop0417_disk_smoke.ps1 tests/test_dpop0417_disk_smoke_contract.py .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "ci(0.4.17): verify disk analyzer on Windows"
```
