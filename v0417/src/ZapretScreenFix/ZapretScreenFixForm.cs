using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DPop.ZapretScreenFix
{
    public sealed class ZapretScreenFixForm : Form
    {
        private readonly TextBox _root = new TextBox();
        private readonly ListBox _files = new ListBox();
        private readonly Label _status = new Label();

        public ZapretScreenFixForm()
        {
            Text = "DPopCleaner 0.4.17 — Zapret Screen Fix";
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(780, 500);
            Size = new Size(920, 590);
            BackColor = Color.FromArgb(7, 17, 31);
            ForeColor = Color.FromArgb(225, 236, 244);
            Font = new Font("Segoe UI", 9F);

            var title = new Label { Text = "Zapret · фикс демонстрации экрана", AutoSize = true, Font = new Font("Segoe UI Semibold", 18F), Location = new Point(24, 20), ForeColor = Color.FromArgb(151, 255, 210) };
            var hint = new Label { Text = "Добавляет TCP 443 только в стратегию discord.media. Перед изменением создаётся резервная копия.", AutoSize = true, Location = new Point(28, 62), ForeColor = Color.FromArgb(174, 192, 205) };
            _root.SetBounds(28, 100, 700, 28);
            _root.Text = TryDetectZapretRoot() ?? string.Empty;
            var browse = Button("Обзор…", 742, 98, 130, 30, Browse);
            var scan = Button("Проверить", 28, 145, 150, 34, (_, __) => Scan());
            var apply = Button("Применить фикс", 188, 145, 170, 34, (_, __) => Apply());
            var restore = Button("Откатить", 368, 145, 150, 34, (_, __) => Restore());
            var service = Button("Открыть service.bat", 528, 145, 190, 34, (_, __) => OpenServiceBat());

            _files.SetBounds(28, 198, 844, 292);
            _files.BackColor = Color.FromArgb(12, 28, 45);
            _files.ForeColor = ForeColor;
            _files.BorderStyle = BorderStyle.FixedSingle;
            _status.SetBounds(28, 510, 844, 28);
            _status.ForeColor = Color.FromArgb(174, 192, 205);
            _status.Text = "Укажите папку Zapret и нажмите «Проверить».";

            Controls.AddRange(new Control[] { title, hint, _root, browse, scan, apply, restore, service, _files, _status });
            if (!string.IsNullOrWhiteSpace(_root.Text)) Scan();
        }

        private Button Button(string text, int x, int y, int w, int h, EventHandler click)
        {
            var button = new Button { Text = text, Bounds = new Rectangle(x, y, w, h), FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(15, 49, 61), ForeColor = Color.FromArgb(177, 255, 221) };
            button.FlatAppearance.BorderColor = Color.FromArgb(72, 160, 132);
            button.Click += click;
            return button;
        }

        private void Browse(object sender, EventArgs e)
        {
            using (var dialog = new FolderBrowserDialog { Description = "Выберите корневую папку zapret-discord-youtube" })
            {
                if (dialog.ShowDialog(this) == DialogResult.OK) { _root.Text = dialog.SelectedPath; Scan(); }
            }
        }

        private void Scan()
        {
            _files.Items.Clear();
            try
            {
                var candidates = ZapretStrategyPatcher.FindCandidates(_root.Text);
                foreach (var file in candidates) _files.Items.Add(file);
                _status.Text = candidates.Count == 0 ? "Стратегии, требующие фикса, не найдены — возможно, 443 уже добавлен." : $"Найдено стратегий для исправления: {candidates.Count}.";
            }
            catch (Exception ex) { _status.Text = "Ошибка проверки: " + ex.Message; }
        }

        private void Apply()
        {
            var paths = _files.Items.Cast<string>().ToArray();
            var changed = 0;
            foreach (var path in paths) if (ZapretStrategyPatcher.PatchFile(path)) changed++;
            Scan();
            _status.Text = changed == 0 ? "Изменения не требуются." : $"Исправлено файлов: {changed}. Если Zapret установлен как служба, переустановите текущую стратегию через service.bat.";
        }

        private void Restore()
        {
            var root = _root.Text;
            if (!Directory.Exists(root)) return;
            var restored = 0;
            foreach (var backup in Directory.EnumerateFiles(root, "*.bat" + ZapretStrategyPatcher.BackupSuffix, SearchOption.AllDirectories))
            {
                var path = backup.Substring(0, backup.Length - ZapretStrategyPatcher.BackupSuffix.Length);
                if (ZapretStrategyPatcher.RestoreFile(path)) restored++;
            }
            Scan();
            _status.Text = $"Восстановлено из резервных копий: {restored}.";
        }

        private void OpenServiceBat()
        {
            try
            {
                var serviceBat = Directory.EnumerateFiles(_root.Text, "service.bat", SearchOption.TopDirectoryOnly).FirstOrDefault();
                if (serviceBat == null) { _status.Text = "service.bat не найден в выбранной папке."; return; }
                Process.Start(new ProcessStartInfo(serviceBat) { UseShellExecute = true });
            }
            catch (Exception ex) { _status.Text = "Не удалось открыть service.bat: " + ex.Message; }
        }

        private static string TryDetectZapretRoot()
        {
            try
            {
                using (var key = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Services\zapret"))
                {
                    var imagePath = key?.GetValue("ImagePath") as string;
                    if (string.IsNullOrWhiteSpace(imagePath)) return null;
                    var marker = imagePath.IndexOf("winws.exe", StringComparison.OrdinalIgnoreCase);
                    if (marker < 0) return null;

                    // Prefix is the directory that contains winws.exe, normally
                    // <zapret-root>\bin\. Removing the final \bin yields the root.
                    var prefix = imagePath.Substring(0, marker).Trim().Trim('"');
                    var root = Path.GetDirectoryName(prefix.TrimEnd('\\'));
                    return root != null && Directory.Exists(root) ? root : null;
                }
            }
            catch { return null; }
        }
    }
}
