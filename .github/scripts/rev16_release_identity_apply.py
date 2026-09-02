from pathlib import Path
import json

ROOT = Path('.')


def replace_exact(path, old, new, expected=1):
    p = ROOT / path
    text = p.read_text(encoding='utf-8')
    actual = text.count(old)
    if actual != expected:
        raise SystemExit(f'{path}: expected {expected} occurrences of {old!r}, found {actual}')
    p.write_text(text.replace(old, new), encoding='utf-8')


replace_exact('v0417/src/SimpleUpdate/Program.cs',
              'internal const int CurrentRevision = 15;',
              'internal const int CurrentRevision = 16;')
replace_exact('v0417/src/SimpleUpdate/LauncherContext.cs',
              'DPopCleaner-SimpleUpdate/0.4.17-rev15',
              'DPopCleaner-SimpleUpdate/0.4.17-rev16')

for path in ('version.json', 'update/stable.json'):
    p = ROOT / path
    data = json.loads(p.read_text(encoding='utf-8'))
    if data.get('version') != '0.4.17' or data.get('revision') != 15:
        raise SystemExit(f'{path}: expected 0.4.17 revision 15 before promotion, got {data!r}')
    data['revision'] = 16
    p.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

replace_exact('release-manifest.js', 'v0\\.4\\.17-rev15', 'v0\\.4\\.17-rev16')
replace_exact('release-manifest.js', 'Number(m.revision) === 15', 'Number(m.revision) === 16')

index = ROOT / 'index.html'
index_text = index.read_text(encoding='utf-8')
if index_text.count('rev.15') < 5:
    raise SystemExit('index.html: expected at least five current rev.15 labels before promotion')
index.write_text(index_text.replace('rev.15', 'rev.16'), encoding='utf-8')

notes = r'''# DPopCleaner 0.4.17 rev.16

DPopCleaner 0.4.17 rev.16 исправляет tray, реальный runtime Zapret и визуальную согласованность вкладки Zapret, не меняя frozen core 0.2.14. Оригинальное ядро остаётся byte-identical как `{app}\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 16

- В системном трее остаётся **одна** рабочая DPopCleaner tray-иконка с цифровым процентом ОЗУ. Один canonical `(HWND,uID)` живёт весь lifetime launcher, сохраняется при self-restart frozen core, а stale/legacy Explorer tray-записи удаляются по фактической DPopCleaner tray identity, включая ghost-записи Explorer с уже уничтоженным owner HWND.
- Сохранено исправление rev.15: смена языка может перезапустить `DPopCleaner.Core.exe`, launcher перепривязывается к successor PID, а Settings bridge и RAM tray продолжают работать без второго значка.
- Zapret Center проверен реальным installed lifecycle: **Install service → Start winws → Status → Stop → смена стратегии → Start с другим command line → Remove**. Состояние определяется по реальным `zapret`, bundled `winws.exe`, `WinDivert` и `WinDivert14`, а не только по тексту интерфейса.
- Кнопка **«Установить сервис»** сохраняет штатную инициализацию Flowseal `service.bat`, но bridge подменяет только два интерактивных выбора во временной копии manager: пункт установки и индекс выбранной стратегии. Благодаря этому сохраняются штатные `GameFilterTCP/GameFilterUDP` и корректный service `ImagePath`.
- **«Запустить winws»** и **«Удалить сервисы»** получили same-bounds bridge proxy там, где frozen core 0.2.14 не совместим с текущим Flowseal 1.10.2. Исполнителем остаются bundled upstream `general*.bat`/`service.bat`.
- Remove ждёт полного исчезновения `zapret`, bundled `winws.exe`, `WinDivert` и `WinDivert14`, поэтому cleanup не обрывается посередине.
- Все видимые кнопки Zapret используют единый presentation-layer и следуют выбранной native теме. Installed pixel-smoke проверяет **Light** и **Midnight**, а native и bridge-кнопки больше не должны визуально выбиваться друг из друга.
- Убраны hardcoded `DarkMode_Explorer` и фиксированная ширина toolbar `709 px`. Четыре дополнительные Zapret-кнопки рассчитывают ширину по доступной панели и фактическому тексту, не перекрываются и не выходят за границы.
- **Журнал** скрывается только на вкладке Zapret; верхний блок статуса остаётся. При переходе на другие вкладки bridge не воскрешает Zapret HWND поверх чужой страницы и не меняет родной журнал/лог другой вкладки.
- Сохраняются «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», frozen-updater compatibility и `DPopUpdate.exe`.

## Сохранено

- Frozen core 0.2.14 остаётся byte-identical: `efd0eff1f4962319282363fa85595c25e0cebe11`.
- Полный **Flowseal Zapret 1.10.2** и все **22 стратегии** остаются в комплекте; native версия читается через `Zapret\utils\dpop_version.txt`.
- Rev.13 UAC/code 740 и `requestedExecutionLevel=asInvoker` + контролируемый `runas` сохранены.
- Rev.14 Settings language bridge, rev.15 core restart recovery, RAM threshold 5–95%, ZapretScreenFix, Disk Analyzer, Restore Center и автообновление сохранены.
- Основной интерфейс 0.2.14 не переписывается и не переносится на C++/0.4.18.

## Проверка rev.16

Production pipeline собирает настоящий Inno Setup installer и на **установленной** сборке требует:

1. обычный installed package smoke;
2. rev.15 language-restart smoke;
3. rev.16 single-tray smoke с restart frozen core и Explorer;
4. rev.16 Zapret functional lifecycle smoke;
5. rev.16 Zapret presentation smoke для Light/Midnight, layout и Journal policy;
6. существующие проверки native Zapret version/updater, Disk Analyzer, Restore Center и byte-identical frozen core.

Релиз блокируется при второй/пустой tray-иконке, если `zapret`/`winws` не достигают требуемого фактического состояния, если смена стратегии не меняет командную строку, если кнопки выходят за панель или если Журнал Zapret появляется на другой вкладке.

## Публикация

Stable manifest для **revision 16** публикуется только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev16`, публикует Pages и повторно скачивает живой installer для проверки SHA-256 и размера.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх rev.15. После установки запускайте обычный `DPopCleaner.exe`.
'''
(ROOT / 'release/RELEASE_NOTES_0.4.17.md').write_text(notes, encoding='utf-8')

