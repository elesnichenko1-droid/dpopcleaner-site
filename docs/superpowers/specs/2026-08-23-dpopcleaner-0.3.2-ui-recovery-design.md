# DPopCleaner 0.3.2 BETA R1 — UI Recovery Edition

## Цель
0.3.2 = внешний вид и UX старой серии 0.2.x + рабочее ядро 0.3.x + модульная архитектура + проверяемый релиз.

Эталон — присланные старые скриншоты интерфейса. На них внизу виден `v0.2.11 BETA`; независимо от номера это визуальный и функциональный эталон старой серии, который нужно восстановить.

## Жёсткие правила
- R4 остаётся стабильной rollback-точкой и не перезаписывается.
- Не использовать левую вертикальную навигацию.
- Не заменять специализированные экраны универсальной текстовой панелью.
- Не публиковать 0.3.2 на сайт до ручного просмотра candidate-сборки.
- Не заявлять рабочими функции без backend.
- Не обходить Defender/SmartScreen.
- Никаких destructive действий без явного выбора/подтверждения.
- Zapret-служба/стратегии не запускаются автоматически без явного действия пользователя.

## Выбранная архитектура
Модульный native Win32 UI поверх текущего рабочего C++-ядра.

Отклонены:
1. дальнейшее разрастание одного `MainWindow.cpp`;
2. переписывание на WinUI/WPF для 0.3.2.

Предлагаемая структура:
```
v032/
  Version.h
  version.rc.in
  DPopCleaner_0.3.2_R1.iss
  ui/
    Shell.h/.cpp
    Theme.h/.cpp
    Controls.h/.cpp
    StatusBar.h/.cpp
    pages/
      OverviewPage.h/.cpp
      CleaningPage.h/.cpp
      MemoryPage.h/.cpp
      GuardPage.h/.cpp
      DiskPage.h/.cpp
      ApplicationsPage.h/.cpp
      WindowsUpdatePage.h/.cpp
      DuplicatesPage.h/.cpp
      ToolsPage.h/.cpp
      ZapretPage.h/.cpp
      SettingsPage.h/.cpp
  modules/
    MemoryManager.h/.cpp
    DiskAnalyzer.h/.cpp
    DuplicateFinder.h/.cpp
    WindowsUpdateManager.h/.cpp
    SettingsStore.h/.cpp
```

## Общая оболочка
Шапка:
- DPopCleaner
- `Очистка • память • защита • диски • Windows`
- BETA
- справа кнопка-шестерёнка

Горизонтальная навигация:
1. Обзор
2. Очистка
3. ОЗУ
4. DPopGuard
5. Диск
6. Приложения
7. Windows
8. Дубликаты
9. Инструменты
10. Zapret

Настройки открываются шестерёнкой и не являются отдельной вкладкой.

Нижняя зона:
- статус;
- реальный журнал текущего сеанса;
- Поддержка слева;
- `v0.3.2 BETA` справа.

Размер:
- целевой ~1200×850;
- минимум 1100×700;
- списки/логи/графики растягиваются;
- никаких огромных пустых зон.

## Тема Midnight
- Background `#0B1017`
- Title `#1B1F25`
- Control `#141D28`
- Hover `#1B2735`
- Border `#2A3949`
- Text `#F6F7F9`
- Muted `#B6C0CC`
- Accent `#39D0A0`
- Accent hover `#47DFB0`
- Warning `#E4B65D`
- Error `#E46F6F`

Убрать декоративный закат R4. Главная ценность эталона — плотность и функциональность.

## Обзор
6 карточек:
- Диск C:
- Оперативная память
- Установленные приложения
- DPopGuard
- Zapret
- Заполненность корзины

Быстрые действия:
- Обновить
- Быстрая очистка
- Быстрый DPopGuard
- Открыть диск
- Открыть приложения

Все показатели реальные.

## Очистка
Две колонки:
- Временные файлы пользователя
- Windows TEMP
- Кэш миниатюр
- Кэш значков Windows
- DirectX Shader Cache
- Дампы сбоев
- Отчёты ошибок Windows
- Chrome
- Edge
- Firefox
- Brave
- Vivaldi
- Opera / Opera GX
- Discord
- Steam web-cache
- Epic Games Launcher
- NVIDIA Shader Cache
- AMD Shader Cache
- Яндекс Браузер
- Корзина

Команды:
- Безопасный набор
- Все
- Снять всё
- DNS
- АНАЛИЗ
- ОЧИСТИТЬ

Analyze не удаляет. Clean затрагивает только выбранное. Профили/документы/пароли/закладки браузеров не удаляются.

## ОЗУ / DPopMemory
- Used / Total / %
- Commit
- File Cache
- realtime-график ~1 сек
- статус прав

Области:
- working sets
- system file cache
- low-priority standby
- full standby (advanced)
- modified pages (advanced)
- modified file cache (advanced)
- registry cache только при безопасной реализации
- page combining

Пресеты:
- Безопасно
- Все области
- ОЧИСТИТЬ ПАМЯТЬ
- Права администратора

Auto:
- по порогу RAM
- по интервалу
- aggressive off by default
- уведомление

Недокументированные операции Windows изолируются и никогда не включены по умолчанию.

## DPopGuard 2
Кнопки:
- Быстрая
- Глубокая
- Полная C:\
- Файл...
- Папка...
- Стоп
- Лечить угрозы
- Карантин
- Очистить карантин

Большая область результатов.

Существующие QuickScan и AMSI сохраняются.
Deep/folder scan асинхронные и отменяемые.
Никакого автоматического удаления.

