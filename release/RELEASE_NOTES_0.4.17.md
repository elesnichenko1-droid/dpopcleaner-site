# DPopCleaner 0.4.17 rev.19

DPopCleaner 0.4.17 rev.19 завершает очистку и стабилизацию интерфейса Zapret после rev.18 wide-screen gate. Frozen core 0.2.14 не переписывается: `{app}\DPopCleaner.Core.exe` остаётся byte-identical с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 19

- Переработана вертикальная компоновка Zapret на широких окнах: idle status/detail больше не растягивает страницу и не оставляет чрезмерную пустую область снизу.
- Добавлен отдельный компактный блок **«Сервисные действия» / Service actions** для Status, Remove services, Diagnostics и Tests; основные действия остаются визуально приоритетными.
- **Check version / Download** сохранены как основные действия, а Auto-update / Autostart Zapret используют компактные text-sized controls.
- Bridge-кнопки Zapret используют единый host-level owner-draw без ghost/native рамок Windows; Light и Midnight сохраняют одинаковую геометрию и оформление.
- Tall-window layout не перемещает frozen status `Edit` по Y. Нижние launcher-owned строки используют виртуальную compact-baseline границу, поэтому delayed native resize больше не создаёт два устойчивых варианта расположения.
- Для nested bridge-hosts и frozen status controls геометрия записывается только при реальном изменении, что убирает повторные SetWindowPos/repaint гонки.
- Installed gate проверяет **1024×768, 1366×800, 1680×840 и 1908×950**, отсутствие overlap/clipping, полнотекстовые подписи, единый visual style и реальный paint кнопки «Удалить сервисы» (`1702`).
- Для 1908×950 verifier ждёт стабилизации фактической HWND-геометрии и отдельно сохраняет жёсткий предел нижнего свободного пространства **≤ 210 px**.
- Canonical RAM tray проверяется циклом **ON → OFF → ON**, без legacy/ghost-дубликатов.

## Сохранено из rev.18

- Отдельный installed user-report gate для окна **1908×950** и проверка canonical tray identity.
- Выбор стратегии Zapret определяется настоящим списком `general*.bat`, а не вертикальной позицией `ComboBox`.
- Для реальной установки Flowseal service сохраняется увеличенный startup budget до **30 секунд**.
- Functional smoke диагностирует точный proxy `1701`, выбранную стратегию, временный service installer и `cmd.exe`, сохраняя cleanup при ошибке.

## Сохранено из rev.17

- Вся рабочая область Zapret использует единый **responsive layout**.
- Строки стратегии, управления сервисом, обновления и дополнительных действий пересчитываются по фактическому размеру окна.
- Layout учитывает native размеры контролов и DPI; вложенные button-группы перемещаются как группы.
- Multi-width regression smoke сохраняется для **1024×768**, **1366×800** и **1680×840**.

## Сохранено из rev.16 и более ранних ревизий

- Одна рабочая RAM **tray**-иконка с цифровым процентом ОЗУ; восстановление canonical tray identity после restart frozen core и Explorer без ghost-иконки.
- Перепривязка launcher к новому `DPopCleaner.Core.exe` после смены языка; Settings bridge и RAM tray переживают self-restart.
- Реальный Zapret lifecycle: **Install service → Start winws → Status → Stop → смена стратегии → Start → Remove**.
- Единое оформление Zapret-кнопок в **Light** и **Midnight**.
- **Журнал** скрывается только на вкладке Zapret и остаётся без изменений на остальных вкладках.
- «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», frozen-updater compatibility и `DPopUpdate.exe`.
- Исправление демонстрации экрана / screen share для ZapretScreenFix.
- Автообновление приложения, Disk Analyzer, Restore Center, UAC/**code 740** fix и RAM threshold **ОЗУ 5–95%**.

## Не меняется

- Frozen core 0.2.14: `efd0eff1f4962319282363fa85595c25e0cebe11`.
- Полный **Flowseal Zapret 1.10.2** и все **22 стратегии**.
- Native версия Zapret читается через `Zapret\utils\dpop_version.txt`.
- Основной интерфейс остаётся интерфейсом DPopCleaner 0.2.14; переход на C++/0.4.18 в rev.19 не выполняется.

## Проверка rev.19

Production pipeline блокирует публикацию, пока не пройдут:

1. общие unit/contract/build проверки;
2. установленный package smoke;
3. rev.15 language-restart + RAM tray smoke;
4. rev.16 single-tray regression smoke;
5. rev.16 Zapret functional lifecycle regression smoke с реальным Flowseal service/winws;
6. rev.16 Light/Midnight presentation и Journal policy smoke;
7. rev.17 multi-width responsive smoke 1024 / 1366 / 1680;
8. rev.18/rev.19 installed **1908×950** user-report/cleanup gate с geometry settle, real-screen `1702` paint и ghost/canonical tray проверкой;
9. Flowseal Zapret 1.10.2 / 22 strategies / native version проверки;
10. byte-identical frozen-core check.

Релиз не публикуется, если кнопки Zapret перекрываются, 1908×950 оставляет больше 210 px нижнего пустого пространства, `1702` не рисуется в primary evidence, появляется лишняя tray-иконка, Install Service не поднимает реальный bundled Zapret или ломается любой сохранённый rev.15/rev.16/rev.17/rev.18 сценарий.

## Публикация

Stable manifest публикуется как **revision 19** только после зелёных installed-проверок. Production publisher создаёт отдельный GitHub Release `v0.4.17-rev19`, публикует Pages и затем повторно проверяет live manifest, SHA-256 и размер скачанного installer. Исторический `v0.4.17-rev18` не должен использоваться для rev.19 binary.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх предыдущей 0.4.17. После установки запускайте обычный `DPopCleaner.exe`.