foundation = ROOT / '.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml'
ft = foundation.read_text(encoding='utf-8')
anchor = "          python tests/test_dpop0417_release_contract.py -v\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n"
if ft.count(anchor) != 1:
    raise SystemExit('Foundation release-contract anchor mismatch')
foundation.write_text(ft.replace(anchor, anchor + "          python tests/test_dpop0417_rev16_release_contract.py -v\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n"), encoding='utf-8')

publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
wt = publisher.read_text(encoding='utf-8')
for old, new in (
    ('Build, release and deploy DPopCleaner 0.4.17 rev.15', 'Build, release and deploy DPopCleaner 0.4.17 rev.16'),
    ('dpopcleaner-0-4-17-rev15-publish-', 'dpopcleaner-0-4-17-rev16-publish-'),
    ('RELEASE_TAG: v0.4.17-rev15', 'RELEASE_TAG: v0.4.17-rev16'),
    ("Join-Path $env:RUNNER_TEMP 'dpop0417-rev15-publish'", "Join-Path $env:RUNNER_TEMP 'dpop0417-rev16-publish'"),
    ('dpopcleaner-0.4.17-rev15-release-candidate', 'dpopcleaner-0.4.17-rev16-release-candidate'),
    ('--title "DPopCleaner 0.4.17 rev.15"', '--title "DPopCleaner 0.4.17 rev.16"'),
    ('$live.revision -eq 15', '$live.revision -eq 16'),
    ('$live.revision -ne 15', '$live.revision -ne 16'),
    ('v0\\.4\\.17-rev15', 'v0\\.4\\.17-rev16'),
):
    if old not in wt:
        raise SystemExit(f'publisher missing expected rev15 anchor: {old!r}')
    wt = wt.replace(old, new)

# Literal revision=15 belongs only to the publication manifest payload in this workflow.
if 'revision=15' not in wt:
    raise SystemExit('publisher missing publication revision=15 anchor')
wt = wt.replace('revision=15', 'revision=16')

common_tests = "          dotnet test v0417/tests/ZapretScreenFix.Tests/ZapretScreenFix.Tests.csproj -c Release --nologo\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n"
if wt.count(common_tests) != 1:
    raise SystemExit('publisher test anchor mismatch')
wt = wt.replace(common_tests, common_tests + "          dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n")

restart_step = "      - name: Run rev.15 installed language-restart bridge and RAM tray smoke\n        shell: pwsh\n        run: ./tools/dpop0417_rev15_installed_restart_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev15-restart'\n"
if wt.count(restart_step) != 1:
    raise SystemExit('publisher rev15 restart step anchor mismatch')
rev16_steps = restart_step + "      - name: Run rev.16 installed single tray identity smoke\n        shell: pwsh\n        run: ./tools/dpop0417_rev16_single_tray_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-tray'\n      - name: Run rev.16 installed Zapret functional smoke\n        shell: pwsh\n        run: ./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'\n      - name: Run rev.16 installed Zapret presentation smoke\n        shell: pwsh\n        run: ./tools/dpop0417_rev16_zapret_presentation_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-presentation'\n"
wt = wt.replace(restart_step, rev16_steps)
publisher.write_text(wt, encoding='utf-8')

old_test = ROOT / 'tests/test_dpop0417_release_contract.py'
ot = old_test.read_text(encoding='utf-8')
replacements = (
    ('test_site_manifest_and_publisher_are_one_stable_0417_rev15_release', 'test_site_manifest_and_publisher_are_one_stable_0417_rev16_release'),
    ("self.assertEqual(version['revision'], 15)", "self.assertEqual(version['revision'], 16)"),
    ("self.assertEqual(stable['revision'], 15)", "self.assertEqual(stable['revision'], 16)"),
    ('number(m.revision) === 15', 'number(m.revision) === 16'),
    ('v0\\\\.4\\\\.17-rev15', 'v0\\\\.4\\\\.17-rev16'),
    ('currentrevision = 15', 'currentrevision = 16'),
    ('dpopcleaner-simpleupdate/0.4.17-rev15', 'dpopcleaner-simpleupdate/0.4.17-rev16'),
    ("'rev.15'", "'rev.16'"),
    ("'revision 15'", "'revision 16'"),
    ('release_tag: v0.4.17-rev15', 'release_tag: v0.4.17-rev16'),
    ('revision=15', 'revision=16'),
    ('dpopcleaner-0.4.17-rev15-release-candidate', 'dpopcleaner-0.4.17-rev16-release-candidate'),
    ('$live.revision -ne 15', '$live.revision -ne 16'),
)
for old, new in replacements:
    if old not in ot:
        raise SystemExit(f'old release contract missing anchor: {old!r}')
    ot = ot.replace(old, new)
old_test.write_text(ot, encoding='utf-8')

print('REV16_RELEASE_IDENTITY_PATCH_OK')
