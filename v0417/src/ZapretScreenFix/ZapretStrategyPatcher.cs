using System;
using System.Collections.Generic;

namespace DPop.ZapretScreenFix
{
    public sealed class PatchTextResult
    {
        public PatchTextResult(string text, bool changed, int patchedLines)
        {
            Text = text;
            Changed = changed;
            PatchedLines = patchedLines;
        }

        public string Text { get; }
        public bool Changed { get; }
        public int PatchedLines { get; }
    }

    public static class ZapretStrategyPatcher
    {
        public const string BackupSuffix = ".dpop0417-screen-share.bak";

        public static PatchTextResult PatchText(string text)
        {
            return new PatchTextResult(text ?? string.Empty, false, 0);
        }

        public static IReadOnlyList<string> FindCandidates(string rootPath)
        {
            return Array.Empty<string>();
        }

        public static bool PatchFile(string path)
        {
            return false;
        }

        public static bool RestoreFile(string path)
        {
            return false;
        }
    }
}
