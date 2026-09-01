using System;
using System.IO;

namespace DPopCleaner.SimpleUpdate
{
    internal static class BridgeDiagnostics
    {
        private const string EnabledVariable = "DPOP_BRIDGE_DIAGNOSTICS";
        private const string EnabledMarkerName = "DPopCleaner-bridge-diagnostics.enabled";
        private const string LogFileName = "DPopCleaner-bridge-diagnostics.log";
        private static readonly object Sync = new object();
        private static string _lastState;

        internal static void Record(Exception exception)
        {
            if (exception == null) return;
            Write("EXCEPTION " + exception);
        }

        internal static void RecordState(string state)
        {
            if (string.IsNullOrWhiteSpace(state)) return;
            lock (Sync)
            {
                if (string.Equals(_lastState, state, StringComparison.Ordinal)) return;
                _lastState = state;
            }
            Write("STATE " + state);
        }

        private static void Write(string message)
        {
            string path;
            if (!TryGetLogPath(out path)) return;

            try
            {
                lock (Sync)
                {
                    File.AppendAllText(
                        path,
                        DateTime.UtcNow.ToString("O") + " " + message + Environment.NewLine);
                }
            }
            catch
            {
                // Diagnostics must never affect the authentic frozen core or bridge lifecycle.
            }
        }

        private static bool TryGetLogPath(out string path)
        {
            path = null;
            try
            {
                var baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
                var marker = Path.Combine(baseDirectory, EnabledMarkerName);
                var envEnabled = string.Equals(
                    Environment.GetEnvironmentVariable(EnabledVariable),
                    "1",
                    StringComparison.Ordinal);
                if (!envEnabled && !File.Exists(marker)) return false;

                path = Path.Combine(baseDirectory, LogFileName);
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
