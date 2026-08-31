from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def write(path, text):
    (ROOT / path).write_text(text, encoding='utf-8', newline='\n')


def replace_required(path, old, new, minimum=1):
    text = read(path)
    count = text.count(old)
    if count < minimum:
        raise SystemExit(f'{path}: expected at least {minimum} occurrence(s) of {old!r}, found {count}')
    write(path, text.replace(old, new))
    print(f'{path}: replaced {count} x {old!r}')


replace_required('v0417/src/SimpleUpdate/Program.cs', 'CurrentRevision = 12', 'CurrentRevision = 13')
replace_required('version.json', '"revision": 12', '"revision": 13')
replace_required('update/stable.json', '"revision": 12', '"revision": 13')
replace_required('release-manifest.js', 'v0\\.4\\.17-rev12', 'v0\\.4\\.17-rev13')
replace_required('release-manifest.js', 'Number(m.revision) === 12', 'Number(m.revision) === 13')

index_path = 'index.html'
index = read(index_path)
if 'rev.12' not in index:
    raise SystemExit('index.html: rev.12 baseline not found')
index = index.replace('rev.12', 'rev.13')
index = index.replace('Для rev.13 исправлена родная версия Zapret', 'Для rev.12 исправлена родная версия Zapret')
summary = '<summary>Технические детали rev.13</summary>'
if summary not in index:
    raise SystemExit('index.html: technical summary not found after revision replacement')
rev13_site_note = '<p>В rev.13 исправлен запуск после установки: launcher встроен как <code>asInvoker</code> и сам запрашивает UAC через <code>runas</code>, поэтому сценарий CreateProcessAsUser / code 740 больше не должен ломать первый запуск. В трее остаётся одна стабильная иконка DPopCleaner с текущим процентом ОЗУ; legacy-иконка frozen core и ghost-записи Explorer подавляются.</p>'
index = index.replace(summary, summary + '\n            ' + rev13_site_note, 1)
write(index_path, index)

notes = '''# DPopCleaner 0.4.17 rev.13

DPopCleaner 0.4.17 rev.13 исправляет два оставшихся runtime-дефекта вокруг запуска и трея, не меняя основной интерфейс и frozen core. Оригинальное ядро 0.2.14 остаётся byte-identical как `{app}\\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 13

- Исправлен post-install сценарий `CreateProcessAsUser` / **code 740**: `SimpleUpdate.exe` теперь имеет `requestedExecutionLevel=asInvoker`, а launcher сам проверяет административный token и при необходимости перезапускает себя через Shell `runas` с сохранением аргументов.
- В Inno Setup post-install запуск сохранён через `runascurrentuser`, чтобы первый запуск шёл в контексте исходного пользователя и уже затем выполнял контролируемое UAC-повышение.
- RAM-индикатор в трее переведён на стабильную native identity `Shell_NotifyIcon` с одним постоянным callback HWND/uID. Значок показывает текущий процент использования ОЗУ и обновляется без пересоздания tray identity.
- Legacy tray-иконка frozen core подавляется. Дополнительно bridge очищает ghost-записи Explorer своего launcher PID, сохраняя только живой `DPopCleaner.TrayRamBadgeHost` с `uID=1`, поэтому после пересоздания старого HWND не должна оставаться вторая иконка.
- Двойной клик по RAM-иконке восстанавливает главное окно; контекстное меню сохраняет действие открытия DPopCleaner.
- Основное окно, вкладки, Settings, Zapret Center и дизайн не переделывались.

## Сохранено из rev.12 и предыдущих revision

- Родная строка версии Zapret формируется неизменным ядром через `{app}\\Zapret\\utils\\dpop_version.txt`; bundled версия остаётся **Flowseal Zapret 1.10.2**.
- Полный Flowseal Zapret 1.10.2 и все **22 стратегии** сохранены.
- Bridge не переписывает родную строку версии Zapret через HWND и не создаёт version proxy; proxy ID `1726` отсутствует.
- Сохраняются тёмные bridge-кнопки, «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», рабочая замена frozen-updater и `DPopUpdate.exe` compatibility.
- Сохраняется исправление Settings против слияния/«улетания» элементов: фиксированные bounds, атомарный `DeferWindowPos` и одна перерисовка после реального scroll.
- Во вкладке ОЗУ используется существующий ComboBox 5–95%.
- `ZapretScreenFix.exe`, Disk Analyzer и Restore Center сохранены.

## Проверка rev.13

Windows CI собирает настоящий Inno Setup installer и проверяет установленный пакет. `dpop0417_rev13_uac_tray_smoke.ps1` извлекает manifest установленного launcher и требует `asInvoker` без `requireAdministrator`, затем проверяет живые tray identities Explorer: у bridge должен остаться ровно один `(HWND,uID)`, а у frozen core — ни одной legacy tray-записи. Диагностика записывает class/title/thread callback HWND, чтобы ghost-запись с уничтоженным окном не маскировалась под живую иконку.

Одновременно сохраняются installed-smoke rev.12 для native Zapret version/screenshot, rev.9 updater click-smoke, проверки Settings, RAM, Zapret 1.10.2/22 стратегий, Disk Analyzer, Restore Center и byte-identical frozen core.

## Публикация

Stable manifest для **revision 13** публикуется только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev13`, публикует Pages и повторно скачивает живой installer для проверки SHA-256 и размера.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх rev.12. После установки запускайте обычный `DPopCleaner.exe`.
'''
write('release/RELEASE_NOTES_0.4.17.md', notes)

