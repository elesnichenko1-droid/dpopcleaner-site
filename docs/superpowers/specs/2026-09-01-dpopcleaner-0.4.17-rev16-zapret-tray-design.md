# DPopCleaner 0.4.17 rev.16 — Single Tray, Zapret Functional Reliability, Unified Theme/Layout

## Цель

Revision 16 должен устранить четыре связанные проблемы в текущей архитектуре DPopCleaner 0.4.17, сохраняя frozen core 0.2.14 byte-identical и не переписывая приложение с нуля:

1. В трее должна существовать ровно одна рабочая иконка DPopCleaner. Она должна показывать текущую загрузку ОЗУ цифрами и выполнять все tray-действия DPopCleaner. Пустая/legacy иконка и отдельная вторая RAM-иконка недопустимы.
2. Zapret Center должен проверяться и исправляться как реальный функционал Windows, а не только как наличие файлов и элементов UI.
3. Все кнопки Zapret должны выглядеть единообразно в выбранной теме, не менять стиль частично после переключения темы и полностью помещаться в существующую панель.
4. На вкладке Zapret оригинальный блок «Журнал» должен быть скрыт, потому что состояние уже отображается верхним status block. На остальных вкладках журнал должен оставаться неизменным.

## Ограничения и неизменяемые части

- `DPopCleaner.Core.exe` / frozen core 0.2.14 не изменяется.
- Общий дизайн DPopCleaner 0.2.14 сохраняется.
- Другие вкладки не переделываются.
- Flowseal Zapret 1.10.2 и все 22 bundled strategy сохраняются.
- Сохраняются исправления rev.13–rev.15: UAC/code 740, language restart recovery, Settings bridge, tray restart recovery, native Zapret version, ZapretScreenFix, Disk Analyzer, Restore Center и updater.
- Новая логика реализуется через существующий `SimpleUpdate` bridge и дополнительные smoke/functional tests.

## Архитектурное решение

### 1. Single Tray Ownership

В rev.16 tray рассматривается как ресурс с одним владельцем. После запуска bridge должен добиться следующего устойчивого состояния:

- существует одна notification icon DPopCleaner;
- у неё одна стабильная `(HWND, uID)` identity на весь launcher lifetime;
- визуально она использует основную иконку DPopCleaner с RAM badge, а не отдельную вторую иконку;
- tooltip показывает DPopCleaner и процент ОЗУ;
- двойной клик восстанавливает главное окно;
- контекстное меню остаётся рабочим;
- legacy notification icon frozen core удаляется из Shell_NotifyIcon/Explorer tray и не появляется повторно;
- после self-restart core, смены языка, смены темы и `TaskbarCreated` остаётся одна и та же bridge-owned tray identity;
- после обычного выхода приложения notification icon удаляется и launcher не остаётся в фоне.

Текущий `TrayRamBadgeHost` сохраняет идею стабильного message HWND, но suppression меняется с «периодически удалять лишние записи» на явную reconciliation-модель: определить все DPopCleaner tray identities, сохранить только канонический bridge `(HWND,uID)`, удалить legacy/ghost entries и повторять reconciliation после core restart и Explorer restart.

Acceptance invariant: `bridge_unique == 1`, `legacy_core_unique == 0`, `other_launcher_duplicates == 0`.

### 2. Zapret Functional Controller + Real Lifecycle Verification

Текущий Zapret UI smoke недостаточен: наличие `service.bat`, `winws.exe`, WinDivert и стратегии в ComboBox не доказывает работу Zapret.

Rev.16 вводит слой functional verification вокруг существующего Flowseal runtime. Он не заменяет Flowseal, а проверяет фактическое состояние Windows после действий пользователя.

Обязательные сценарии installed functional test:

