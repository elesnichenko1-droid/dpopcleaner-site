using System;
using System.Collections.Generic;
using System.IO;
using System.Web.Script.Serialization;

namespace DPop.Common.Localization
{
    public sealed class LanguageCatalog
    {
        private readonly Dictionary<string, string> _active;
        private readonly Dictionary<string, string> _fallback;

        private LanguageCatalog(Dictionary<string, string> active, Dictionary<string, string> fallback)
        {
            _active = active;
            _fallback = fallback;
        }

        public static LanguageCatalog Load(string languagesDirectory, string requestedCode)
        {
            if (languagesDirectory == null) throw new ArgumentNullException(nameof(languagesDirectory));

            var fallback = ReadPack(Path.Combine(languagesDirectory, "ru.json"));
            var code = string.IsNullOrWhiteSpace(requestedCode)
                ? "ru"
                : requestedCode.Trim().ToLowerInvariant();

            if (code.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || code.Contains("/") || code.Contains("\\"))
                code = "ru";

            Dictionary<string, string> active;
            var requestedPath = Path.Combine(languagesDirectory, code + ".json");
            if (code == "ru")
                active = fallback;
            else if (File.Exists(requestedPath))
                active = ReadPack(requestedPath);
            else
                active = new Dictionary<string, string>(StringComparer.Ordinal);

            return new LanguageCatalog(active, fallback);
        }

        public string Get(string key)
        {
            if (key == null) throw new ArgumentNullException(nameof(key));

            string value;
            if (_active.TryGetValue(key, out value)) return value;
            if (_fallback.TryGetValue(key, out value)) return value;
            return "[" + key + "]";
        }

        private static Dictionary<string, string> ReadPack(string path)
        {
            if (!File.Exists(path))
                return new Dictionary<string, string>(StringComparer.Ordinal);

            var serializer = new JavaScriptSerializer();
            var parsed = serializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(path));
            return parsed ?? new Dictionary<string, string>(StringComparer.Ordinal);
        }
    }
}
