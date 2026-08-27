using System;

namespace DPopCleaner.SimpleUpdate
{
    public sealed class LauncherOptions
    {
        public const string DefaultManifestUrl = "https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json";

        private LauncherOptions()
        {
            UpdateCheckEnabled = true;
            ManifestUrl = DefaultManifestUrl;
        }

        public bool UpdateCheckEnabled { get; private set; }
        public string ManifestUrl { get; private set; }
        public string SettingsPathOverride { get; private set; }

        public static LauncherOptions Parse(string[] args)
        {
            var options = new LauncherOptions();
            if (args == null) return options;

            for (var i = 0; i < args.Length; i++)
            {
                var arg = args[i] ?? string.Empty;
                if (string.Equals(arg, "--no-update-check", StringComparison.OrdinalIgnoreCase))
                {
                    options.UpdateCheckEnabled = false;
                }
                else if (string.Equals(arg, "--manifest-url", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    var value = args[++i];
                    Uri uri;
                    if (!Uri.TryCreate(value, UriKind.Absolute, out uri) || uri.Scheme != Uri.UriSchemeHttps)
                        throw new ArgumentException("--manifest-url must be an absolute HTTPS URL.");
                    options.ManifestUrl = uri.AbsoluteUri;
                }
                else if (string.Equals(arg, "--settings-path", StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    options.SettingsPathOverride = args[++i];
                }
            }

            return options;
        }
    }
}
