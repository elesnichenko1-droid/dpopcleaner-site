using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    public static class PackageVerifier
    {
        public static bool Verify(string path, long expectedSize, string expectedSha256)
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path)) return false;
            if (expectedSize <= 0 || string.IsNullOrWhiteSpace(expectedSha256) || expectedSha256.Length != 64) return false;
            var info = new FileInfo(path);
            if (info.Length != expectedSize) return false;

            using (var stream = File.OpenRead(path))
            using (var sha = SHA256.Create())
            {
                var hash = sha.ComputeHash(stream);
                var builder = new StringBuilder(hash.Length * 2);
                foreach (var b in hash) builder.Append(b.ToString("x2"));
                return string.Equals(builder.ToString(), expectedSha256.Trim(), StringComparison.OrdinalIgnoreCase);
            }
        }
    }
}
