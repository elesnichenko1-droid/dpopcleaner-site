using System;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;

namespace DPopCleaner.SimpleUpdate
{
    public sealed class UpdateClient
    {
        private readonly HttpClient _http;

        public UpdateClient(HttpClient http)
        {
            _http = http ?? throw new ArgumentNullException(nameof(http));
        }

        public async Task<UpdateManifest> CheckForNewerAsync(
            string manifestUrl,
            int currentVersionCode,
            int currentRevision,
            CancellationToken cancellationToken)
        {
            Uri uri;
            if (!Uri.TryCreate(manifestUrl, UriKind.Absolute, out uri) || uri.Scheme != Uri.UriSchemeHttps)
                throw new InvalidDataException("Update manifest URL must use HTTPS.");

            using (var response = await _http.GetAsync(uri, HttpCompletionOption.ResponseContentRead, cancellationToken).ConfigureAwait(false))
            {
                response.EnsureSuccessStatusCode();
                var json = await response.Content.ReadAsStringAsync().ConfigureAwait(false);
                UpdateManifest manifest;
                if (!UpdateManifest.TryParseValidated(json, out manifest))
                    throw new InvalidDataException("Invalid stable update manifest.");

                return UpdatePolicy.IsNewer(currentVersionCode, currentRevision, manifest.VersionCode, manifest.Revision)
                    ? manifest
                    : null;
            }
        }

        public async Task<string> DownloadVerifiedPackageAsync(
            UpdateManifest manifest,
            string destinationPath,
            CancellationToken cancellationToken)
        {
            if (manifest == null) throw new ArgumentNullException(nameof(manifest));
            if (string.IsNullOrWhiteSpace(destinationPath)) throw new ArgumentException("Destination is required.", nameof(destinationPath));

            var fullPath = Path.GetFullPath(destinationPath);
            var directory = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);

            try
            {
                if (File.Exists(fullPath)) File.Delete(fullPath);

                using (var response = await _http.GetAsync(manifest.DownloadUrl, HttpCompletionOption.ResponseHeadersRead, cancellationToken).ConfigureAwait(false))
                {
                    response.EnsureSuccessStatusCode();
                    using (var input = await response.Content.ReadAsStreamAsync().ConfigureAwait(false))
                    using (var output = new FileStream(fullPath, FileMode.CreateNew, FileAccess.Write, FileShare.None, 81920, true))
                    {
                        await input.CopyToAsync(output, 81920, cancellationToken).ConfigureAwait(false);
                        await output.FlushAsync(cancellationToken).ConfigureAwait(false);
                    }
                }

                if (!PackageVerifier.Verify(fullPath, manifest.Size, manifest.Sha256))
                    throw new InvalidDataException("Downloaded update failed size or SHA-256 verification.");

                return fullPath;
            }
            catch
            {
                try { if (File.Exists(fullPath)) File.Delete(fullPath); } catch { }
                throw;
            }
        }
    }
}
