using System;
using System.IO;
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
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            var moduleDirectory = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var parent = Directory.GetParent(moduleDirectory);
            var installRoot = parent != null && string.Equals(parent.Name, "Modules", StringComparison.OrdinalIgnoreCase)
                ? parent.Parent?.FullName ?? moduleDirectory
                : moduleDirectory;

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
