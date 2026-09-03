# DPopCleaner 0.4.17 rev.17

DPopCleaner 0.4.17 rev.17 исправляет адаптацию вкладки **Zapret** к изменению размера окна и широким экранам. Frozen core 0.2.14 не переписывается: `{app}\DPopCleaner.Core.exe` остаётся byte-identical с Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`.

## Revision 17

- Вся рабочая область Zapret переведена на единый **responsive layout**, а не только четыре дополнительные bridge-кнопки.
- Строки стратегии, управления сервисом, обновления, дополнительных действий и нижних инструментов пересчитываются по фактической клиентской ширине окна.
- Кнопки используют свободное место справа при расширении окна, сохраняют единые высоты и интервалы и не должны перекрывать `ComboBox`, заголовки или друг друга.
- Layout учитывает реальные native размеры контролов и DPI. Вложенные native button-группы перемещаются как группы, а их дочерние кнопки раскладываются в локальных координатах родителя.
- Финальный responsive pass выполняется после прежних Zapret enhancement/presentation слоёв, поэтому frozen core 0.2.14 и bridge-proxy больше не должны перетирать корректную геометрию после resize.
- Добавлен installed multi-width smoke на **1024×768**, **1366×800** и пользовательский широкий сценарий **1680×840**. Он проверяет все 19 управляемых Zapret-кнопок: границы окна, текст, пересечения, одинаковую высоту строк и использование доступной ширины.

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
- Основной интерфейс остаётся интерфейсом DPopCleaner 0.2.14; переход на C++/0.4.18 в rev.17 не выполняется.

## Проверка rev.17

Production pipeline собирает настоящий Inno Setup installer и блокирует публикацию, пока не пройдут:

1. общие unit/contract/build проверки;
2. установленный package smoke;
3. rev.15 language-restart + RAM tray smoke;
4. rev.16 single-tray regression smoke;
5. rev.16 Zapret functional lifecycle regression smoke;
6. rev.16 Light/Midnight presentation и Journal policy smoke;
7. **rev.17 multi-width responsive smoke 1024 / 1366 / 1680**;
8. Flowseal Zapret 1.10.2 / 22 strategies / native version проверки;
9. byte-identical frozen-core check.

Релиз не публикуется, если кнопки Zapret перекрываются, текст не помещается, строки имеют различающуюся высоту, широкое окно оставляет старую фиксированную область вместо использования доступной ширины или если ломается любой из сохранённых rev.15/rev.16 сценариев.

## Публикация

Stable manifest публикуется как **revision 17** только после зелёных installed-проверок. Production publisher создаёт GitHub Release `v0.4.17-rev17`, публикует Pages и затем повторно проверяет live manifest, SHA-256 и размер скачанного installer.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe` поверх предыдущей 0.4.17. После установки запускайте обычный `DPopCleaner.exe`.