1. Проверить bundled files и точную версию 1.10.2.
2. Проверить наличие 22 стратегий и выбрать реальную `general*.bat`.
3. Выполнить установку/применение через тот же путь, который использует Zapret Center.
4. Убедиться, что ожидаемые Windows service/driver/runtime entities действительно созданы.
5. Запустить winws и убедиться, что соответствующий процесс/сервис реально находится в рабочем состоянии.
6. Проверить, что верхний status block отражает реальное ON-состояние.
7. Остановить Zapret и убедиться, что runtime остановлен и status block показывает OFF.
8. Выбрать вторую стратегию, применить её и подтвердить, что используется новый strategy command line/configuration, а не только изменился ComboBox.
9. Проверить удаление сервисов и отсутствие остатков после удаления.
10. В `finally` всегда очищать service/process/driver state runner-а.

Дополнительные bridge actions (`Починка трансляции`, `Починка подключения`, `Игровой фильтр 1.10.2`, `Менеджер 1.10.2`) получают отдельные contract/smoke assertions. Для действий, которые меняют состояние, тест должен проверять observable effect, а не только отсутствие исключения.

Если bundled Flowseal command завершился с ошибкой, bridge обязан показать пользователю конкретный код/команду/этап, а не общий «Zapret не работает».

### 3. Unified Zapret Theme and Layout Layer

Rev.16 не заменяет существующую вкладку Zapret новым экраном. Вместо этого создаётся единый presentation layer для native controls и bridge controls.

Требования:

- bridge определяет текущую тему из native theme state, а не жёстко применяет `DarkMode_Explorer`;
- все Zapret buttons получают согласованный стиль для light/dark theme;
- theme refresh выполняется атомарно после смены темы, language restart и повторного attach к core;
- кнопки не должны смешивать системный светлый стиль и принудительный тёмный стиль;
- layout рассчитывает доступную ширину фактической Zapret page;
- кнопки имеют единый высотный ритм и gap;
- текст не обрезается; при необходимости ширины перераспределяются по measured text/minimum width;
- ни одна кнопка не выходит за правую границу существующей панели;
- native buttons не перекрываются bridge-hosts;
- изменение DPI/scale не должно ломать размещение.

Рекомендуемый подход: один `ZapretPresentationHost`, который владеет вычислением theme + geometry, а `ZapretEnhancementHost` предоставляет только команды/controls. Это уменьшает расхождение между кнопками и исключает разрозненные `SetWindowTheme` вызовы.

### 4. Journal Visibility Policy

Оригинальный journal не удаляется и не пересоздаётся.

При активной вкладке Zapret bridge скрывает только native controls, относящиеся к заголовку `Журнал` и его большому журнал-полю. Верхний status block остаётся видимым и становится единственным постоянным status output Zapret page.

При уходе с Zapret:

- журнал возвращается;
- HWND остаются теми же;
- исходные bounds, font, text, visibility state и Z-order восстанавливаются;
- другие вкладки не получают layout изменений.

После language/theme/core restart bridge заново определяет Zapret page и применяет ту же policy без накопления offsets или дубликатов.

## Компоненты

### Tray

Предполагаемые изменения:

- `TrayRamBadgeHost.cs` — каноническая single-icon identity и publish/update lifecycle.
- `BridgeTrayGhostSuppressor.cs` / `LegacyTrayIconSuppressor` — reconciliation всех DPopCleaner tray entries вместо частичной очистки.
- `LauncherContext.cs` — явные reconciliation hooks после attach/restart/theme-language lifecycle.
- tray smoke scripts — проверка именно одной working identity и отсутствия legacy/ghost icons.

### Zapret functional layer

Предполагаемые изменения:

- `ZapretEnhancementHost.cs` — действия должны возвращать/обновлять проверяемое состояние.
- новый небольшой helper/controller для запуска Flowseal commands и чтения фактического состояния Windows, если это позволит отделить process/service logic от UI.
- `dpop0417_zapret_ui_smoke.ps1` остаётся UI/discovery тестом.
- новый installed `rev16_zapret_functional_smoke.ps1` проверяет service/winws lifecycle.

### Zapret presentation

Предполагаемые изменения:

- единый theme/layout helper/host для Zapret controls;
- исключение hard-coded `DarkMode_Explorer` как безусловного выбора;
- explicit restore logic для native journal controls.

