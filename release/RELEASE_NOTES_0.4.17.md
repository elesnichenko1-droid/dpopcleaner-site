# DPopCleaner 0.4.17 rev.18

DPopCleaner 0.4.17 rev.18 завершает исправление пользовательского wide-screen сценария Zapret и стабилизирует реальный Flowseal lifecycle. Frozen core 0.2.14 не переписывается: `{app}\DPopCleaner.Core.exe` остаётся byte-identical с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 18

- Добавлен отдельный installed user-report gate для окна **1908×950**: он проверяет итоговую геометрию Zapret после resize/maximize, оформление Light/Midnight и единственную canonical RAM tray-иконку без ghost-дубликатов.
- Запрет-кнопки продолжают использовать responsive layout rev.17, но теперь layout учитывает не только ширину, а фактическую клиентскую высоту и не оставляет промежуточные native/bridge позиции на широком окне.
- Owner-draw кнопок Zapret закреплён независимо от Windows theme state: Light и Midnight сохраняют единый размер, рамку и визуальный стиль.
- Tray preference отделён от состояния frozen core: canonical bridge tray можно выключить и снова включить без появления legacy/ghost-иконок.
- Выбор стратегии Zapret больше не определяется вертикальной позицией `ComboBox`. Bridge находит настоящий список по его содержимому `general*.bat`, поэтому responsive layout не может заставить Install/Start прочитать соседний фильтр.
- Для реальной установки Flowseal service увеличен безопасный startup budget с 15 до **30 секунд**. На installed runner фактический запуск занимал около 10 секунд, а прежний короткий budget давал редкий ложный timeout на более медленной Windows image.
- Functional smoke теперь диагностирует точный proxy `1701`, выбранную стратегию, временный `service-dpop-install-*.bat` и `cmd.exe`, сохраняя cleanup при любой ошибке.

## Сохранено из rev.17

- Вся рабочая область Zapret использует единый **responsive layout**, а не только четыре дополнительные bridge-кнопки.
- Строки стратегии, управления сервисом, обновления, дополнительных действий и нижних инструментов пересчитываются по фактическому размеру окна.
- Кнопки используют свободное место справа при расширении окна, сохраняют единые высоты и интервалы и не перекрывают `ComboBox`, заголовки или друг друга.
- Layout учитывает native размеры контролов и DPI; вложенные button-группы перемещаются как группы.
- Installed multi-width smoke сохраняется для **1024×768**, **1366×800** и широкого сценария **1680×840**.

## Сохранено из rev.16

- Одна рабочая RAM tray-иконка с цифровым процентом ОЗУ; восстановление canonical tray identity после restart frozen core и Explorer без ghost-иконки.
- Перепривязка launcher к новому `DPopCleaner.Core.exe` после смены языка, Settings bridge и RAM tray после self-restart.
- Реальный Zapret lifecycle: **Install service → Start winws → Status → Stop → смена стратегии → Start → Remove**.
- Единое оформление Zapret-кнопок в **Light** и **Midnight**.
- **Журнал** скрывается только на вкладке Zapret и остаётся без изменений на остальных вкладках.
- «Починка трансляции», «Починка подключения», «Игровой фильтр 1.10.2», «Менеджер 1.10.2», frozen-updater compatibility и `DPopUpdate.exe`.
- Исправление демонстрации экрана / screen share для ZapretScreenFix.
- Автообновление приложения, Disk Analyzer, Restore Center, UAC/code 740 fix и RAM threshold 5–95%.

## Не меняется

- Frozen core 0.2.14: `efd0eff1f4962319282363fa85595c25e0cebe11`.
- Полный **Flowseal Zapret 1.10.2** и все **22 стратегии**.
- Native версия Zapret читается через `Zapret\utils\dpop_version.txt`.
- Основной интерфейс остаётся интерфейсом DPopCleaner 0.2.14; переход на C++/0.4.18 в rev.18 не выполняется.

## Проверка rev.18

Production pipeline собирает настоящий Inno Setup installer и блокирует публикацию, пока не пройдут:

1. общие unit/contract/build проверки;
2. установленный package smoke;
3. rev.15 language-restart + RAM tray smoke;
4. rev.16 single-tray regression smoke;
5. rev.16 Zapret functional lifecycle regression smoke с реальным Flowseal service/winws;
6. rev.16 Light/Midnight presentation и Journal policy smoke;
7. rev.17 multi-width responsive smoke 1024 / 1366 / 1680;
8. **rev.18 installed 1908×950 user-report gate** с ghost/canonical tray проверкой;
9. Flowseal Zapret 1.10.2 / 22 strategies / native version проверки;
10. byte-identical frozen-core check.

Релиз не публикуется, если кнопки Zapret перекрываются, wide-screen layout возвращается к старой фиксированной геометрии, появляется лишняя tray-иконка, Install Service не поднимает реальный bundled Zapret или ломается любой сохранённый rev.15/rev.16/rev.17 сценарий.

## Публикация

Stable manifest публикуется как **revision 18** только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev18`, публикует Pages и затем повторно проверяет live manifest, SHA-256 и размер скачанного installer.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх предыдущей 0.4.17. После установки запускайте обычный `DPopCleaner.exe`.
