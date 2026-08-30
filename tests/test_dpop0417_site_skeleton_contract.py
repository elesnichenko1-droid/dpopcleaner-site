from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SUPPORT_EMAIL = 'elesnichenko1@gmail.com'


class SiteSkeletonContractTests(unittest.TestCase):
    def test_homepage_has_complete_product_site_skeleton(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in (
            'id="features"',
            'id="screens"',
            'id="download"',
            'id="license"',
            'id="support"',
            'id="faq"',
            'href="#download"',
            'href="#license"',
            'href="#support"',
            'очистка системы',
            'мониторинг оперативной памяти',
            'zapret center',
            'анализатор диска',
            'центр восстановления',
            'автообновлен',
        ):
            self.assertIn(token, index)

    def test_hero_describes_product_usefulness_not_release_changes(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        hero_start = index.index('<section class="hero')
        hero_end = index.index('</section>', hero_start)
        hero = index[hero_start:hero_end]
        for token in (
            'чистее система',
            'очистка системы',
            'оперативной памяти',
            'zapret center',
            'анализатор диска',
            'центр восстановления',
        ):
            self.assertIn(token, hero)
        for forbidden in ('1.9.9d', 'frozen-core', 'wm_settext', 'исправляет оставшуюся строку'):
            self.assertNotIn(forbidden, hero)

    def test_download_block_keeps_live_release_contract(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        for token in ('js-download', 'js-download-label', 'js-size', 'hashvalue', 'releasestatus'):
            self.assertIn(token, index)
        for token in ('loadmanifest', 'applymanifest', 'download_url', 'sha256'):
            self.assertIn(token, script)

    def test_support_and_basic_license_purchase_use_confirmed_support_email(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        for token in (
            'id="supportform"',
            'id="supportname"',
            'id="supportemail"',
            'id="supportmessage"',
            'class="button button-primary js-license-buy',
            'js-support-email',
            SUPPORT_EMAIL,
            'покупка лицензии dpopcleaner',
            'платёжный модуль пока не подключён',
            'базовый режим покупки',
        ):
            self.assertIn(token, index + script)
        self.assertIn(f"supportemail: '{SUPPORT_EMAIL}'", script)
        self.assertIn('buildlicensemailto', script)
        self.assertIn('mailto:', script)
        self.assertIn('licensepurchaseurl', script)

    def test_mobile_navigation_and_accessibility_hooks_are_preserved(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        styles = (ROOT / 'styles.css').read_text(encoding='utf-8').lower()
        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('class="menu-button"', index)
        self.assertIn('aria-expanded="false"', index)
        self.assertIn('@media(max-width:900px)', styles)
        self.assertIn("nav.classlist.toggle('open')", script)

    def test_site_shell_styles_are_in_every_public_staging_path(self):
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        stage = (ROOT / 'scripts/Stage-Site.ps1').read_text(encoding='utf-8').lower()
        publisher = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        static = (ROOT / '.github/workflows/static.yml').read_text(encoding='utf-8').lower()
        self.assertIn('site-shell.css', index)
        self.assertIn('site-shell.css', stage)
        self.assertIn('site-shell.css', publisher)
        self.assertIn('site-shell.css', static)


if __name__ == '__main__':
    unittest.main()
