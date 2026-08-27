using System.Threading;
using System.Threading.Tasks;

namespace DPopCleaner.SimpleUpdate
{
    public interface IUpdateSource
    {
        Task<UpdateManifest> CheckForNewerAsync(
            string manifestUrl,
            int currentVersionCode,
            int currentRevision,
            CancellationToken cancellationToken);

        Task<string> DownloadVerifiedPackageAsync(
            UpdateManifest manifest,
            string destinationPath,
            CancellationToken cancellationToken);
    }
}
