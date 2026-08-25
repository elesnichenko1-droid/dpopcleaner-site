using System;
using System.Collections.Generic;
using System.IO;

namespace DPop.Common.History
{
    public sealed class BackupStore
    {
        private static readonly IDictionary<string, string> AllowedCategories =
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["Settings"] = "Settings",
                ["Registry"] = "Registry",
                ["System"] = "System",
            };

        private readonly string _backupRoot;
        private readonly string _backupRootWithSeparator;

        public BackupStore(string backupRoot)
        {
            if (string.IsNullOrWhiteSpace(backupRoot))
                throw new ArgumentException("Backup root is required.", nameof(backupRoot));

            _backupRoot = Path.GetFullPath(backupRoot).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            _backupRootWithSeparator = _backupRoot + Path.DirectorySeparatorChar;
            Directory.CreateDirectory(_backupRoot);
        }

        public string SaveBytes(string category, byte[] data)
        {
            if (data == null) throw new ArgumentNullException(nameof(data));

            string canonicalCategory;
            if (string.IsNullOrWhiteSpace(category) || !AllowedCategories.TryGetValue(category, out canonicalCategory))
                throw new ArgumentException("Unsupported backup category.", nameof(category));

            var categoryDirectory = Path.Combine(_backupRoot, canonicalCategory);
            Directory.CreateDirectory(categoryDirectory);

            var fileName = DateTime.UtcNow.ToString("yyyyMMdd-HHmmssfff") + "-" + Guid.NewGuid().ToString("N") + ".bin";
            var fullPath = Path.Combine(categoryDirectory, fileName);
            using (var stream = new FileStream(fullPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                stream.Write(data, 0, data.Length);
                stream.Flush(true);
            }

            return canonicalCategory + "/" + fileName;
        }

        public byte[] ReadBytes(string reference)
        {
            var fullPath = ResolveReference(reference);
            if (!File.Exists(fullPath))
                throw new FileNotFoundException("Backup does not exist.", fullPath);
            return File.ReadAllBytes(fullPath);
        }

        public bool Exists(string reference)
        {
            try
            {
                return File.Exists(ResolveReference(reference));
            }
            catch (InvalidDataException)
            {
                return false;
            }
        }

        private string ResolveReference(string reference)
        {
            if (string.IsNullOrWhiteSpace(reference))
                throw new InvalidDataException("Backup reference is required.");
            if (Path.IsPathRooted(reference))
                throw new InvalidDataException("Backup reference must be relative.");

            var normalizedReference = reference.Replace('/', Path.DirectorySeparatorChar);
            var parts = normalizedReference.Split(new[] { Path.DirectorySeparatorChar }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 2)
                throw new InvalidDataException("Backup reference must contain exactly a category and file name.");

            string canonicalCategory;
            if (!AllowedCategories.TryGetValue(parts[0], out canonicalCategory))
                throw new InvalidDataException("Unsupported backup category.");
            if (!string.Equals(Path.GetFileName(parts[1]), parts[1], StringComparison.Ordinal))
                throw new InvalidDataException("Backup file name is invalid.");

            var candidate = Path.GetFullPath(Path.Combine(_backupRoot, canonicalCategory, parts[1]));
            if (!candidate.StartsWith(_backupRootWithSeparator, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Backup reference escapes backup root.");
            return candidate;
        }
    }
}
