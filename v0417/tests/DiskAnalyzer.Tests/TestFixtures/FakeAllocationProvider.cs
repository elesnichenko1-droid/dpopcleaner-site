using System;
using System.Collections.Generic;
using DPop.DiskAnalyzer.Scanning;

namespace DPop.DiskAnalyzer.Tests.TestFixtures
{
    internal sealed class FakeAllocationProvider : IAllocationSizeProvider
    {
        private readonly IDictionary<string, long?> _values;

        public FakeAllocationProvider(IDictionary<string, long?> values = null)
        {
            _values = values ?? new Dictionary<string, long?>(StringComparer.OrdinalIgnoreCase);
        }

        public long? GetAllocatedBytes(string path)
        {
            long? value;
            return _values.TryGetValue(path, out value) ? value : null;
        }
    }
}
