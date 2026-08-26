namespace DPop.Common.Restore
{
    public interface IRestoreProvider
    {
        string BackupCategory { get; }
        bool CanHandle(string operationId);
        string Capture(string target);
        void Restore(string target, string state);
    }
}
