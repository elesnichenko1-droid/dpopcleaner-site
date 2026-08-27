using System;
using System.IO;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal static class Program
    {
        internal const int CurrentVersionCode = 417;
        internal const int CurrentRevision = 3;
        internal const string StableManifestUrl = "https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json";

        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            var baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
            var corePath = Path.Combine(baseDirectory, "DPopCleaner.exe");
            var settingsPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "DPopCleaner", "SimpleUpdate.ini");

            for (var i = 0; i < args.Length; i++)
            {
                if (string.Equals(args[i], "--settings-path", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                    settingsPath = args[++i];
            }

            try
            {
                using (var mutex = new System.Threading.Mutex(false, "DPopCleaner.SimpleUpdate.Launcher"))
                {
                    bool acquired;
                    try { acquired = mutex.WaitOne(0, false); }
                    catch (System.Threading.AbandonedMutexException) { acquired = true; }
                    if (!acquired)
                    {
                        // Let the already running launcher own the bridge; launch the core normally.
                        System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(corePath) { UseShellExecute = true, WorkingDirectory = baseDirectory });
                        return;
                    }

                    var context = new LauncherContext(corePath, settingsPath);
                    Application.Run(context);
                    try { mutex.ReleaseMutex(); } catch { }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Не удалось запустить DPopCleaner.\r\n\r\n" + ex.Message,
                    "DPopCleaner", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
