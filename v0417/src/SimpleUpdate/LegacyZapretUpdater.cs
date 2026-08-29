using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal static class LegacyZapretUpdater
    {
        private const string LatestVersionUrl = "https://raw.githubusercontent.com/Flowseal/zapret-discord-youtube/main/.service/version.txt";
        private const string LatestReleaseUrl = "https://github.com/Flowseal/zapret-discord-youtube/releases/latest";

        internal static void Run(string applicationRoot)
        {
            var root = Path.GetFullPath(applicationRoot ?? string.Empty);

            // Runtime smoke seam: when explicitly requested by CI, prove that the visible
            // Zapret update button reached this bridge-owned handler without opening UI/network.
            var smokeMarker = Environment.GetEnvironmentVariable("DPOPCLEANER_ZAPRET_UPDATE_SMOKE_MARKER");
            if (!string.IsNullOrWhiteSpace(smokeMarker))
            {
                var markerPath = Path.GetFullPath(smokeMarker);
                var markerDirectory = Path.GetDirectoryName(markerPath);
                if (!string.IsNullOrWhiteSpace(markerDirectory)) Directory.CreateDirectory(markerDirectory);
                File.WriteAllText(markerPath, "BRIDGE_ZAPRET_UPDATER_OK" + Environment.NewLine);
                return;
            }

            var zapretRoot = Path.Combine(root, "Zapret");
            var servicePath = Path.Combine(zapretRoot, "service.bat");
            var localVersionPath = Path.Combine(zapretRoot, ".service", "version.txt");

            if (!File.Exists(servicePath))
            {
                MessageBox.Show(
                    "Компоненты Zapret не найдены. Переустановите DPopCleaner, чтобы восстановить комплект Flowseal Zapret.",
                    "DPopCleaner — обновление Zapret",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            var localVersion = ReadLocalVersion(localVersionPath);
            try
            {
                string latestVersion;
                using (var http = new HttpClient { Timeout = TimeSpan.FromSeconds(8) })
                {
                    http.DefaultRequestHeaders.TryAddWithoutValidation("User-Agent", "DPopCleaner-DPopUpdate/0.4.17-rev9");
                    latestVersion = (http.GetStringAsync(LatestVersionUrl).GetAwaiter().GetResult() ?? string.Empty).Trim();
                }

                if (string.IsNullOrWhiteSpace(latestVersion))
                    throw new InvalidDataException("Официальный сервер не вернул номер версии.");

                if (string.Equals(localVersion, latestVersion, StringComparison.OrdinalIgnoreCase))
                {
                    MessageBox.Show(
                        "Установлена актуальная версия Flowseal Zapret " + latestVersion + ".",
                        "DPopCleaner — обновление Zapret",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information);
                    return;
                }

                var installedText = string.IsNullOrWhiteSpace(localVersion) ? "не определена" : localVersion;
                var answer = MessageBox.Show(
                    "Установленная версия Zapret: " + installedText + ".\r\n" +
                    "Актуальная версия Flowseal: " + latestVersion + ".\r\n\r\n" +
                    "Открыть официальную страницу обновления Flowseal Zapret?",
                    "DPopCleaner — обновление Zapret",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information,
                    MessageBoxDefaultButton.Button1);
                if (answer == DialogResult.Yes) OpenOfficialRelease();
            }
            catch (Exception ex)
            {
                var answer = MessageBox.Show(
                    "Не удалось автоматически проверить версию Zapret.\r\n\r\n" + ex.Message +
                    "\r\n\r\nОткрыть официальный менеджер Flowseal Zapret?",
                    "DPopCleaner — обновление Zapret",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Warning,
                    MessageBoxDefaultButton.Button1);
                if (answer == DialogResult.Yes) OpenManager(servicePath, zapretRoot);
            }
        }

        private static string ReadLocalVersion(string path)
        {
            try
            {
                return File.Exists(path) ? (File.ReadAllText(path) ?? string.Empty).Trim() : string.Empty;
            }
            catch
            {
                return string.Empty;
            }
        }

        private static void OpenOfficialRelease()
        {
            var info = new ProcessStartInfo(LatestReleaseUrl) { UseShellExecute = true };
            if (Process.Start(info) == null)
                throw new InvalidOperationException("Не удалось открыть официальную страницу обновления Flowseal Zapret.");
        }

        private static void OpenManager(string servicePath, string zapretRoot)
        {
            var info = new ProcessStartInfo(servicePath)
            {
                UseShellExecute = true,
                WorkingDirectory = zapretRoot
            };
            if (Process.Start(info) == null)
                throw new InvalidOperationException("Не удалось запустить официальный менеджер Flowseal Zapret.");
        }
    }
}
