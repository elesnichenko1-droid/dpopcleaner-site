using System;
using System.IO;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal static class Program
    {
        internal const int CurrentVersionCode = 417;
        internal const int CurrentRevision = 9;
        internal const string StableManifestUrl = LauncherOptions.DefaultManifestUrl;

        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            try
            {
                var baseDirectory = AppDomain.CurrentDomain.BaseDirectory;

                if (string.Equals(Path.GetFileName(Application.ExecutablePath), "DPopUpdate.exe", StringComparison.OrdinalIgnoreCase))
                {
                    LegacyZapretUpdater.Run(baseDirectory);
                    return;
                }

                var options = LauncherOptions.Parse(args);
                var corePath = Path.Combine(baseDirectory, "DPopCleaner.Core.exe");
                var settingsPath = options.SettingsPathOverride ?? Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "DPopCleaner", "SimpleUpdate.ini");

                using (var mutex = new System.Threading.Mutex(false, "DPopCleaner.SimpleUpdate.Launcher"))
                {
                    bool acquired;
                    try { acquired = mutex.WaitOne(0, false); }
                    catch (System.Threading.AbandonedMutexException) { acquired = true; }
                    if (!acquired) return;

                    var context = new LauncherContext(corePath, settingsPath, options);
                    ZapretUpdateProxyHost zapretUpdateProxy = null;
                    var zapretProxyTimer = new System.Windows.Forms.Timer { Interval = 100 };
                    zapretProxyTimer.Tick += delegate
                    {
                        try
                        {
                            var core = context.CoreProcess;
                            core.Refresh();
                            if (core.HasExited) return;
                            var mainWindow = core.MainWindowHandle;
                            if (mainWindow == IntPtr.Zero) return;

                            var anchor = NativeBridge.FindChildById(mainWindow, NativeBridge.ZapretCheckVersionButtonId);
                            var visible = anchor != IntPtr.Zero && NativeBridge.IsWindowVisible(anchor);
                            if (!visible)
                            {
                                if (zapretUpdateProxy != null) zapretUpdateProxy.Hide();
                                return;
                            }

                            if (zapretUpdateProxy == null)
                                zapretUpdateProxy = new ZapretUpdateProxyHost(mainWindow, baseDirectory);
                            else
                                zapretUpdateProxy.Show();
                        }
                        catch
                        {
                            // A proxy failure must never terminate the frozen core or launcher.
                        }
                    };
                    zapretProxyTimer.Start();

                    Application.Run(context);

                    zapretProxyTimer.Stop();
                    if (zapretUpdateProxy != null) zapretUpdateProxy.Dispose();
                    zapretProxyTimer.Dispose();
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
