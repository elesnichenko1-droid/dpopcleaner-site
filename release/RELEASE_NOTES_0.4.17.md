# DPopCleaner 0.4.17 rev.14

DPopCleaner 0.4.17 rev.14 исправляет возврат старого интерфейса Настроек при смене языка, не меняя основной дизайн и frozen core. Оригинальное ядро 0.2.14 остаётся byte-identical как `{app}\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 14

- Исправлена смена языка в **Settings**: bridge больше не определяет открытость страницы по левому native checkbox, который сам скрывается overlay. Страница определяется по стабильным правым native-контролам, поэтому host Настроек не исчезает после выбора `English`.
- Bridge-owned элементы Настроек синхронизируют подписи с реальным native **Language ComboBox**. При переключении на English переводятся proxy-checkboxes, автообновление приложения, кнопка проверки обновлений и секция License.
- Устранена коллизия Win32 control ID: proxy ID `1490/1493/1500–1505` теперь ищутся только внутри bridge host `1492`, а не рекурсивно от главного окна, где frozen core может иметь другие descendants с теми же ID.
- Добавлены authentic runtime-smoke и installed-package smoke: оба реально открывают Settings, выбирают `English` в native ComboBox и требуют сохранения host `1492` и английских proxy-контролов. Installed-проверка печатает `INSTALLED_SETTINGS_LANGUAGE_SWITCH_SMOKE_OK`.
- Основное окно, вкладки, Settings layout, Zapret Center и общий дизайн не переделывались.

## Сохранено из rev.13 и предыдущих revision

- Исправлен post-install сценарий `CreateProcessAsUser` / **code 740**: `SimpleUpdate.exe` имеет `requestedExecutionLevel=asInvoker`, а launcher сам проверяет административный token и при необходимости перезапускает себя через Shell `runas` с сохранением аргументов.
- В Inno Setup post-install запуск сохранён через `runascurrentuser`, чтобы первый запуск шёл в контексте исходного пользователя и уже затем выполнял контролируемое UAC-повышение.
- RAM-индикатор в трее использует стабильную native identity `Shell_NotifyIcon` с одним постоянным callback HWND/uID. Legacy tray-иконка frozen core подавляется; bridge очищает ghost-записи Explorer своего launcher PID, сохраняя только живой `DPopCleaner.TrayRamBadgeHost` с `uID=1`.
- Родная строка версии Zapret формируется неизменным ядром через `{app}\Zapret\utils\dpop_version.txt`; bundled версия остаётся **Flowseal Zapret 1.10.2**.
- Полный Flowseal Zapret 1.10.2 и все **22 стратегии** сохранены.
- Bridge не переписывает родную строку версии Zapret через HWND и не создаёт version proxy; proxy ID `1726` отсутствует.
- Сохраняются тёмные bridge-кнопки, «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», рабочая замена frozen-updater и `DPopUpdate.exe` compatibility.
- Сохраняется исправление Settings против слияния/«улетания» элементов: фиксированные bounds, атомарный `DeferWindowPos` и одна перерисовка после реального scroll.
- Во вкладке ОЗУ используется существующий ComboBox 5–95%.
- `ZapretScreenFix.exe` и исправление демонстрации экрана Zapret сохранены; Disk Analyzer и Restore Center также остаются в комплекте.
- Автообновление приложения и ручная проверка обновлений в Настройках сохранены.

## Проверка rev.14

Windows CI собирает настоящий Inno Setup installer и проверяет установленный пакет. Новый installed Settings smoke воспроизводит смену языка на `English` и требует, чтобы современный Settings bridge не исчезал и не уступал место прежнему интерфейсу.

Одновременно `dpop0417_rev13_uac_tray_smoke.ps1` продолжает проверять `asInvoker` / UAC и живые tray identities Explorer: у bridge должен остаться ровно один `(HWND,uID)`, а у frozen core — ни одной legacy tray-записи. Сохраняются installed-smoke rev.12 для native Zapret version/screenshot, rev.9 updater click-smoke, проверки RAM, Zapret 1.10.2/22 стратегий, Disk Analyzer, Restore Center и byte-identical frozen core.

## Публикация

Stable manifest для **revision 14** публикуется только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev14`, публикует Pages и повторно скачивает живой installer для проверки SHA-256 и размера.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх rev.13. После установки запускайте обычный `DPopCleaner.exe`.
