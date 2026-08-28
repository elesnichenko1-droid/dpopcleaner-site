using System;
using System.IO;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal static class Program
    {
        internal const int CurrentVersionCode = 417;
        internal const int CurrentRevision = 6;
        internal const string StableManifestUrl = LauncherOptions.DefaultManifestUrl;

        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            try
            {
                var options = LauncherOptions.Parse(args);
                var baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
                var corePath = Path.Combine(baseDirectory, "DPopCleaner.Core.exe");
                var settingsPath = options.SettingsPathOverride ?? Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "DPopCleaner", "SimpleUpdate.ini");

                using (var mutex = new System.Threading.Mutex(false, "DPopCleaner.SimpleUpdate.Launcher"))
                {
                    bool acquired;
                    try { acquired = mutex.WaitOne(0, false); }
                    catch (System.Threading.AbandonedMutexException) { acquired = true; }
                    if (!acquired)
                    {
                        // Never start the frozen core without its UI bridge.
                        return;
                    }

                    var context = new LauncherContext(corePath, settingsPath, options);
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
