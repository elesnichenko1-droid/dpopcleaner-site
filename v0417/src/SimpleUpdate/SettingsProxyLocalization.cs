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

            var combo = FindLanguageCombo(parent);
            var selectedLanguage = combo != IntPtr.Zero ? ReadSelectedItem(combo) : string.Empty;
            BridgeDiagnostics.RecordState(
                "settings-language combo=0x" + combo.ToInt64().ToString("X") +
                " selected='" + selectedLanguage + "'");

            var english = string.Equals(selectedLanguage, "English", StringComparison.OrdinalIgnoreCase);
            var settings = english ? EnglishSettingTexts : RussianSettingTexts;

            for (var i = 0; i < settings.Length; i++)
                WriteIfDifferent(parent, FirstSettingProxyId + i, settings[i]);

            WriteIfDifferent(
                parent,
                NativeBridge.AutoUpdateCheckboxId,
                english ? "Enable application auto-updates" : "Включить автообновление приложения");
            WriteIfDifferent(
                parent,
                NativeBridge.CheckNowButtonId,
                english ? "Check for updates" : "Проверить обновления");
            WriteIfDifferent(
                parent,
                NativeBridge.LicenseHeadingProxyId,
                english ? "License" : "Лицензия");
            WriteIfDifferent(
                parent,
                LicenseInfoProxyId,
                english
                    ? "Free BETA. License server will be connected later."
                    : "Бесплатная BETA. Лицензионный сервер будет подключён позже.");
            WriteIfDifferent(
                parent,
                NativeBridge.LicenseSaveProxyId,
                english ? "Save key" : "Сохранить ключ");
            WriteIfDifferent(
                parent,
                NativeBridge.LicenseBuyProxyId,
                english ? "Buy license" : "Купить лицензию");
            WriteIfDifferent(
                parent,
                LicenseNoteProxyId,
                english
                    ? "Purchasing and online key validation will be connected later."
                    : "Покупка и проверка ключей будут подключены позже.");
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

        private static void WriteIfDifferent(IntPtr parent, int id, string text)
        {
            var handle = NativeBridge.FindChildById(parent, id);
            if (handle == IntPtr.Zero)
            {
                TraceWrite(id, handle, "<missing>", text, "<missing>");
                return;
            }

            var before = NativeBridge.ReadWindowText(handle);
            if (!string.Equals(before, text, StringComparison.Ordinal))
                NativeBridge.WriteWindowText(handle, text);
            var after = NativeBridge.ReadWindowText(handle);
            TraceWrite(id, handle, before, text, after);
        }

        private static void TraceWrite(int id, IntPtr handle, string before, string desired, string after)
        {
            if (id != FirstSettingProxyId + 5 &&
                id != NativeBridge.AutoUpdateCheckboxId &&
                id != NativeBridge.LicenseHeadingProxyId)
                return;

            BridgeDiagnostics.RecordState(
                "settings-write id=" + id +
                " hwnd=0x" + handle.ToInt64().ToString("X") +
                " before='" + before +
                "' desired='" + desired +
                "' after='" + after + "'");
        }
    }
}
