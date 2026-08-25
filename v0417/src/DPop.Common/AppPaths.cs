using System.IO;

namespace DPop.Common
{
    public sealed class AppPaths
    {
        public AppPaths(string installRoot)
        {
            InstallRoot = installRoot;
        }

        public string InstallRoot { get; }
        public string LanguagesDirectory => Path.Combine(InstallRoot, "Languages");
        public string ShellDirectory => Path.Combine(InstallRoot, "Shell");
        public string DocumentationDirectory => Path.Combine(InstallRoot, "Documentation");
        public string ModulesDirectory => Path.Combine(InstallRoot, "Modules");
        public string ResourcesDirectory => Path.Combine(InstallRoot, "Resources");
    }
}
