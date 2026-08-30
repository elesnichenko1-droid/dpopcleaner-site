from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev13UacTrayContractTests(unittest.TestCase):
    def test_launcher_uses_asinvoker_and_self_elevates_with_argument_preservation(self):
        manifest = (ROOT / 'v0417/src/SimpleUpdate/app.manifest').read_text(encoding='utf-8').lower()
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        elevation = (ROOT / 'v0417/src/SimpleUpdate/ElevationBootstrap.cs').read_text(encoding='utf-8').lower()
        installer = (ROOT / 'release/DPopCleaner_0.4.17.iss').read_text(encoding='utf-8').lower()

        self.assertIn('requestedexecutionlevel level="asinvoker"', manifest)
        self.assertNotIn('requireadministrator', manifest)
        self.assertIn('elevationbootstrap.ensureadministrator(args)', program)
        self.assertIn('windowsprincipal', elevation)
        self.assertIn('windowsbuiltinrole.administrator', elevation)
        self.assertIn('verb = "runas"', elevation)
        self.assertIn('useshellexecute = true', elevation)
        self.assertIn('buildarguments', elevation)
        self.assertIn('runascurrentuser', installer)

    def test_ram_badge_uses_one_stable_native_tray_identity_and_suppresses_legacy_core_icon(self):
        tray = (ROOT / 'v0417/src/SimpleUpdate/TrayRamBadgeHost.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        csproj = (ROOT / 'v0417/src/SimpleUpdate/SimpleUpdate.csproj').read_text(encoding='utf-8').lower()

        self.assertIn('notifyicondata', tray)
        self.assertIsNone(re.search(r'\bnew\s+notifyicon\s*(?:\{|\()', tray))
        self.assertIn('createhandle', tray)
        self.assertIn('nim_add', tray)
        self.assertIn('nim_modify', tray)
        self.assertIn('nim_delete', tray)
        self.assertIn('shell_notifyicon', tray)
        self.assertIn('globalmemorystatusex', tray)
        self.assertIn('dwmemoryload', tray)
        self.assertIn('drawstring', tray)
        self.assertIn('1000', tray)
        self.assertIn('legacytrayiconsuppressor', tray)
        self.assertIn('tb_getbutton', tray)
        self.assertIn('getwindowthreadprocessid', tray)
        self.assertIn('_trayramhost', launcher)
        self.assertIn('update(_core.id', launcher)
        self.assertIn('system.drawing', csproj)

    def test_rev13_has_installed_runtime_smoke_for_uac_and_single_ram_tray_icon(self):
        smoke = (ROOT / 'tools/dpop0417_rev13_uac_tray_smoke.ps1').read_text(encoding='utf-8').lower()
        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8').lower()

        for token in ('code 740', 'requestedexecutionlevel', 'tray', 'ram', 'one tray icon', 'rev13_uac_tray_smoke_ok'):
            self.assertIn(token, smoke)
        self.assertIn('uniqueidentitiesforprocess', smoke)
        self.assertIn('dpop0417_rev13_uac_tray_smoke.ps1', install_smoke)


if __name__ == '__main__':
    unittest.main()
