using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using DPop.Common.Localization;
using DPop.DiskAnalyzer.Model;
using DPop.DiskAnalyzer.Scanning;

namespace DPop.DiskAnalyzer.UI
{
    public sealed class DiskAnalyzerForm : Form
    {
        private readonly LanguageCatalog _language;
        private readonly DiskScanner _scanner;
        private readonly FlowLayoutPanel _toolbar;
        private readonly TextBox _pathBox;
        private readonly DataGridView _grid;
        private readonly Label _status;
        private readonly Button _scanButton;
        private readonly Button _stopButton;
        private readonly Button _largeFilesButton;
        private readonly Button _explorerButton;

        private CancellationTokenSource _scanCancellation;
        private string _currentRoot;
        private DiskNode _rootNode;
        private string _sortColumn = "size";
        private bool _sortAscending;
        private bool _showingLargeFiles;

        public DiskAnalyzerForm(LanguageCatalog language, DiskScanner scanner)
        {
            _language = language ?? throw new ArgumentNullException(nameof(language));
            _scanner = scanner ?? throw new ArgumentNullException(nameof(scanner));

            Text = "DPopCleaner 0.4.17 — " + L("disk.title");
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(1200, 850);
            MinimumSize = new Size(900, 600);
            Font = new Font("Segoe UI", 9F, FontStyle.Regular, GraphicsUnit.Point);

            _toolbar = new FlowLayoutPanel
            {
                Dock = DockStyle.Top,
                Height = 42,
                Padding = new Padding(5, 5, 5, 4),
                WrapContents = false,
                AutoScroll = true,
                FlowDirection = FlowDirection.LeftToRight,
            };

            var backButton = MakeButton(L("disk.back"), 76, BackClicked);
            var driveButton = MakeButton(@"C:\", 52, DriveClicked);
            var chooseButton = MakeButton(L("disk.choose_folder"), 126, ChooseClicked);
            _pathBox = new TextBox
            {
                Width = 320,
                Margin = new Padding(5, 3, 5, 3),
            };
            _scanButton = MakeButton(L("disk.scan"), 96, ScanClicked);
            _stopButton = MakeButton(L("disk.stop"), 68, StopClicked);
            var refreshButton = MakeButton(L("disk.refresh"), 86, RefreshClicked);
            _largeFilesButton = MakeButton(L("disk.large_files"), 112, LargeFilesClicked);
            _explorerButton = MakeButton(L("disk.explorer"), 96, ExplorerClicked);

            _stopButton.Enabled = false;
            _largeFilesButton.Enabled = false;
            _explorerButton.Enabled = false;

            _toolbar.Controls.Add(backButton);
            _toolbar.Controls.Add(driveButton);
            _toolbar.Controls.Add(chooseButton);
            _toolbar.Controls.Add(_pathBox);
            _toolbar.Controls.Add(_scanButton);
            _toolbar.Controls.Add(_stopButton);
            _toolbar.Controls.Add(refreshButton);
            _toolbar.Controls.Add(_largeFilesButton);
            _toolbar.Controls.Add(_explorerButton);

            _status = new Label
            {
                Dock = DockStyle.Bottom,
                Height = 26,
                Padding = new Padding(7, 4, 7, 2),
                Text = L("disk.status_ready"),
                AutoEllipsis = true,
            };

            _grid = BuildGrid();
            _grid.CellPainting += GridCellPainting;
            _grid.ColumnHeaderMouseClick += GridColumnHeaderMouseClick;
            UpdateSortGlyph();

            Controls.Add(_grid);
            Controls.Add(_status);
            Controls.Add(_toolbar);
        }

        public string InitialRoot { get; set; }
        public string SmokeReportPath { get; set; }

        protected override async void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (!string.IsNullOrWhiteSpace(InitialRoot))
            {
                _pathBox.Text = InitialRoot;
                await StartScanAsync();
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            _scanCancellation?.Cancel();
            base.OnFormClosing(e);
        }

        private DataGridView BuildGrid()
        {
            var grid = new DataGridView
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                AllowUserToResizeRows = false,
                RowHeadersVisible = false,
                MultiSelect = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                AutoGenerateColumns = false,
                BackgroundColor = SystemColors.Window,
                BorderStyle = BorderStyle.Fixed3D,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing,
                ColumnHeadersHeight = 28,
                RowTemplate = { Height = 24 },
            };

            grid.Columns.Add(Column("name", L("disk.column_name"), 330, true));
            grid.Columns.Add(Column("size", L("disk.column_size"), 110));
            grid.Columns.Add(Column("allocated", L("disk.column_allocated"), 110));
            grid.Columns.Add(Column("files", L("disk.column_files"), 82));
            grid.Columns.Add(Column("folders", L("disk.column_folders"), 82));
            grid.Columns.Add(Column("parentPercent", L("disk.column_parent_percent"), 132));
            grid.Columns.Add(Column("modified", L("disk.column_modified"), 150));
            return grid;
        }

