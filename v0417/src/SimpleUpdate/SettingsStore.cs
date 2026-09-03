using System;
using System.Collections.Generic;
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
            var value = LoadBoolean("auto_update");
            return value ?? true;
        }

        public void SaveAutoUpdateEnabled(bool enabled)
        {
            SaveBoolean("auto_update", enabled);
        }

        public bool? LoadTrayIconEnabled()
        {
            return LoadBoolean("tray_icon");
        }

        public void SaveTrayIconEnabled(bool enabled)
        {
            SaveBoolean("tray_icon", enabled);
        }

        private bool? LoadBoolean(string key)
        {
            if (!File.Exists(_path)) return null;
            var prefix = key + "=";
            foreach (var raw in File.ReadAllLines(_path))
            {
                var line = raw.Trim();
                if (!line.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) continue;
                var value = line.Substring(prefix.Length).Trim();
                if (value == "0" || value.Equals("false", StringComparison.OrdinalIgnoreCase)) return false;
                if (value == "1" || value.Equals("true", StringComparison.OrdinalIgnoreCase)) return true;
            }
            return null;
        }

        private void SaveBoolean(string key, bool enabled)
        {
            var directory = Path.GetDirectoryName(_path);
            if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);

            var lines = new List<string>();
            if (File.Exists(_path)) lines.AddRange(File.ReadAllLines(_path));
            var prefix = key + "=";
            var replacement = prefix + (enabled ? "1" : "0");
            var replaced = false;
            for (var i = 0; i < lines.Count; i++)
            {
                if (!lines[i].Trim().StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) continue;
                lines[i] = replacement;
                replaced = true;
                break;
            }
            if (!replaced) lines.Add(replacement);
            File.WriteAllLines(_path, lines.ToArray());
        }
    }
}
