from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev18ReleaseContractTests(unittest.TestCase):
    def test_all_current_release_facing_identity_is_rev18(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['version'], '0.4.17')
        self.assertEqual(version['revision'], 18)
        self.assertEqual(stable['version'], '0.4.17')
        self.assertEqual(stable['revision'], 18)
        self.assertEqual(stable['channel'], 'stable')

        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 18', program)

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn('number(m.revision) === 18', manifest)
        self.assertIn('v0\\.4\\.17-rev18', manifest)

        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        for token in (
            'rev.18',
            'revision 18',
            'responsive',
            '1908',
            'flowseal zapret 1.10.2',
            '22 стратегии',
            'efd0eff1f4962319282363fa85595c25e0cebe11',
        ):
            self.assertIn(token, notes)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        for token in (
            'rev.18',
            'release_tag: v0.4.17-rev18',
            'dpopcleaner-0.4.17-rev18-release-candidate',
            'revision=18',
            'dpop0417-rev18-publish',
            # Older smoke names remain regression gates for the behaviors introduced there.
            'dpop0417_rev16_single_tray_smoke.ps1',
            'dpop0417_rev16_zapret_functional_smoke.ps1',
            'dpop0417_rev16_zapret_presentation_smoke.ps1',
            'dpop0417_rev15_installed_restart_smoke.ps1',
            'v0\\.4\\.17-rev18',
        ):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 18', workflow)

        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8').lower()
        self.assertIn('dpop0417_rev17_zapret_responsive_smoke.ps1', install_smoke)
        self.assertIn('installed rev.17 multi-width/dpi-aware zapret responsive layout smoke: pass', install_smoke)

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