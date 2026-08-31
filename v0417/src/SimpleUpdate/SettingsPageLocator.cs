using System;

namespace DPopCleaner.SimpleUpdate
{
    internal static class SettingsPageLocator
    {
        // These two controls live in the frozen Settings right column, outside the bridge-owned
        // left scroll host. HideLegacyOverflowControls never hides them, so they cannot create
        // the self-hiding feedback loop caused by using a left-side checkbox as the page marker.
        private const int SaveSettingsButtonId = NativeBridge.SaveSettingsButtonId;
        private const int AddFileButtonId = 1402;

        internal static bool IsVisible(IntPtr parent)
        {
            if (parent == IntPtr.Zero) return false;

            var save = NativeBridge.FindChildById(parent, SaveSettingsButtonId);
            var addFile = NativeBridge.FindChildById(parent, AddFileButtonId);
            return save != IntPtr.Zero &&
                   addFile != IntPtr.Zero &&
                   NativeBridge.IsWindowVisible(save) &&
                   NativeBridge.IsWindowVisible(addFile);
        }
    }
}
