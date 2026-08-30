using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Security.Principal;
using System.Text;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal static class ElevationBootstrap
    {
        internal static bool EnsureAdministrator(string[] args)
        {
            if (IsAdministrator()) return true;

            var startInfo = new ProcessStartInfo(Application.ExecutablePath)
            {
                UseShellExecute = true,
                Verb = "runas",
                WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory,
                Arguments = BuildArguments(args)
            };

            try
            {
                Process.Start(startInfo);
            }
            catch (Win32Exception ex)
            {
                // 1223 = ERROR_CANCELLED. The user declined the UAC prompt; exit quietly.
                if (ex.NativeErrorCode != 1223) throw;
            }
            return false;
        }

        internal static bool IsAdministrator()
        {
            using (var identity = WindowsIdentity.GetCurrent())
            {
                var principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
        }

        internal static string BuildArguments(string[] args)
        {
            if (args == null || args.Length == 0) return string.Empty;
            var result = new StringBuilder();
            for (var i = 0; i < args.Length; i++)
            {
                if (i > 0) result.Append(' ');
                result.Append(QuoteArgument(args[i] ?? string.Empty));
            }
            return result.ToString();
        }

        private static string QuoteArgument(string value)
        {
            if (value.Length > 0 && value.IndexOfAny(new[] { ' ', '\t', '\n', '\v', '"' }) < 0)
                return value;

            var result = new StringBuilder();
            result.Append('"');
            var slashes = 0;
            foreach (var ch in value)
            {
                if (ch == '\\')
                {
                    slashes++;
                    continue;
                }
                if (ch == '"')
                {
                    result.Append('\\', slashes * 2 + 1);
                    result.Append('"');
                    slashes = 0;
                    continue;
                }
                if (slashes > 0)
                {
                    result.Append('\\', slashes);
                    slashes = 0;
                }
                result.Append(ch);
            }
            if (slashes > 0) result.Append('\\', slashes * 2);
            result.Append('"');
            return result.ToString();
        }
    }
}
