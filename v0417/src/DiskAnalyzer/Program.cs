using System;
using System.Globalization;
using System.IO;
using System.Windows.Forms;
using DPop.Common;
using DPop.Common.Localization;
using DPop.DiskAnalyzer.Scanning;
using DPop.DiskAnalyzer.UI;

namespace DPop.DiskAnalyzer
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            var root = GetArgument(args, "--root");
            var smokeReport = GetArgument(args, "--smoke-report");
            var languageCode = GetArgument(args, "--lang")
                ?? CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;

            var moduleDirectory = AppDomain.CurrentDomain.BaseDirectory
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var installRoot = Directory.GetParent(moduleDirectory)?.FullName ?? moduleDirectory;
            var paths = new AppPaths(installRoot);
            var language = LanguageCatalog.Load(paths.LanguagesDirectory, languageCode);
            var scanner = new DiskScanner(new WindowsAllocationSizeProvider());

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            var form = new DiskAnalyzerForm(language, scanner)
            {
                InitialRoot = root,
                SmokeReportPath = smokeReport,
            };
            Application.Run(form);
        }

        private static string GetArgument(string[] args, string name)
        {
            if (args == null) return null;
            for (var i = 0; i < args.Length - 1; i++)
            {
                if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase))
                    return args[i + 1];
            }
            return null;
        }
    }
}
