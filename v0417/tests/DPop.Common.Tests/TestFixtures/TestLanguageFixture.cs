using System;
using System.IO;

namespace DPop.Common.Tests.TestFixtures
{
    internal static class TestLanguageFixture
    {
        public static string Create(string ru, string en)
        {
            var dir = Path.Combine(Path.GetTempPath(), "dpop0417-lang-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(dir);
            File.WriteAllText(Path.Combine(dir, "ru.json"), ru);
            File.WriteAllText(Path.Combine(dir, "en.json"), en);
            return dir;
        }
    }
}