        private static DataGridViewTextBoxColumn Column(string name, string header, int width, bool fill = false)
        {
            return new DataGridViewTextBoxColumn
            {
                Name = name,
                HeaderText = header,
                Width = width,
                MinimumWidth = Math.Min(width, 70),
                AutoSizeMode = fill ? DataGridViewAutoSizeColumnMode.Fill : DataGridViewAutoSizeColumnMode.None,
                SortMode = DataGridViewColumnSortMode.Programmatic,
            };
        }

        private static Button MakeButton(string text, int width, EventHandler click)
        {
            var button = new Button
            {
                Text = text,
                Width = width,
                Height = 28,
                Margin = new Padding(2, 0, 2, 0),
                UseVisualStyleBackColor = true,
            };
            button.Click += click;
            return button;
        }

        private string L(string key) => _language.Get(key);

        private void BackClicked(object sender, EventArgs e)
        {
            var path = string.IsNullOrWhiteSpace(_currentRoot) ? _pathBox.Text : _currentRoot;
            if (string.IsNullOrWhiteSpace(path)) return;

            try
            {
                var trimmed = path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
                var parent = Directory.GetParent(trimmed);
                if (parent == null) return;
                _pathBox.Text = parent.FullName;
                _ = StartScanAsync();
            }
            catch (Exception ex)
            {
                _status.Text = Format("disk.status_failed", ex.Message);
            }
        }

        private void DriveClicked(object sender, EventArgs e)
        {
            _pathBox.Text = @"C:\";
            _ = StartScanAsync();
        }

        private void ChooseClicked(object sender, EventArgs e)
        {
            using (var dialog = new FolderBrowserDialog())
            {
                dialog.Description = L("disk.folder_dialog");
                dialog.ShowNewFolderButton = false;
                if (!string.IsNullOrWhiteSpace(_currentRoot) && Directory.Exists(_currentRoot))
                    dialog.SelectedPath = _currentRoot;

                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    _pathBox.Text = dialog.SelectedPath;
                    _ = StartScanAsync();
                }
            }
        }

        private async void ScanClicked(object sender, EventArgs e)
        {
            await StartScanAsync();
        }

        private void StopClicked(object sender, EventArgs e)
        {
            _scanCancellation?.Cancel();
        }

        private async void RefreshClicked(object sender, EventArgs e)
        {
            await StartScanAsync();
        }

        private void LargeFilesClicked(object sender, EventArgs e)
        {
            if (_rootNode == null) return;
            _showingLargeFiles = true;
            PopulateLargeFiles();
        }

        private void ExplorerClicked(object sender, EventArgs e)
        {
            var node = SelectedNode() ?? _rootNode;
            if (node == null || string.IsNullOrWhiteSpace(node.FullPath)) return;

            try
            {
                if (node.IsDirectory)
                    Process.Start("explorer.exe", "\"" + node.FullPath + "\"");
                else
                    Process.Start("explorer.exe", "/select,\"" + node.FullPath + "\"");
            }
            catch (Exception ex)
            {
                _status.Text = Format("disk.status_failed", ex.Message);
            }
        }

        private DiskNode SelectedNode()
        {
            if (_grid.SelectedRows.Count == 0) return null;
            var row = _grid.SelectedRows[0].Tag as DiskGridRow;
            return row?.Node;
        }

