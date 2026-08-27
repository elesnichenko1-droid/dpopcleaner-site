using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace DPopCleaner.SimpleUpdate
{
    public sealed class UpdateManifest
    {
        public string Product { get; private set; }
        public string Channel { get; private set; }
        public string Version { get; private set; }
        public int VersionCode { get; private set; }
        public int Revision { get; private set; }
        public bool Available { get; private set; }
        public string DownloadUrl { get; private set; }
        public string Sha256 { get; private set; }
        public long Size { get; private set; }
        public string InstallArgs { get; private set; }

        private static readonly Regex Sha256Pattern = new Regex("^[0-9a-fA-F]{64}$", RegexOptions.CultureInvariant);

        public static bool TryParseValidated(string json, out UpdateManifest manifest)
        {
            manifest = null;
            if (string.IsNullOrWhiteSpace(json)) return false;
            try
            {
                var data = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(json);
                if (data == null) return false;

                var product = GetString(data, "product");
                var channel = GetString(data, "channel");
                var version = GetString(data, "version");
                var downloadUrl = GetString(data, "download_url");
                var sha = GetString(data, "sha256");
                var installArgs = GetString(data, "install_args");
                var versionCode = GetInt(data, "version_code");
                var revision = GetInt(data, "revision");
                var size = GetLong(data, "size");
                var available = GetBool(data, "available");

                Uri uri;
                if (!string.Equals(product, "DPopCleaner", StringComparison.Ordinal)) return false;
                if (!string.Equals(channel, "stable", StringComparison.OrdinalIgnoreCase)) return false;
                if (string.IsNullOrWhiteSpace(version) || versionCode <= 0 || revision < 0) return false;
                if (!available || size <= 0 || !Sha256Pattern.IsMatch(sha ?? string.Empty)) return false;
                if (!Uri.TryCreate(downloadUrl, UriKind.Absolute, out uri) || uri.Scheme != Uri.UriSchemeHttps) return false;

                manifest = new UpdateManifest
                {
                    Product = product,
                    Channel = channel,
                    Version = version,
                    VersionCode = versionCode,
                    Revision = revision,
                    Available = available,
                    DownloadUrl = downloadUrl,
                    Sha256 = sha.ToLowerInvariant(),
                    Size = size,
                    InstallArgs = string.IsNullOrWhiteSpace(installArgs) ? "/SILENT /NORESTART" : installArgs
                };
                return true;
            }
            catch
            {
                return false;
            }
        }

        private static string GetString(IDictionary<string, object> data, string key)
        {
            object value;
            return data.TryGetValue(key, out value) && value != null ? Convert.ToString(value) : null;
        }

        private static int GetInt(IDictionary<string, object> data, string key)
        {
            object value;
            int parsed;
            return data.TryGetValue(key, out value) && value != null && int.TryParse(Convert.ToString(value), out parsed) ? parsed : 0;
        }

        private static long GetLong(IDictionary<string, object> data, string key)
        {
            object value;
            long parsed;
            return data.TryGetValue(key, out value) && value != null && long.TryParse(Convert.ToString(value), out parsed) ? parsed : 0;
        }

        private static bool GetBool(IDictionary<string, object> data, string key)
        {
            object value;
            bool parsed;
            return data.TryGetValue(key, out value) && value != null && bool.TryParse(Convert.ToString(value), out parsed) && parsed;
        }
    }
}
