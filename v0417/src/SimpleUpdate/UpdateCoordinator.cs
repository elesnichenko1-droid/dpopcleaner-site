using System;
using System.Threading;
using System.Threading.Tasks;

namespace DPopCleaner.SimpleUpdate
{
    public sealed class UpdateCoordinator
    {
        private readonly IUpdateSource _source;
        private readonly Func<UpdateManifest, bool> _confirm;
        private readonly Func<UpdateManifest, string> _destinationFactory;
        private readonly Func<Task> _closeCore;
        private readonly Action<string, string> _launchInstaller;

        public UpdateCoordinator(
            IUpdateSource source,
            Func<UpdateManifest, bool> confirm,
            Func<UpdateManifest, string> destinationFactory,
            Func<Task> closeCore,
            Action<string, string> launchInstaller)
        {
            _source = source ?? throw new ArgumentNullException(nameof(source));
            _confirm = confirm ?? throw new ArgumentNullException(nameof(confirm));
            _destinationFactory = destinationFactory ?? throw new ArgumentNullException(nameof(destinationFactory));
            _closeCore = closeCore ?? throw new ArgumentNullException(nameof(closeCore));
            _launchInstaller = launchInstaller ?? throw new ArgumentNullException(nameof(launchInstaller));
        }

        public async Task<bool> CheckAndInstallAsync(
            string manifestUrl,
            int currentVersionCode,
            int currentRevision,
            CancellationToken cancellationToken)
        {
            var manifest = await _source.CheckForNewerAsync(
                manifestUrl, currentVersionCode, currentRevision, cancellationToken).ConfigureAwait(true);
            if (manifest == null) return false;
            if (!_confirm(manifest)) return false;

            var destination = _destinationFactory(manifest);
            var verifiedPackage = await _source.DownloadVerifiedPackageAsync(
                manifest, destination, cancellationToken).ConfigureAwait(true);

            await _closeCore().ConfigureAwait(true);
            _launchInstaller(verifiedPackage, manifest.InstallArgs);
            return true;
        }
    }
}
