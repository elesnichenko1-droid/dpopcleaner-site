namespace DPop.DiskAnalyzer.Scanning
{
    public interface IAllocationSizeProvider
    {
        long? GetAllocatedBytes(string path);
    }
}
