# DPopCleaner 0.3.4 BETA R2

DPopCleaner — Windows x64 utility для очистки, диагностики, защиты и повседневного обслуживания системы.

## Архитектура 0.3.4

Версия 0.3.4 строится по схеме:

**UX и логика 0.2.14 → исправленный donor 0.3.3 → отдельный v034 overlay → DPopCleaner 0.3.4 R2.**

R2 — functional-parity и safety pass: интерфейс остаётся знакомым по 0.2.x, но проблемные разделы получили реальные Windows-действия, проверки и защиту от случайных изменений.

## Рабочие разделы

- Обзор.
- Очистка с реальными исключениями файлов/папок.
- ОЗУ и параметры обслуживания памяти.
- DPopGuard: собственные эвристики + Windows AMSI + Microsoft Defender при наличии.
- Автозагрузка: иконки, источник, системность, рекомендации и безопасное включение/отключение пользовательских записей.
- Диск: встроенная навигация по файловой системе, иконки, крупные файлы и предупреждения для системных путей.
- Приложения: версия/издатель/путь, штатное удаление, хвосты и WinGet-проверка обновлений при наличии.
- Windows: Update cache, Component Cleanup и DISM с журналом результата.
- Дубликаты: «Эталон группы» и конкретные копии, защита системных/исключённых путей и удаление через Корзину.
- Инструменты.
- Zapret Center: bundled-стратегии, status/service/winws, verified update и «Исправление трансляций» для Discord/RTC.
- Обновления DPopCleaner через HTTPS manifest + size + SHA-256 + DPopUpdater.
- Настройки с startup hooks, автозапуском, RUNASADMIN и исключениями очистки.

## Безопасность R2

- layout рассчитывается от клиентской области и DPI; legacy `top=54` не используется как начало контента;
- системные/HKLM элементы автозагрузки защищены от автоматического изменения;
- дубликаты не выдаются за исторический «оригинал»: используется детерминированный «Эталон группы (оставить)»;
- startup hooks не выполняют автоматическую очистку файлов или Windows Update cache;
- RTC repair работает только с bundled standalone `winws`, очищает DNS и повторно запускает выбранную стратегию, не отключая Defender/Firewall;
- Zapret updater принимает только официальный HTTPS ZIP release asset при наличии size + SHA-256 digest, проверяет staging-bundle и делает rollback при ошибке;
- DPopCleaner updater проверяет manifest, размер и SHA-256 перед передачей пакета DPopUpdater.

## Release identity

- Display version: `0.3.4 BETA R2`
- Version code: `3042`
- Revision: `2`
- Windows resource version: `0.3.4.2`
- Installer: `DPopCleaner_Setup_0.3.4_BETA_R2.exe`

## CI и публикация

`.github/workflows/DPopCleaner_0.3.4_CANDIDATE.yml` проверяет migration/layout/shell/settings/startup/DPopGuard/storage/applications/duplicates/Zapret/R2 identity contracts, собирает x64 через Visual Studio 2022, запускает CTest и UI smoke.

`.github/workflows/publish-dpopcleaner-0.3.4.yml` повторяет полный contract/build gate, проверяет bundled Zapret, формирует Inno Setup installer, тихо устанавливает его в тестовую папку, повторно проверяет установленное приложение, а после merge публикует `v0.3.4-beta-r2`, GitHub Pages и выполняет live SHA-256 verification скачанного установщика.

Старые установщики не используются как fallback: если R2 manifest не прошёл проверку, кнопка скачивания остаётся отключённой.