        private async Task StartScanAsync()
        {
            if (_scanCancellation != null) return;

            var requested = _pathBox.Text?.Trim();
            if (string.IsNullOrWhiteSpace(requested) || !Directory.Exists(requested))
            {
                _status.Text = Format("disk.status_failed", requested ?? string.Empty);
                return;
            }

            string root;
            try
            {
                root = Path.GetFullPath(requested);
            }
            catch (Exception ex)
            {
                _status.Text = Format("disk.status_failed", ex.Message);
                return;
            }

            _currentRoot = root;
            _pathBox.Text = root;
            _scanCancellation = new CancellationTokenSource();
            _showingLargeFiles = false;
            SetBusy(true);
            _status.Text = Format("disk.status_scanning", root);

            try
            {
                var result = await _scanner.ScanAsync(root, _scanCancellation.Token, ProgressChanged);
                _rootNode = result;
                PopulateTree(result);
                _status.Text = Format(
                    "disk.status_complete",
                    result.FileCount,
                    result.FolderCount,
                    SizeFormatter.BytesText(result.LogicalBytes));
                WriteSmokeReport(result);
            }
            catch (OperationCanceledException)
            {
                _status.Text = L("disk.status_canceled");
            }
            catch (Exception ex)
            {
                _status.Text = Format("disk.status_failed", ex.Message);
            }
            finally
            {
                _scanCancellation.Dispose();
                _scanCancellation = null;
                SetBusy(false);
            }
        }

        private void WriteSmokeReport(DiskNode result)
        {
            if (string.IsNullOrWhiteSpace(SmokeReportPath)) return;

            var fullPath = Path.GetFullPath(SmokeReportPath);
            var directory = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(directory))
                Directory.CreateDirectory(directory);

            var columns = _grid.Columns
                .Cast<DataGridViewColumn>()
                .Select(column => column.HeaderText)
                .ToArray();
            var visibleToolbarControls = _toolbar.Controls
                .Cast<Control>()
                .Where(control => control.Visible)
                .ToArray();
            var toolbarContentRight = visibleToolbarControls.Length == 0
                ? 0
                : visibleToolbarControls.Max(control => control.Right + control.Margin.Right);
            var toolbarLimit = Math.Max(0, _toolbar.ClientSize.Width - _toolbar.Padding.Right);
            var toolbarOverflow = toolbarContentRight > toolbarLimit;

            var report = new Dictionary<string, object>
            {
                ["target"] = "DPopCleaner 0.4.17 Disk Analyzer",
                ["root"] = result.FullPath,
                ["logical_bytes"] = result.LogicalBytes,
                ["allocated_complete"] = result.AllocatedComplete,
                ["allocated_bytes"] = result.AllocatedBytes,
                ["file_count"] = result.FileCount,
                ["folder_count"] = result.FolderCount,
                ["row_count"] = _grid.Rows.Count,
                ["columns"] = columns,
                ["status"] = _status.Text,
                ["toolbar_overflow"] = toolbarOverflow,
                ["toolbar_client_width"] = _toolbar.ClientSize.Width,
                ["toolbar_content_right"] = toolbarContentRight,
            };

