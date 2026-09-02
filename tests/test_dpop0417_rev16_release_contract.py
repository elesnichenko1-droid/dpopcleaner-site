from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev16ReleaseContractTests(unittest.TestCase):
    def test_all_release_facing_identity_is_rev16(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['version'], '0.4.17')
        self.assertEqual(version['revision'], 16)
        self.assertEqual(stable['version'], '0.4.17')
        self.assertEqual(stable['revision'], 16)
        self.assertEqual(stable['channel'], 'stable')

        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 16', program)
        self.assertIn('dpopcleaner-simpleupdate/0.4.17-rev16', launcher)

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn('number(m.revision) === 16', manifest)
        self.assertIn('v0\\.4\\.17-rev16', manifest)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('rev.16', index)

        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        for token in (
            'rev.16',
            'одна',
            'tray',
            'zapret',
            'журнал',
            'flowseal zapret 1.10.2',
            '22 стратегии',
            'efd0eff1f4962319282363fa85595c25e0cebe11',
        ):
            self.assertIn(token, notes)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        for token in (
            'release_tag: v0.4.17-rev16',
            'dpopcleaner-0.4.17-rev16-release-candidate',
            'revision=16',
            'dpop0417_rev16_single_tray_smoke.ps1',
            'dpop0417_rev16_zapret_functional_smoke.ps1',
            'dpop0417_rev16_zapret_presentation_smoke.ps1',
            'dpop0417_rev15_installed_restart_smoke.ps1',
            'v0\\.4\\.17-rev16',
        ):
            self.assertIn(token, workflow)

        installed = workflow.index('dpop0417_install_smoke.ps1')
        restart = workflow.index('dpop0417_rev15_installed_restart_smoke.ps1')
        tray = workflow.index('dpop0417_rev16_single_tray_smoke.ps1')
        functional = workflow.index('dpop0417_rev16_zapret_functional_smoke.ps1')
        presentation = workflow.index('dpop0417_rev16_zapret_presentation_smoke.ps1')
        payload = workflow.index('build publication payload')
        self.assertLess(installed, restart)
        self.assertLess(restart, tray)
        self.assertLess(tray, functional)
        self.assertLess(functional, presentation)
        self.assertLess(presentation, payload)


if __name__ == '__main__':
    unittest.main()
