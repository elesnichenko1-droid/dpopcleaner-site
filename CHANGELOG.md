# Changelog

## 0.3.4 BETA R1

- Возвращена левая вертикальная навигация в духе 0.2.14.
- Сформированы 13 рабочих разделов, включая отдельные Автозагрузку, Обновления и Настройки.
- Добавлен DPI-aware layout и регрессионные проверки 1100×700 / 1200×850 / 100–150% DPI.
- Исправлено физическое пересечение заголовка и контента Zapret Center.
- Zapret Center расширен до выбора bundled `general*.bat`, запуска выбранной/default стратегии, Service Manager, открытия bundle и безопасной остановки только bundled `winws.exe`.
- Восстановлена Автозагрузка на реальных HKCU/HKLM Run и Startup folders.
- Восстановлена отдельная страница обновлений поверх HTTPS manifest + SHA-256 + DPopUpdater.
- Сохранены исправления 0.3.3, включая `RecoveryControls small -> smallFont`.
- Candidate CI проверяет migration, layout, shell, Zapret, MSVC, CTest и UI smoke.
- Release CI собирает Inno Setup installer, тестово устанавливает его с настоящим Zapret 1.10.1 bundle и повторно проверяет UI установленной программы.
- GitHub Release, update manifest, сайт и live SHA-256 verification объединены в 0.3.4 publish pipeline.

## 0.3.3 BETA R1

- Вернули faithful 0.2.14-style UX как основу новой ветки.
- Перенесли функциональное ядро 0.3.2 в recovered source.
- Исправили конфликт `small` с Windows SDK (`smallFont`).
- Подтвердили Visual Studio 2022 x64 build и 13/13 CTest.
- Добавили UI smoke-test и защиту от белого фона.
- Синхронизировали release, updater manifest и GitHub Pages в одном 0.3.3 pipeline.
- Убрали fallback на 0.3.1 R4 с сайта.