release_test = 'tests/test_dpop0417_release_contract.py'
text = read(release_test)
replacements = [
    ('test_site_manifest_and_publisher_are_one_stable_0417_rev12_release', 'test_site_manifest_and_publisher_are_one_stable_0417_rev13_release'),
    ("self.assertEqual(version['revision'], 12)", "self.assertEqual(version['revision'], 13)"),
    ("self.assertEqual(stable['revision'], 12)", "self.assertEqual(stable['revision'], 13)"),
    ('number(m.revision) === 12', 'number(m.revision) === 13'),
    (r'v0\\.4\\.17-rev12', r'v0\\.4\\.17-rev13'),
    ('currentrevision = 12', 'currentrevision = 13'),
    ("'rev.12'", "'rev.13'"),
    ("'revision 12'", "'revision 13'"),
    ('release_tag: v0.4.17-rev12', 'release_tag: v0.4.17-rev13'),
    ('revision=12', 'revision=13'),
    ('dpopcleaner-0.4.17-rev12-release-candidate', 'dpopcleaner-0.4.17-rev13-release-candidate'),
    ('$live.revision -ne 12', '$live.revision -ne 13'),
]
for old, new in replacements:
    if old not in text:
        raise SystemExit(f'{release_test}: expected token not found: {old!r}')
    text = text.replace(old, new)
old_notes_tokens = "('revision 13', 'flowseal zapret 1.10.2', 'dpopcleaner.core.exe', 'dpopupdate.exe', 'модуль обновления zapret не найден', 'проверить версию', 'скачать и установить', '1.9.9d', 'utils\\\\dpop_version.txt', 'wm_settext', 'перерис', 'owner-draw', 'реальн', 'png')"
new_notes_tokens = "('revision 13', 'flowseal zapret 1.10.2', 'dpopcleaner.core.exe', 'dpopupdate.exe', 'модуль обновления zapret не найден', 'проверить версию', 'скачать и установить', 'utils\\\\dpop_version.txt', 'перерис', 'owner-draw', 'code 740', 'uac', 'одна', 'озу')"
if old_notes_tokens not in text:
    raise SystemExit(f'{release_test}: release-note token tuple not found')
text = text.replace(old_notes_tokens, new_notes_tokens)
old = "self.assertIn('bridge больше не ищет', notes_text)"
if old not in text:
    raise SystemExit(f'{release_test}: old release-note architecture assertion not found')
text = text.replace(old, "self.assertIn('ghost-записи explorer', notes_text)")
write(release_test, text)

screen_test = 'tests/test_dpop0417_zapret_screen_fix_contract.py'
text = read(screen_test)
text = text.replace('test_rev12_', 'test_rev13_')
for old, new in [
    ("self.assertEqual(version['revision'], 12)", "self.assertEqual(version['revision'], 13)"),
    ("self.assertEqual(stable['revision'], 12)", "self.assertEqual(stable['revision'], 13)"),
    ("self.assertIn('revision=12', workflow)", "self.assertIn('revision=13', workflow)"),
    ("self.assertIn('v0.4.17-rev12', workflow)", "self.assertIn('v0.4.17-rev13', workflow)"),
]:
    if old not in text:
        raise SystemExit(f'{screen_test}: expected token not found: {old!r}')
    text = text.replace(old, new)
write(screen_test, text)

workflow_path = '.github/workflows/publish-dpopcleaner-0.4.17.yml'
workflow = read(workflow_path)
workflow_replacements = [
    ('name: Build, release and deploy DPopCleaner 0.4.17 rev.12', 'name: Build, release and deploy DPopCleaner 0.4.17 rev.13'),
    ('dpopcleaner-0-4-17-rev12-publish-', 'dpopcleaner-0-4-17-rev13-publish-'),
    ('RELEASE_TAG: v0.4.17-rev12', 'RELEASE_TAG: v0.4.17-rev13'),
    ('Run installed package smoke (includes rev.12 native version smoke)', 'Run installed package smoke (includes rev.12 + rev.13 smokes)'),
    ("'dpop0417-rev12-publish'", "'dpop0417-rev13-publish'"),
    ('revision=12', 'revision=13'),
    ('dpopcleaner-0.4.17-rev12-release-candidate', 'dpopcleaner-0.4.17-rev13-release-candidate'),
    ('DPopCleaner 0.4.17 rev.12', 'DPopCleaner 0.4.17 rev.13'),
    ('$live.revision -eq 12', '$live.revision -eq 13'),
    ('$live.revision -ne 12', '$live.revision -ne 13'),
    (r'v0\.4\.17-rev12', r'v0\.4\.17-rev13'),
]
for old, new in workflow_replacements:
    if old not in workflow:
        raise SystemExit(f'{workflow_path}: expected token not found: {old!r}')
    workflow = workflow.replace(old, new)
write(workflow_path, workflow)

checks = {
    'v0417/src/SimpleUpdate/Program.cs': ['CurrentRevision = 13'],
    'version.json': ['"revision": 13'],
    'update/stable.json': ['"revision": 13'],
    'release-manifest.js': ['rev13', 'Number(m.revision) === 13'],
    '.github/workflows/publish-dpopcleaner-0.4.17.yml': ['RELEASE_TAG: v0.4.17-rev13', 'revision=13', 'rev13-release-candidate'],
    'release/RELEASE_NOTES_0.4.17.md': ['Revision 13', 'code 740', 'ghost-записи Explorer'],
    'index.html': ['rev.13', 'code 740', 'одна стабильная иконка'],
}
for path, tokens in checks.items():
    data = read(path)
    for token in tokens:
        if token not in data:
            raise SystemExit(f'{path}: missing rev13 guard token {token!r}')

print('REV13_RELEASE_METADATA_SYNC_OK')
