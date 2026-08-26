using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace DPop.ZapretScreenFix
{
    public sealed class PatchTextResult
    {
        public PatchTextResult(string text, bool changed, int patchedLines)
        {
            Text = text;
            Changed = changed;
            PatchedLines = patchedLines;
        }

        public string Text { get; }
        public bool Changed { get; }
        public int PatchedLines { get; }
    }

    public static class ZapretStrategyPatcher
    {
        public const string BackupSuffix = ".dpop0417-screen-share.bak";

        private const string DiscordMediaToken = "--hostlist-domains=discord.media";
        private static readonly Regex FilterTcpRegex = new Regex(
            @"--filter-tcp=(?<ports>\d+(?:,\d+)*)",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled);

        public static PatchTextResult PatchText(string text)
        {
            if (text == null)
                text = string.Empty;

            // Split with a capturing group so every original CRLF/LF/CR separator is
            // retained verbatim. Only the matching command line itself is rewritten.
            var parts = Regex.Split(text, "(\\r\\n|\\n|\\r)");
            var patchedLines = 0;

            for (var i = 0; i < parts.Length; i += 2)
            {
                string patched;
                if (TryPatchLine(parts[i], out patched))
                {
                    parts[i] = patched;
                    patchedLines++;
                }
            }

            return new PatchTextResult(
                string.Concat(parts),
                patchedLines > 0,
                patchedLines);
        }

        public static IReadOnlyList<string> FindCandidates(string rootPath)
        {
            if (string.IsNullOrWhiteSpace(rootPath))
                throw new ArgumentException("Zapret root path is empty.", nameof(rootPath));
            if (!Directory.Exists(rootPath))
                throw new DirectoryNotFoundException("Zapret root path was not found: " + rootPath);

            var result = new List<string>();
            foreach (var path in Directory.EnumerateFiles(rootPath, "*.bat", SearchOption.AllDirectories))
            {
                try
                {
                    var decoded = ReadTextPreservingEncoding(path);
                    if (PatchText(decoded.Text).Changed)
                        result.Add(path);
                }
                catch (IOException)
                {
                    // A locked/unreadable strategy is simply not presented as an
                    // actionable candidate. PatchFile will still fail loudly if the
                    // user explicitly attempts to change such a file later.
                }
                catch (UnauthorizedAccessException)
                {
                }
            }

            result.Sort(StringComparer.OrdinalIgnoreCase);
            return result;
        }

        public static bool PatchFile(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
                throw new ArgumentException("Strategy path is empty.", nameof(path));
            if (!File.Exists(path))
                throw new FileNotFoundException("Zapret strategy file was not found.", path);

            var originalBytes = File.ReadAllBytes(path);
            var decoded = Decode(originalBytes);
            var result = PatchText(decoded.Text);
            if (!result.Changed)
                return false;

            var backupPath = path + BackupSuffix;
            if (!File.Exists(backupPath))
                File.WriteAllBytes(backupPath, originalBytes);

            var patchedBytes = Encode(result.Text, decoded.Encoding, decoded.Preamble);
            var temporaryPath = path + ".dpop0417-screen-share.tmp";
            try
            {
                File.WriteAllBytes(temporaryPath, patchedBytes);
                File.Copy(temporaryPath, path, true);
            }
            finally
            {
                if (File.Exists(temporaryPath))
                    File.Delete(temporaryPath);
            }

            return true;
        }

        public static bool RestoreFile(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
                throw new ArgumentException("Strategy path is empty.", nameof(path));

            var backupPath = path + BackupSuffix;
            if (!File.Exists(backupPath))
                return false;

            // Keep the backup after restoration. It is the immutable pre-fix state
            // and gives the user another recovery point if a service reinstall or a
            // later Zapret update changes the strategy again.
            File.WriteAllBytes(path, File.ReadAllBytes(backupPath));
            return true;
        }

        private static bool TryPatchLine(string line, out string patched)
        {
            patched = line;
            if (string.IsNullOrEmpty(line))
                return false;

            var domainIndex = line.IndexOf(DiscordMediaToken, StringComparison.OrdinalIgnoreCase);
            if (domainIndex < 0)
                return false;

            // Zapret strategy lines can contain several --new sections. Select the
            // --filter-tcp belonging to the same section as discord.media rather than
            // accidentally changing an earlier rule on the same physical line.
            var sectionStart = LastIndexOf(line, "--new", domainIndex);
            var sectionEnd = line.IndexOf("--new", domainIndex + DiscordMediaToken.Length, StringComparison.OrdinalIgnoreCase);
            if (sectionEnd < 0)
                sectionEnd = line.Length;

            Match selected = null;
            foreach (Match match in FilterTcpRegex.Matches(line))
            {
                if (!match.Success || match.Index < sectionStart || match.Index >= sectionEnd)
                    continue;

                if (match.Index <= domainIndex)
                    selected = match;
                else if (selected == null)
                {
                    selected = match;
                    break;
                }
            }

            if (selected == null)
                return false;

            var portsGroup = selected.Groups["ports"];
            var ports = portsGroup.Value.Split(',');
            if (ports.Any(port => string.Equals(port.Trim(), "443", StringComparison.Ordinal)))
                return false;

            patched = line.Substring(0, portsGroup.Index)
                      + "443," + portsGroup.Value
                      + line.Substring(portsGroup.Index + portsGroup.Length);
            return true;
        }

        private static int LastIndexOf(string value, string token, int beforeIndex)
        {
            if (beforeIndex <= 0)
                return 0;

            var index = value.LastIndexOf(token, beforeIndex, StringComparison.OrdinalIgnoreCase);
            return index < 0 ? 0 : index;
        }

        private sealed class DecodedText
        {
            public DecodedText(string text, Encoding encoding, byte[] preamble)
            {
                Text = text;
                Encoding = encoding;
                Preamble = preamble ?? Array.Empty<byte>();
            }

            public string Text { get; }
            public Encoding Encoding { get; }
            public byte[] Preamble { get; }
        }

        private static DecodedText ReadTextPreservingEncoding(string path)
        {
            return Decode(File.ReadAllBytes(path));
        }

        private static DecodedText Decode(byte[] bytes)
        {
            if (HasPrefix(bytes, new byte[] { 0xEF, 0xBB, 0xBF }))
            {
                var encoding = new UTF8Encoding(false, true);
                return new DecodedText(encoding.GetString(bytes, 3, bytes.Length - 3), encoding, new byte[] { 0xEF, 0xBB, 0xBF });
            }
            if (HasPrefix(bytes, new byte[] { 0xFF, 0xFE }))
            {
                var encoding = new UnicodeEncoding(false, false, true);
                return new DecodedText(encoding.GetString(bytes, 2, bytes.Length - 2), encoding, new byte[] { 0xFF, 0xFE });
            }
            if (HasPrefix(bytes, new byte[] { 0xFE, 0xFF }))
            {
                var encoding = new UnicodeEncoding(true, false, true);
                return new DecodedText(encoding.GetString(bytes, 2, bytes.Length - 2), encoding, new byte[] { 0xFE, 0xFF });
            }

            // Most modern Zapret batch files are UTF-8/ASCII without a BOM. Use a
            // strict decoder first so non-ASCII UTF-8 paths/comments are preserved.
            try
            {
                var utf8 = new UTF8Encoding(false, true);
                return new DecodedText(utf8.GetString(bytes), utf8, Array.Empty<byte>());
            }
            catch (DecoderFallbackException)
            {
                return new DecodedText(Encoding.Default.GetString(bytes), Encoding.Default, Array.Empty<byte>());
            }
        }

        private static byte[] Encode(string text, Encoding encoding, byte[] preamble)
        {
            var body = encoding.GetBytes(text ?? string.Empty);
            if (preamble == null || preamble.Length == 0)
                return body;

            var output = new byte[preamble.Length + body.Length];
            Buffer.BlockCopy(preamble, 0, output, 0, preamble.Length);
            Buffer.BlockCopy(body, 0, output, preamble.Length, body.Length);
            return output;
        }

        private static bool HasPrefix(byte[] bytes, byte[] prefix)
        {
            if (bytes == null || prefix == null || bytes.Length < prefix.Length)
                return false;
            for (var i = 0; i < prefix.Length; i++)
            {
                if (bytes[i] != prefix[i])
                    return false;
            }
            return true;
        }
    }
}
