using System;
using System.Diagnostics;
using System.IO;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretRuntimeState
    {
        internal bool BundledWinwsRunning { get; private set; }
        internal bool ZapretServiceExists { get; private set; }
        internal bool ZapretServiceRunning { get; private set; }
        internal bool WinDivertServiceExists { get; private set; }
        internal bool WinDivert14ServiceExists { get; private set; }
        internal string BundledWinwsCommandLine { get; private set; }

        private ZapretRuntimeState()
        {
            BundledWinwsCommandLine = string.Empty;
        }

        internal static ZapretRuntimeState Read(string applicationRoot)
        {
            var state = new ZapretRuntimeState();
            var root = Path.GetFullPath(applicationRoot ?? string.Empty);
            var bundledWinws = Path.GetFullPath(Path.Combine(root, "Zapret", "bin", "winws.exe"));

            foreach (var process in Process.GetProcessesByName("winws"))
            {
                try
                {
                    process.Refresh();
                    if (process.HasExited || process.MainModule == null) continue;
                    var executable = process.MainModule.FileName;
                    if (!string.Equals(Path.GetFullPath(executable), bundledWinws, StringComparison.OrdinalIgnoreCase)) continue;
                    state.BundledWinwsRunning = true;
                    state.BundledWinwsCommandLine = executable;
                    break;
                }
                catch
                {
                    // A disappearing or inaccessible process is not factual evidence that bundled winws is running.
                }
                finally
                {
                    process.Dispose();
                }
            }

            var service = QueryService("zapret");
            state.ZapretServiceExists = service.Exists;
            state.ZapretServiceRunning = service.Running;
            state.WinDivertServiceExists = QueryService("WinDivert").Exists;
            state.WinDivert14ServiceExists = QueryService("WinDivert14").Exists;
            return state;
        }

        private static ServiceState QueryService(string name)
        {
            var info = new ProcessStartInfo("sc.exe", "query \"" + name + "\"")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            using (var process = Process.Start(info))
            {
                if (process == null) return new ServiceState();
                var output = process.StandardOutput.ReadToEnd();
                var error = process.StandardError.ReadToEnd();
                if (!process.WaitForExit(5000))
                {
                    try { process.Kill(); } catch { }
                    return new ServiceState();
                }

                var text = (output ?? string.Empty) + Environment.NewLine + (error ?? string.Empty);
                var missing = text.IndexOf("1060", StringComparison.OrdinalIgnoreCase) >= 0 ||
                              text.IndexOf("does not exist", StringComparison.OrdinalIgnoreCase) >= 0;
                var exists = !missing && text.IndexOf("SERVICE_NAME", StringComparison.OrdinalIgnoreCase) >= 0;
                var running = exists && text.IndexOf("RUNNING", StringComparison.OrdinalIgnoreCase) >= 0;
                return new ServiceState { Exists = exists, Running = running };
            }
        }

        private sealed class ServiceState
        {
            internal bool Exists;
            internal bool Running;
        }
    }
}