            var json = new JavaScriptSerializer().Serialize(report);
            var temporary = fullPath + ".tmp";
            File.WriteAllText(temporary, json);
            if (File.Exists(fullPath)) File.Delete(fullPath);
            File.Move(temporary, fullPath);
        }

        private void ProgressChanged(ScanProgress progress)
        {
            if (IsDisposed || !IsHandleCreated) return;
            BeginInvoke(new Action(() =>
            {
                if (IsDisposed) return;
                _status.Text = Format(
                    "disk.status_progress",
                    progress.FilesScanned,
                    progress.FoldersScanned,
                    SizeFormatter.BytesText(progress.LogicalBytesFound));
            }));
        }

        private void SetBusy(bool busy)
        {
            _scanButton.Enabled = !busy;
            _stopButton.Enabled = busy;
            _pathBox.Enabled = !busy;
            _largeFilesButton.Enabled = !busy && _rootNode != null;
            _explorerButton.Enabled = !busy && _rootNode != null;
        }

        private void GridColumnHeaderMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            if (e.ColumnIndex < 0) return;
            var column = _grid.Columns[e.ColumnIndex].Name;
            if (string.Equals(_sortColumn, column, StringComparison.OrdinalIgnoreCase))
            {
                _sortAscending = !_sortAscending;
            }
            else
            {
                _sortColumn = column;
                _sortAscending = string.Equals(column, "name", StringComparison.OrdinalIgnoreCase);
            }

            UpdateSortGlyph();
            if (_rootNode == null) return;
            if (_showingLargeFiles)
                PopulateLargeFiles();
            else
                PopulateTree(_rootNode);
        }

        private void UpdateSortGlyph()
        {
            foreach (DataGridViewColumn column in _grid.Columns)
                column.HeaderCell.SortGlyphDirection = SortOrder.None;

            var active = _grid.Columns[_sortColumn];
            if (active != null)
                active.HeaderCell.SortGlyphDirection = _sortAscending ? SortOrder.Ascending : SortOrder.Descending;
        }

        private void PopulateTree(DiskNode root)
        {
            _grid.SuspendLayout();
            try
            {
                _grid.Rows.Clear();
                AddNode(root, 0, 100.0);
                if (_grid.Rows.Count > 0)
                    _grid.Rows[0].Selected = true;
            }
            finally
            {
                _grid.ResumeLayout();
            }
        }

        private void PopulateLargeFiles()
        {
            var files = DiskTreeQuery.LargestFiles(_rootNode, 200);
            var rows = files.Select(file => new DiskGridRow(
                file,
                0,
                _rootNode.LogicalBytes <= 0 ? 0.0 : file.LogicalBytes * 100.0 / _rootNode.LogicalBytes));

            _grid.SuspendLayout();
            try
            {
                _grid.Rows.Clear();
                foreach (var row in DiskGridSorter.Sort(rows, _sortColumn, _sortAscending))
                    AddGridRow(row);
                if (_grid.Rows.Count > 0)
                    _grid.Rows[0].Selected = true;
            }
            finally
            {
                _grid.ResumeLayout();
            }

            _status.Text = L("disk.large_files") + ": " + files.Count.ToString("N0");
        }

        private void AddNode(DiskNode node, int depth, double parentPercent)
        {
            var model = new DiskGridRow(node, depth, parentPercent);
            AddGridRow(model);

            var childRows = node.Children.Select(child => new DiskGridRow(
                child,
                depth + 1,
                node.LogicalBytes <= 0 ? 0.0 : child.LogicalBytes * 100.0 / node.LogicalBytes));

            foreach (var child in DiskGridSorter.Sort(childRows, _sortColumn, _sortAscending))
                AddNode(child.Node, depth + 1, child.ParentPercent);
        }

        private void AddGridRow(DiskGridRow model)
        {
            var node = model.Node;
            var modified = node.ModifiedUtc == default(DateTime)
                ? "—"
                : node.ModifiedUtc.ToLocalTime().ToString("g");

            var index = _grid.Rows.Add(
                model.NameText,
                SizeFormatter.LogicalText(node),
                SizeFormatter.AllocatedText(node),
                node.FileCount.ToString("N0"),
                node.FolderCount.ToString("N0"),
                model.ParentPercent.ToString("0.0") + " %",
                modified);
            _grid.Rows[index].Tag = model;
        }

        private void GridCellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            if (e.RowIndex < 0 || e.ColumnIndex != _grid.Columns["parentPercent"].Index)
                return;

            var model = _grid.Rows[e.RowIndex].Tag as DiskGridRow;
            if (model == null) return;

            e.PaintBackground(e.ClipBounds, true);
            var width = Math.Max(0, (int)((e.CellBounds.Width - 4) * model.ParentPercent / 100.0));
            if (width > 0)
            {
                using (var brush = new SolidBrush(Color.FromArgb(196, 235, 196)))
                {
                    e.Graphics.FillRectangle(
                        brush,
                        e.CellBounds.X + 2,
                        e.CellBounds.Y + 3,
                        width,
                        Math.Max(1, e.CellBounds.Height - 6));
                }
            }
            e.PaintContent(e.ClipBounds);
            e.Handled = true;
        }

        private string Format(string key, params object[] args)
        {
            return string.Format(L(key), args);
        }
    }
}
