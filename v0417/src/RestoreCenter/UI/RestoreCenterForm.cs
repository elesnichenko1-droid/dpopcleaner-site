using System;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using DPop.Common.History;
using DPop.Common.Localization;
using DPop.Common.Restore;

namespace DPop.RestoreCenter.UI
{
    public sealed class RestoreCenterForm : Form
    {
        private readonly LanguageCatalog _language;
        private readonly HistoryStore _store;
        private readonly RestoreCoordinator _coordinator;
        private readonly DataGridView _grid;
        private readonly TextBox _details;
        private readonly Button _restoreButton;
        private readonly Label _status;

        public RestoreCenterForm(LanguageCatalog language, HistoryStore store, RestoreCoordinator coordinator)
        {
            _language = language ?? throw new ArgumentNullException(nameof(language));
            _store = store ?? throw new ArgumentNullException(nameof(store));
            _coordinator = coordinator ?? throw new ArgumentNullException(nameof(coordinator));

            Text = "DPopCleaner 0.4.17 — " + L("restore.title");
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(1050, 700);
            MinimumSize = new Size(900, 600);
            Font = new Font("Segoe UI", 9F, FontStyle.Regular, GraphicsUnit.Point);

            var toolbar = new FlowLayoutPanel
            {
                Dock = DockStyle.Top,
                Height = 42,
                Padding = new Padding(6, 6, 6, 5),
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = false,
            };
            toolbar.Controls.Add(MakeButton(L("restore.refresh"), 100, RefreshClicked));
            _restoreButton = MakeButton(L("restore.rollback"), 115, RestoreClicked);
            _restoreButton.Enabled = false;
            toolbar.Controls.Add(_restoreButton);
            toolbar.Controls.Add(MakeButton(L("common.close"), 90, (s, e) => Close()));

            _grid = new DataGridView
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                AllowUserToResizeRows = false,
                AutoGenerateColumns = false,
                RowHeadersVisible = false,
                MultiSelect = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                BackgroundColor = SystemColors.Window,
                BorderStyle = BorderStyle.Fixed3D,
            };
            _grid.Columns.Add(Column("date", L("restore.column.date"), 145));
            _grid.Columns.Add(Column("action", L("restore.column.action"), 180));
            _grid.Columns.Add(Column("target", L("restore.column.target"), 330, true));
            _grid.Columns.Add(Column("status", L("restore.column.status"), 135));
            _grid.Columns.Add(Column("rollback", L("restore.column.rollback"), 125));
            _grid.SelectionChanged += SelectionChanged;

            _details = new TextBox
            {
                Dock = DockStyle.Bottom,
                Height = 130,
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = SystemColors.Window,
            };

            _status = new Label
            {
                Dock = DockStyle.Bottom,
                Height = 27,
                Padding = new Padding(7, 5, 7, 2),
                AutoEllipsis = true,
                Text = L("restore.status.ready"),
            };

            Controls.Add(_grid);
            Controls.Add(_details);
            Controls.Add(_status);
            Controls.Add(toolbar);
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            ReloadHistory();
        }

        private static DataGridViewTextBoxColumn Column(string name, string text, int width, bool fill = false)
        {
            return new DataGridViewTextBoxColumn
            {
                Name = name,
                HeaderText = text,
                Width = width,
                MinimumWidth = Math.Min(width, 80),
                AutoSizeMode = fill ? DataGridViewAutoSizeColumnMode.Fill : DataGridViewAutoSizeColumnMode.None,
                SortMode = DataGridViewColumnSortMode.NotSortable,
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

        private void RefreshClicked(object sender, EventArgs e)
        {
            ReloadHistory();
        }

        private void RestoreClicked(object sender, EventArgs e)
        {
            var record = SelectedRecord();
            if (record == null || !record.RollbackAvailable) return;

            var answer = MessageBox.Show(
                this,
                string.Format(L("restore.confirm"), record.Description ?? record.OperationId),
                L("restore.title"),
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2);
            if (answer != DialogResult.Yes) return;

            _restoreButton.Enabled = false;
            _status.Text = L("restore.status.working");
            try
            {
                var result = _coordinator.Restore(record.Id);
                _status.Text = result.Success
                    ? L("restore.status.success")
                    : string.Format(L("restore.status.failed"), result.Code);
                ReloadHistory();
            }
            catch (Exception ex)
            {
                _status.Text = string.Format(L("restore.status.failed"), ex.Message);
            }
        }

        private void ReloadHistory()
        {
            _grid.SuspendLayout();
            try
            {
                _grid.Rows.Clear();
                foreach (var record in _store.ReadAll())
                {
                    var rollbackText = record.RollbackAvailable
                        ? L("restore.available")
                        : L("restore.unavailable");
                    var index = _grid.Rows.Add(
                        record.TimestampUtc.ToLocalTime().ToString("g"),
                        record.Description ?? record.OperationId,
                        record.Target ?? string.Empty,
                        record.RollbackStatus ?? string.Empty,
                        rollbackText);
                    _grid.Rows[index].Tag = new HistoryRow(record);
                }
                if (_grid.Rows.Count > 0)
                    _grid.Rows[0].Selected = true;
            }
            finally
            {
                _grid.ResumeLayout();
            }
            UpdateSelection();
        }

        private void SelectionChanged(object sender, EventArgs e)
        {
            UpdateSelection();
        }

        private void UpdateSelection()
        {
            var record = SelectedRecord();
            _restoreButton.Enabled = record != null && record.RollbackAvailable;
            if (record == null)
            {
                _details.Clear();
                return;
            }

            var text = new StringBuilder();
            text.AppendLine(L("restore.details.operation") + ": " + (record.OperationId ?? string.Empty));
            text.AppendLine(L("restore.details.target") + ": " + (record.Target ?? string.Empty));
            text.AppendLine(L("restore.details.status") + ": " + (record.RollbackStatus ?? string.Empty));
            text.AppendLine(L("restore.details.backup") + ": " + (record.BackupReference ?? L("restore.none")));
            text.AppendLine(L("restore.details.id") + ": " + record.Id.ToString("D"));
            _details.Text = text.ToString();
        }

        private HistoryRecord SelectedRecord()
        {
            if (_grid.SelectedRows.Count == 0) return null;
            return (_grid.SelectedRows[0].Tag as HistoryRow)?.Record;
        }
    }
}
