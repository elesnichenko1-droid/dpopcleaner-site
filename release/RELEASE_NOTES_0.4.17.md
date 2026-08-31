# DPopCleaner 0.4.17 rev.13

DPopCleaner 0.4.17 rev.13 исправляет два оставшихся runtime-дефекта вокруг запуска и трея, не меняя основной интерфейс и frozen core. Оригинальное ядро 0.2.14 остаётся byte-identical как `{app}\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 13

- Исправлен post-install сценарий `CreateProcessAsUser` / **code 740**: `SimpleUpdate.exe` теперь имеет `requestedExecutionLevel=asInvoker`, а launcher сам проверяет административный token и при необходимости перезапускает себя через Shell `runas` с сохранением аргументов.
- В Inno Setup post-install запуск сохранён через `runascurrentuser`, чтобы первый запуск шёл в контексте исходного пользователя и уже затем выполнял контролируемое UAC-повышение.
- RAM-индикатор в трее переведён на стабильную native identity `Shell_NotifyIcon` с одним постоянным callback HWND/uID. Значок показывает текущий процент использования ОЗУ и обновляется без пересоздания tray identity.
- Legacy tray-иконка frozen core подавляется. Дополнительно bridge очищает ghost-записи Explorer своего launcher PID, сохраняя только живой `DPopCleaner.TrayRamBadgeHost` с `uID=1`, поэтому после пересоздания старого HWND не должна оставаться вторая иконка.
- Двойной клик по RAM-иконке восстанавливает главное окно; контекстное меню сохраняет действие открытия DPopCleaner.
- Основное окно, вкладки, Settings, Zapret Center и дизайн не переделывались.

## Сохранено из rev.12 и предыдущих revision

- Родная строка версии Zapret формируется неизменным ядром через `{app}\Zapret\utils\dpop_version.txt`; bundled версия остаётся **Flowseal Zapret 1.10.2**.
- Полный Flowseal Zapret 1.10.2 и все **22 стратегии** сохранены.
- Bridge не переписывает родную строку версии Zapret через HWND и не создаёт version proxy; proxy ID `1726` отсутствует.
- Сохраняются тёмные bridge-кнопки, «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», рабочая замена frozen-updater и `DPopUpdate.exe` compatibility.
- Сохраняется исправление Settings против слияния/«улетания» элементов: фиксированные bounds, атомарный `DeferWindowPos` и одна перерисовка после реального scroll.
- Во вкладке ОЗУ используется существующий ComboBox 5–95%.
- `ZapretScreenFix.exe` и исправление демонстрации экрана Zapret сохранены; Disk Analyzer и Restore Center также остаются в комплекте.
- Автообновление приложения и ручная проверка обновлений в Настройках сохранены.

## Проверка rev.13

Windows CI собирает настоящий Inno Setup installer и проверяет установленный пакет. `dpop0417_rev13_uac_tray_smoke.ps1` извлекает manifest установленного launcher и требует `asInvoker` без `requireAdministrator`, затем проверяет живые tray identities Explorer: у bridge должен остаться ровно один `(HWND,uID)`, а у frozen core — ни одной legacy tray-записи. Диагностика записывает class/title/thread callback HWND, чтобы ghost-запись с уничтоженным окном не маскировалась под живую иконку.

Одновременно сохраняются installed-smoke rev.12 для native Zapret version/screenshot, rev.9 updater click-smoke, проверки Settings, RAM, Zapret 1.10.2/22 стратегий, Disk Analyzer, Restore Center и byte-identical frozen core.

## Публикация

Stable manifest для **revision 13** публикуется только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev13`, публикует Pages и повторно скачивает живой installer для проверки SHA-256 и размера.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх rev.12. После установки запускайте обычный `DPopCleaner.exe`.
