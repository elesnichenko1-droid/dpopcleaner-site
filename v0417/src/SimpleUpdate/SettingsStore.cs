using System;
using System.IO;

namespace DPopCleaner.SimpleUpdate
{
    public sealed class SettingsStore
    {
        private readonly string _path;

        public SettingsStore(string path)
        {
            if (string.IsNullOrWhiteSpace(path)) throw new ArgumentException("Settings path is required.", nameof(path));
            _path = path;
        }

        public bool LoadAutoUpdateEnabled()
        {
            if (!File.Exists(_path)) return true;
            foreach (var raw in File.ReadAllLines(_path))
            {
                var line = raw.Trim();
                if (line.StartsWith("auto_update=", StringComparison.OrdinalIgnoreCase))
                {
                    var value = line.Substring("auto_update=".Length).Trim();
                    if (value == "0" || value.Equals("false", StringComparison.OrdinalIgnoreCase)) return false;
                    if (value == "1" || value.Equals("true", StringComparison.OrdinalIgnoreCase)) return true;
                }
            }
            return true;
        }

        public void SaveAutoUpdateEnabled(bool enabled)
        {
            var directory = Path.GetDirectoryName(_path);
            if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
            File.WriteAllText(_path, "auto_update=" + (enabled ? "1" : "0") + Environment.NewLine);
        }
    }
}
