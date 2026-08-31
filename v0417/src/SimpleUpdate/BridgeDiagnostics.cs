using System;
using System.IO;

namespace DPopCleaner.SimpleUpdate
{
    internal static class BridgeDiagnostics
    {
        private const string EnabledVariable = "DPOP_BRIDGE_DIAGNOSTICS";
        private const string LogFileName = "DPopCleaner-bridge-diagnostics.log";

        internal static void Record(Exception exception)
        {
            if (exception == null) return;
            if (!string.Equals(Environment.GetEnvironmentVariable(EnabledVariable), "1", StringComparison.Ordinal)) return;

            try
            {
                var path = Path.Combine(Path.GetTempPath(), LogFileName);
                File.AppendAllText(
                    path,
                    DateTime.UtcNow.ToString("O") + " " + exception + Environment.NewLine + Environment.NewLine);
            }
            catch
            {
                // Diagnostics must never affect the authentic frozen core or bridge lifecycle.
            }
        }
    }
}
