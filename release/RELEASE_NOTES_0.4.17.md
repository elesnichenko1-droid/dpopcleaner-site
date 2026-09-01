# DPopCleaner 0.4.17 rev.15

DPopCleaner 0.4.17 rev.15 исправляет полный сценарий смены языка, при котором frozen core перезапускает себя и раньше оставлял новый процесс без enhanced Settings bridge и без цифровой RAM tray-иконки. Основной дизайн не меняется, а оригинальное ядро 0.2.14 остаётся byte-identical как `{app}\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 15

- Launcher больше не считает выход старого `DPopCleaner.Core.exe` окончательным закрытием сразу же. После выхода core он даёт короткое окно для появления нового процесса **из того же установленного пути** и с более поздним временем старта.
- Если смена языка вызывает штатный перезапуск frozen core, launcher перепривязывается к successor PID через `TryAttachRestartedCore`, сбрасывает старые bridge-hosts через `ResetBridgeForRestartedCore` и заново подключает новое главное окно.
- После такого перезапуска повторно создаётся enhanced **Settings** host и возвращается цифровой RAM **tray**-host `DPopCleaner.TrayRamBadgeHost`; legacy tray-иконка frozen core по-прежнему не должна появляться.
- Обычное закрытие приложения без successor-процесса остаётся обычным закрытием: после grace-window launcher завершается и не зависает в фоне.
- Добавлены отдельные runtime и installed smoke-проверки. Они требуют смену PID, сохранение launcher, повторное появление Settings host `1492` и восстановление цифровой RAM tray-иконки. Installed-проверка печатает `REV15_INSTALLED_LANGUAGE_RESTART_BRIDGE_SMOKE_OK` и `REV15_INSTALLED_LANGUAGE_RESTART_RAM_TRAY_SMOKE_OK`.
- Исправление rev.14 по синхронизации подписей с native Language ComboBox сохранено: после смены языка bridge-owned элементы продолжают соответствовать выбранному языку, а прежний интерфейс не должен возвращаться.
- Основное окно, вкладки, Settings layout, Zapret Center и общий дизайн не переделывались.

## Сохранено из rev.14, rev.13 и предыдущих revision

- Исправлена смена языка в **Settings**: bridge определяет страницу по стабильным native-контролам, а proxy-элементы локализуются по реальному native Language ComboBox.
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

## Проверка rev.15

Windows CI собирает настоящий Inno Setup installer и проверяет установленный пакет. Новый installed restart smoke воспроизводит замену frozen core на новый PID и требует, чтобы тот же launcher подхватил successor-процесс, восстановил enhanced Settings и снова создал цифровую RAM tray-иконку.

Одновременно `dpop0417_rev13_uac_tray_smoke.ps1` продолжает проверять `asInvoker` / UAC и живые tray identities Explorer: у bridge должен остаться ровно один `(HWND,uID)`, а у frozen core — ни одной legacy tray-записи. Сохраняются installed-smoke rev.14 для смены языка, rev.12 для native Zapret version/screenshot, rev.9 updater click-smoke, проверки RAM, Zapret 1.10.2/22 стратегий, Disk Analyzer, Restore Center и byte-identical frozen core.

## Публикация

Stable manifest для **revision 15** публикуется только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev15`, публикует Pages и повторно скачивает живой installer для проверки SHA-256 и размера.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх rev.14. После установки запускайте обычный `DPopCleaner.exe`.
