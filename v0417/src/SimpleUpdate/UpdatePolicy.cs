namespace DPopCleaner.SimpleUpdate
{
    public static class UpdatePolicy
    {
        public static bool IsNewer(int currentVersionCode, int currentRevision, int remoteVersionCode, int remoteRevision)
        {
            if (remoteVersionCode != currentVersionCode) return remoteVersionCode > currentVersionCode;
            return remoteRevision > currentRevision;
        }
    }
}
