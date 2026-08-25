using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using DPop.Common.History;
using DPop.Common.Localization;
using DPop.Common.Restore;
using DPop.RestoreCenter.UI;

namespace DPop.RestoreCenter
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            var smokeExit = TryRunSmokeMode(args);
            if (smokeExit.HasValue) return smokeExit.Value;

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            var installRoot = ResolveInstallRoot();
            var languageCode = ReadArgument(args, "--lang") ?? "ru";
            var language = LanguageCatalog.Load(Path.Combine(installRoot, "Languages"), languageCode);
            var documentationRoot = Path.Combine(installRoot, "Documentation");
            var history = new HistoryStore(Path.Combine(documentationRoot, "History"));
            var backups = new BackupStore(Path.Combine(documentationRoot, "Backups"));

            var settingsRoot = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "DPopCleaner");
            Directory.CreateDirectory(settingsRoot);

            var coordinator = new RestoreCoordinator(
                history,
                backups,
                new IRestoreProvider[]
                {
                    new FileStateProvider(settingsRoot),
                    new HkcuRegistryValueProvider(),
                });

            Application.Run(new RestoreCenterForm(language, history, coordinator));
            return 0;
        }

        private static int? TryRunSmokeMode(string[] args)
        {
            if (args == null || args.Length == 0) return null;

            if (string.Equals(args[0], "--smoke-create-file-record", StringComparison.OrdinalIgnoreCase))
            {
                if (args.Length != 3 || !SmokePathsAllowed(args[1], args[2])) return 90;
                return SmokeCreateFileRecord(args[1], args[2], rollbackAvailable: true);
            }

            if (string.Equals(args[0], "--smoke-create-nonreversible", StringComparison.OrdinalIgnoreCase))
            {
                if (args.Length != 3 || !SmokePathsAllowed(args[1], args[2])) return 90;
                return SmokeCreateFileRecord(args[1], args[2], rollbackAvailable: false);
            }

            if (string.Equals(args[0], "--smoke-restore-latest", StringComparison.OrdinalIgnoreCase))
            {
                if (args.Length != 2 || !SmokeDocumentationAllowed(args[1])) return 90;
                return SmokeRestoreLatest(args[1]);
            }

            return null;
        }

        private static int SmokeCreateFileRecord(string documentationRoot, string target, bool rollbackAvailable)
        {
            try
            {
                var history = new HistoryStore(Path.Combine(documentationRoot, "History"));
                var backups = new BackupStore(Path.Combine(documentationRoot, "Backups"));
                var targetDirectory = Path.GetDirectoryName(Path.GetFullPath(target));
                var provider = new FileStateProvider(targetDirectory);

                var record = HistoryRecord.Create(
                    rollbackAvailable ? "settings.file" : "cleanup.temp",
                    Path.GetFullPath(target),
                    rollbackAvailable);
                record.Description = rollbackAvailable ? "reversible-roundtrip" : "nonreversible";

                if (rollbackAvailable)
                {
                    var state = provider.Capture(record.Target);
                    record.BeforeState = state;
                    record.BackupReference = backups.SaveBytes("Settings", Encoding.UTF8.GetBytes(state));
                    record.RollbackStatus = "available";
                }
                else
                {
                    record.RollbackStatus = "unavailable";
                }

                history.Append(record);
                return 0;
            }
            catch
            {
                return 91;
            }
        }

        private static int SmokeRestoreLatest(string documentationRoot)
        {
            try
            {
                var history = new HistoryStore(Path.Combine(documentationRoot, "History"));
                var record = history.ReadAll().FirstOrDefault();
                if (record == null) return 92;

                if (!record.RollbackAvailable) return 4;
                if (!string.Equals(record.OperationId, "settings.file", StringComparison.Ordinal)) return 93;

                var targetDirectory = Path.GetDirectoryName(Path.GetFullPath(record.Target));
                if (!SmokePathsAllowed(documentationRoot, record.Target)) return 90;

                var backups = new BackupStore(Path.Combine(documentationRoot, "Backups"));
                var coordinator = new RestoreCoordinator(
                    history,
                    backups,
                    new IRestoreProvider[] { new FileStateProvider(targetDirectory) });
                var result = coordinator.Restore(record.Id);
                return result.Success ? 0 : 94;
            }
            catch
            {
                return 95;
            }
        }

        private static bool SmokePathsAllowed(string documentationRoot, string target)
        {
            if (!SmokeDocumentationAllowed(documentationRoot)) return false;
            if (!string.Equals(Environment.GetEnvironmentVariable("DPOP0417_SMOKE"), "1", StringComparison.Ordinal))
                return false;

            try
            {
                var temp = NormalizeDirectory(Path.GetTempPath());
                var fullTarget = Path.GetFullPath(target);
                return IsInside(temp, fullTarget) &&
                       fullTarget.IndexOf("dpop0417-restore-smoke", StringComparison.OrdinalIgnoreCase) >= 0;
            }
            catch
            {
                return false;
            }
        }

        private static bool SmokeDocumentationAllowed(string documentationRoot)
        {
            if (!string.Equals(Environment.GetEnvironmentVariable("DPOP0417_SMOKE"), "1", StringComparison.Ordinal))
                return false;

            try
            {
                var temp = NormalizeDirectory(Path.GetTempPath());
                var fullDocumentation = Path.GetFullPath(documentationRoot);
                return IsInside(temp, fullDocumentation) &&
                       fullDocumentation.IndexOf("dpop0417-restore-smoke", StringComparison.OrdinalIgnoreCase) >= 0;
            }
            catch
            {
                return false;
            }
        }

        private static bool IsInside(string root, string path)
        {
            var prefix = root.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal)
                ? root
                : root + Path.DirectorySeparatorChar;
            return path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase);
        }

        private static string NormalizeDirectory(string path)
        {
            return Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        }

        private static string ResolveInstallRoot()
        {
            var moduleDirectory = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var parent = Directory.GetParent(moduleDirectory);
            return parent != null && string.Equals(parent.Name, "Modules", StringComparison.OrdinalIgnoreCase)
                ? parent.Parent?.FullName ?? moduleDirectory
                : moduleDirectory;
        }

        private static string ReadArgument(string[] args, string name)
        {
            if (args == null) return null;
            for (var i = 0; i + 1 < args.Length; i++)
            {
                if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase))
                    return args[i + 1];
            }
            return null;
        }
    }
}
