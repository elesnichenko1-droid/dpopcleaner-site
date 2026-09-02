# DPopCleaner 0.4.17 rev.16

DPopCleaner 0.4.17 rev.16 исправляет tray, реальный runtime Zapret и визуальную согласованность вкладки Zapret, не меняя frozen core 0.2.14. Оригинальное ядро остаётся byte-identical как `{app}\DPopCleaner.Core.exe` с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 16

- В системном трее остаётся **одна** рабочая DPopCleaner tray-иконка с цифровым процентом ОЗУ. Tray reconciliation продолжает использовать `Shell_NotifyIcon`; после перезапуска core или Explorer canonical identity восстанавливается без второй ghost-иконки. Один canonical `(HWND,uID)` живёт весь lifetime launcher, сохраняется при self-restart frozen core, а stale/legacy Explorer tray-записи удаляются по фактической DPopCleaner tray identity, включая ghost-записи Explorer с уже уничтоженным owner HWND.
- Сохранено исправление rev.15: смена языка может перезапустить `DPopCleaner.Core.exe`, launcher перепривязывается к successor PID, а Settings bridge и RAM tray продолжают работать без второго значка.
- Zapret Center проверен реальным installed lifecycle: **Install service → Start winws → Status → Stop → смена стратегии → Start с другим command line → Remove**. Состояние определяется по реальным `zapret`, bundled `winws.exe`, `WinDivert` и `WinDivert14`, а не только по тексту интерфейса.
- Кнопка **«Установить сервис»** сохраняет штатную инициализацию Flowseal `service.bat`, но bridge подменяет только два интерактивных выбора во временной копии manager: пункт установки и индекс выбранной стратегии. Благодаря этому сохраняются штатные `GameFilterTCP/GameFilterUDP` и корректный service `ImagePath`.
- **«Запустить winws»** и **«Удалить сервисы»** получили same-bounds bridge proxy там, где frozen core 0.2.14 не совместим с текущим Flowseal 1.10.2. Исполнителем остаются bundled upstream `general*.bat`/`service.bat`.
- Remove ждёт полного исчезновения `zapret`, bundled `winws.exe`, `WinDivert` и `WinDivert14`, поэтому cleanup не обрывается посередине.
- Все видимые кнопки Zapret используют единый presentation-layer и следуют выбранной native теме. Installed pixel-smoke проверяет **Light** и **Midnight**, а native и bridge-кнопки больше не должны визуально выбиваться друг из друга. При смене темы Zapret-кнопки сразу перерисовываются в выбранной палитре.
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
