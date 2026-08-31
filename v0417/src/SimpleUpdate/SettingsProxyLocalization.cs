using System;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal static class SettingsProxyLocalization
    {
        private const uint CB_GETCURSEL = 0x0147;
        private const uint CB_GETLBTEXT = 0x0148;
        private const uint CB_GETLBTEXTLEN = 0x0149;

        private const int FirstSettingProxyId = 1500;
        private const int LicenseInfoProxyId = 1498;
        private const int LicenseNoteProxyId = 1499;

        private static readonly string[] RussianSettingTexts =
        {
            "Фоновый контроль мусора каждые 30 минут",
            "Быстрый DPopGuard-скан при запуске",
            "Проверять кэш Windows Update при запуске",
            "Работать в трее и отслеживать новые установки",
            "Автозапуск DPopCleaner вместе с Windows",
            "Запускать приложение от имени администратора"
        };

        private static readonly string[] EnglishSettingTexts =
        {
            "Background junk check every 30 minutes",
            "Quick DPopGuard scan on startup",
            "Check Windows Update cache on startup",
            "Run in tray and watch newly installed apps",
            "Start DPopCleaner with Windows",
            "Always run application as administrator"
        };

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
        private static extern IntPtr SendMessageBuffer(IntPtr hWnd, uint msg, IntPtr wParam, StringBuilder lParam);

        internal static void Apply(IntPtr parent)
        {
            if (parent == IntPtr.Zero) return;
            var english = IsEnglish(parent);
            var settings = english ? EnglishSettingTexts : RussianSettingTexts;

            for (var i = 0; i < settings.Length; i++)
                WriteIfDifferent(NativeBridge.FindChildById(parent, FirstSettingProxyId + i), settings[i]);

            WriteIfDifferent(
                NativeBridge.FindChildById(parent, NativeBridge.AutoUpdateCheckboxId),
                english ? "Enable application auto-updates" : "Включить автообновление приложения");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, NativeBridge.CheckNowButtonId),
                english ? "Check for updates" : "Проверить обновления");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, NativeBridge.LicenseHeadingProxyId),
                english ? "License" : "Лицензия");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, LicenseInfoProxyId),
                english
                    ? "Free BETA. License server will be connected later."
                    : "Бесплатная BETA. Лицензионный сервер будет подключён позже.");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, NativeBridge.LicenseSaveProxyId),
                english ? "Save key" : "Сохранить ключ");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, NativeBridge.LicenseBuyProxyId),
                english ? "Buy license" : "Купить лицензию");
            WriteIfDifferent(
                NativeBridge.FindChildById(parent, LicenseNoteProxyId),
                english
                    ? "Purchasing and online key validation will be connected later."
                    : "Покупка и проверка ключей будут подключены позже.");
        }

        private static bool IsEnglish(IntPtr parent)
        {
            var combo = FindLanguageCombo(parent);
            if (combo == IntPtr.Zero) return false;
            return string.Equals(ReadSelectedItem(combo), "English", StringComparison.OrdinalIgnoreCase);
        }

        private static IntPtr FindLanguageCombo(IntPtr parent)
        {
            NativeBridge.ChildInfo best = null;
            foreach (var child in NativeBridge.GetChildren(parent))
            {
                if (!child.Visible) continue;
                if (!string.Equals(child.ClassName, "ComboBox", StringComparison.OrdinalIgnoreCase)) continue;
                if (best == null || child.Top < best.Top) best = child;
            }
            return best != null ? best.Handle : IntPtr.Zero;
        }

        private static string ReadSelectedItem(IntPtr combo)
        {
            var index = NativeBridge.SendMessage(combo, CB_GETCURSEL, IntPtr.Zero, IntPtr.Zero).ToInt32();
            if (index < 0) return string.Empty;

            var length = NativeBridge.SendMessage(combo, CB_GETLBTEXTLEN, new IntPtr(index), IntPtr.Zero).ToInt32();
            if (length < 0) return string.Empty;

            var value = new StringBuilder(length + 1);
            SendMessageBuffer(combo, CB_GETLBTEXT, new IntPtr(index), value);
            return value.ToString();
        }

        private static void WriteIfDifferent(IntPtr handle, string text)
        {
            if (handle == IntPtr.Zero) return;
            if (string.Equals(NativeBridge.ReadWindowText(handle), text, StringComparison.Ordinal)) return;
            NativeBridge.WriteWindowText(handle, text);
        }
    }
}
