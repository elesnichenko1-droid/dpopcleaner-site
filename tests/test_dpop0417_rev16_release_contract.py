from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev16RegressionContractTests(unittest.TestCase):
    def test_rev16_behaviors_remain_release_gates_under_rev19(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 19)
        self.assertEqual(stable['revision'], 19)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        for token in (
            'release_tag: v0.4.17-rev19',
            'dpop0417_rev15_installed_restart_smoke.ps1',
            'dpop0417_rev16_single_tray_smoke.ps1',
            'dpop0417_rev16_zapret_functional_smoke.ps1',
            'dpop0417_rev16_zapret_presentation_smoke.ps1',
            'dpop0417_rev19_installed_zapret_cleanup_smoke.ps1',
        ):
            self.assertIn(token, workflow)
        self.assertNotIn('release_tag: v0.4.17-rev16', workflow)
        self.assertNotIn('release_tag: v0.4.17-rev17', workflow)
        self.assertNotIn('release_tag: v0.4.17-rev18', workflow)

        installed = workflow.index('dpop0417_install_smoke.ps1')
        restart = workflow.index('dpop0417_rev15_installed_restart_smoke.ps1')
        tray = workflow.index('dpop0417_rev16_single_tray_smoke.ps1')
        functional = workflow.index('dpop0417_rev16_zapret_functional_smoke.ps1')
        presentation = workflow.index('dpop0417_rev16_zapret_presentation_smoke.ps1')
        rev19 = workflow.index('dpop0417_rev19_installed_zapret_cleanup_smoke.ps1')
        payload = workflow.index('build publication payload')
        self.assertLess(installed, restart)
        self.assertLess(restart, tray)
        self.assertLess(tray, functional)
        self.assertLess(functional, presentation)
        self.assertLess(presentation, rev19)
        self.assertLess(rev19, payload)

        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        for token in ('rev.16', 'tray', 'zapret', 'журнал', 'flowseal zapret 1.10.2', '22 стратегии'):
            self.assertIn(token, notes)


if __name__ == '__main__':
    unittest.main()