## Поток состояний

### Запуск приложения

1. Launcher запускает/подхватывает core.
2. Bridge определяет main HWND.
3. Tray reconciliation удаляет legacy/ghost entries и публикует единственную canonical RAM-badged icon.
4. При открытии Zapret bridge создаёт/показывает enhancement/presentation hosts.
5. Presentation host читает текущую тему и рассчитывает geometry.
6. Journal policy скрывает журнал только пока Zapret page активна.

### Смена темы

1. Frozen core применяет тему штатным образом.
2. Bridge обнаруживает theme state change.
3. Zapret presentation выполняет единый refresh всех Zapret buttons/hosts.
4. Geometry пересчитывается один раз и применяется атомарно.
5. Tray identity не меняется.

### Language/core restart

1. Rev.15 restart recovery перепривязывает launcher к successor core PID.
2. Все старые HWND references сбрасываются.
3. Tray reconciliation сохраняет одну canonical tray identity и удаляет legacy icon нового core.
4. Zapret presentation/journal policy перепривязываются к новым native HWND.

## Ошибки и восстановление

- Неудачная команда Zapret не должна оставлять CI runner или пользовательскую систему в частично установленном тестовом состоянии, насколько это возможно автоматически восстановить.
- Functional smoke использует `try/finally` cleanup и фиксирует диагностический report: service state, process list, selected strategy, command line, status text и exit codes.
- Если Explorer toolbar introspection недоступен, tray test не считается успешным только по наличию `TrayRamBadgeHost`; требуется доказать absence legacy/duplicate identities либо явно упасть с диагностикой.
- UI theme/layout failure должен логировать HWND, control id/text, bounds, current theme и available page width.

## TDD и тестовая матрица

Реализация выполняется RED → GREEN. До production-кода добавляются failing contracts/smokes на каждый пользовательский симптом.

Минимальная матрица:

| Сценарий | Требование |
|---|---|
| Fresh launch | 1 tray icon, numeric RAM badge, legacy=0 |
| Core language restart | та же одна tray identity, Settings/Zapret bridge восстановлены |
| Explorer restart | одна tray icon пересоздана без дубля |
| Light theme Zapret | все buttons одного light-style, bounds внутри page |
| Dark theme Zapret | все buttons одного dark-style, bounds внутри page |
| Theme switch | нет mixed style, нет clipping/overlap |
| Zapret active | Journal hidden, upper status visible |
| Leave Zapret | Journal восстановлен без изменения других вкладок |
| Install/start Zapret | service/runtime реально ON, status UI соответствует |
| Stop Zapret | runtime реально OFF, status UI соответствует |
| Change strategy | observable runtime/config effect соответствует выбранной стратегии |
| Remove services | service/runtime/driver остатки отсутствуют |
| Close app | launcher/core/tray icon корректно завершаются |

## Release gate rev.16

Revision 16 нельзя публиковать, пока одновременно не зелёные:

- unit/contract tests;
- existing rev.13–rev.15 regressions;
- single-tray installed smoke;
- Zapret real installed functional lifecycle smoke;
- light/dark Zapret visual/layout smoke;
- journal visibility smoke;
- language restart + tray + Zapret reattach smoke;
- production candidate build/install;
- post-publish live manifest + installer SHA-256 verification.

## Не входит в rev.16

- перенос приложения на C++/новый core;
- изменение frozen 0.2.14 UI целиком;
- новая система тем для остальных вкладок;
- новая версия Flowseal Zapret выше 1.10.2;
- изменение общего дизайна сайта/установщика, кроме release identity и release notes;
- удаление журнала из других вкладок.

## Критерий готовности

Rev.16 считается готовым только если на реально установленной сборке пользователь видит одну рабочую DPopCleaner tray icon с RAM percentage, Zapret проходит реальный start/stop/install/remove lifecycle, все кнопки Zapret единообразны и полностью помещаются в light/dark theme, а журнал отсутствует только на Zapret и возвращается без изменений на остальных страницах.
