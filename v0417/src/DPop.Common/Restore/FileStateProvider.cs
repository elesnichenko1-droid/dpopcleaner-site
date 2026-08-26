using System;
using System.Collections.Generic;
using System.IO;
using System.Web.Script.Serialization;

namespace DPop.Common.Restore
{
    public sealed class FileStateProvider : IRestoreProvider
    {
        private readonly string _allowedRoot;
        private readonly string _allowedPrefix;
        private readonly JavaScriptSerializer _json = new JavaScriptSerializer();

        public FileStateProvider(string allowedRoot)
        {
            if (string.IsNullOrWhiteSpace(allowedRoot))
                throw new ArgumentException("An allowed root is required.", nameof(allowedRoot));

            _allowedRoot = NormalizeDirectory(allowedRoot);
            _allowedPrefix = _allowedRoot.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal)
                ? _allowedRoot
                : _allowedRoot + Path.DirectorySeparatorChar;
        }

        public string BackupCategory => "Settings";

        public bool CanHandle(string operationId)
        {
            return string.Equals(operationId, "settings.file", StringComparison.Ordinal);
        }

        public string Capture(string targetPath)
        {
            var target = ValidateTarget(targetPath);
            var exists = File.Exists(target);
            var state = new Dictionary<string, object>
            {
                ["exists"] = exists,
                ["bytes"] = exists ? Convert.ToBase64String(File.ReadAllBytes(target)) : null,
            };
            return _json.Serialize(state);
        }

        public void Restore(string targetPath, string stateJson)
        {
            var target = ValidateTarget(targetPath);
            if (string.IsNullOrWhiteSpace(stateJson))
                throw new InvalidDataException("File restore state is empty.");

            Dictionary<string, object> state;
            try
            {
                state = _json.Deserialize<Dictionary<string, object>>(stateJson);
            }
            catch (Exception ex)
            {
                throw new InvalidDataException("File restore state is invalid.", ex);
            }

            object existsValue;
            if (state == null || !state.TryGetValue("exists", out existsValue) || !(existsValue is bool))
                throw new InvalidDataException("File restore state has no valid exists flag.");

            if (!(bool)existsValue)
            {
                if (File.Exists(target)) File.Delete(target);
                return;
            }

            object bytesValue;
            if (!state.TryGetValue("bytes", out bytesValue) || !(bytesValue is string))
                throw new InvalidDataException("File restore state has no file bytes.");

            byte[] bytes;
            try
            {
                bytes = Convert.FromBase64String((string)bytesValue);
            }
            catch (FormatException ex)
            {
                throw new InvalidDataException("File restore bytes are invalid.", ex);
            }

            var directory = Path.GetDirectoryName(target);
            if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
            File.WriteAllBytes(target, bytes);
        }

        private string ValidateTarget(string targetPath)
        {
            if (string.IsNullOrWhiteSpace(targetPath))
                throw new InvalidDataException("File restore target is empty.");

            string fullPath;
            try
            {
                fullPath = Path.GetFullPath(targetPath);
            }
            catch (Exception ex)
            {
                throw new InvalidDataException("File restore target is invalid.", ex);
            }

            if (!fullPath.StartsWith(_allowedPrefix, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("File restore target escapes the allowed root.");

            return fullPath;
        }

        private static string NormalizeDirectory(string path)
        {
            var resolved = Path.GetFullPath(path);
            var full = resolved.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            return full.Length == 0 ? Path.GetPathRoot(resolved) : full;
        }
    }
}