## Диск
- поле пути
- Назад
- C:\
- Выбрать папку
- Сканирование
- Крупные файлы
- Проводник

Таблица:
- Имя
- Размер
- %
- Полный путь / директория

Scan асинхронный, cancel, access-denied логируется.

## Приложения
Режимы:
- Установленные
- Приложения по умолчанию

Список:
- иконка
- имя
- версия
- издатель
- путь
- размер, если доступен

Действия:
- Обновить список
- Найти обновления
- Обновить выбранное
- Исправить / восстановить
- Установить...
- Удалить выбранное
- Открыть папку

Переиспользовать текущие EnumerateInstalledApps, RunUninstaller, FindLeftovers, MoveLeftoversToRecycleBin.
После удаления — повторная инвентаризация и предложение scan leftovers.
Registry cleanup не делать молча.
Поиск обновлений — через winget только при однозначном сопоставлении.

## Windows Update
- размер download cache
- Проверить размер
- Очистить кэш
- Очистить старые компоненты
- /ResetBase

Использовать штатные Windows/DISM средства.
`/ResetBase` требует отдельного предупреждения.

## Дубликаты
- Сканировать папку...
- Остановить
- Открыть расположение...
- В Корзину

Алгоритм:
1. группировка по размеру;
2. SHA-256 только в одинаковых size-группах;
3. одинаковый SHA-256 = duplicate content;
4. пользователь выбирает, что отправить в Корзину.

## Инструменты
Системные:
- SFC /scannow
- DISM CheckHealth
- DISM ScanHealth
- DISM RestoreHealth
- CHKDSK C: /scan

Быстрый доступ:
- Диспетчер задач
- Просмотр событий
- Автозагрузка
- Восстановление системы
- Безопасность Windows
- Открыть логи
- Анализ производительности

Долгие процессы не блокируют UI.

## Zapret Center
Статус:
- версия bundle
- service installed/running
- winws running
- bundle validity

Стратегии — combo из реального bundle.

Команды:
- управление service flow
- запуск выбранной стратегии
- остановка
- статус

Update:
- Проверить версию
- Скачать и установить
- Auto update off by default
- Auto start off by default
- Удалить сервисы

Дополнительно:
- Фильтр
- Применить
- IPSet
- Обновить IPSet
- Обновить hosts
- Диагностика
- Тесты

## Настройки
Открываются шестерёнкой.

- Язык: Русский / English
- Тема: Midnight; другие только после реализации
- фоновый контроль мусора
- quick Guard at startup
- check Windows Update cache at startup
- tray + new installs watch
- autostart DPopCleaner
- run as admin

Cleanup exclusions:
- список
- Добавить файл
- Добавить папку
- Удалить

Persist:
`%LOCALAPPDATA%\DPopCleaner\settings.json`
с atomic write и fallback defaults при повреждении.

Лицензия:
- не имитировать отсутствующий сервер;
- пока `Бесплатная BETA`.

## Async
В background:
- disk scan
- duplicate scan
- deep/folder Guard
- app update discovery
- heavy cleaning analysis
- repair commands
- Zapret update

Close корректно отменяет workers и не запускает updater/installer повторно.

## Логи
`%LOCALAPPDATA%\DPopCleaner\Logs\DPopCleaner.log`

timestamp/category/severity/message.
В UI — текущий сеанс.
Секреты и содержимое пользовательских файлов не логируются.

## Версия 0.3.2
- Display: `0.3.2 BETA R1`
- Version: `0.3.2`
- Version code: `3021`
- Revision: `1`
- Tag: `v0.3.2-beta-r1`
- Installer: `DPopCleaner_Setup_0.3.2_BETA_R1.exe`

## Release process
### Candidate workflow
`.github/workflows/build-dpopcleaner-0.3.2-r1-candidate.yml`

Делает build/tests/installer/Defender/screenshots/evidence.
НЕ публикует Release, НЕ меняет beta.json, НЕ трогает сайт.

### Publish workflow
`.github/workflows/publish-dpopcleaner-0.3.2-r1.yml`

Запускается вручную только после визуального одобрения candidate:
- immutable prerelease
- fresh public SHA/size
- beta.json/version.json
- screenshot
- changelog
- Pages

Сайт остаётся на R4 до ручного одобрения 0.3.2.

## Definition of Done
0.3.2 готова только если:
1. Shell узнаваем как старый DPopCleaner.
2. 10 горизонтальных вкладок + gear Settings.
3. У каждого экрана собственные реальные controls.
4. Overview повторяет старый dashboard.
5. Cleaning имеет selectable analysis/clean.
6. Memory имеет metrics/graph.
7. Guard/Disk/Apps/Windows/Duplicates/Tools/Zapret/Settings полноценны.
8. UI не зависает на долгих задачах.
9. destructive действия подтверждаются.
10. unit/integration tests зелёные.
11. Defender зелёный.
12. close/relaunch test зелёный.
13. candidate EXE/screenshots просмотрены пользователем.
14. только потом release+site.
15. R4 остаётся rollback-точкой.

## Порядок реализации
1. Shell Recovery: Theme, Controls, Shell, StatusBar, tabs, Settings gear, Overview, resize, logging.
2. Existing Core Screens: Cleaning, Applications, DPopGuard, Zapret, Tools.
3. Missing Screens: Memory, Disk, Windows Update, Duplicates, Settings, localization/exclusions.
4. Integration: async workers, startup actions, updater/version 0.3.2, UI smoke.
5. Candidate: build, Defender, screenshots all pages, manual review.
6. Publish: release, manifest, changelog, site, Pages.
