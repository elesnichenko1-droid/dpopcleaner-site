# Changelog

## 0.3.4 BETA R2

- Переведена release identity на `0.3.4 BETA R2`, `version_code 3042`, `revision 2`, Windows resource `0.3.4.2`.
- Убран legacy-класс наложений контента: страницы используют общий DPI-aware safe content boundary.
- Автозагрузка получила системные иконки, классификацию системности/vendor/user, рекомендации и безопасное управление пользовательскими HKCU-записями.
- DPopGuard переведён на связку собственных эвристик, Windows AMSI и Microsoft Defender при наличии; добавлены Quick/Custom сценарии и карантинный workflow.
- Диск переработан в интерактивный файловый браузер с навигацией, иконками, крупными файлами и предупреждениями для системных путей.
- Приложения получили иконки, путь установки, штатный uninstall, поиск хвостов и WinGet-проверку обновлений при наличии.
- Дубликаты показываются как `Эталон группы (оставить) -> Дубликат 1/2/...`; эталон, системные и исключённые пути защищены, удаление идёт через Корзину.
- Windows page переработана вокруг Update cache, Component Cleanup и DISM с журналом результата и предупреждением для `/ResetBase`.
- Расширены Настройки: реальные исключения очистки, startup hooks, HKCU Run, RUNASADMIN и параметры памяти.
- Startup hooks выполняют только неразрушающие действия: проверка обновлений, Quick DPopGuard по настройке и оценка Windows Update cache без автоочистки.
- Zapret Center расширен до 8 действий.
- Добавлено `Исправление трансляций`: безопасная остановка только bundled standalone `winws`, `ipconfig /flushdns` и повторный запуск выбранной стратегии; Defender/Firewall не отключаются.
- Добавлено проверяемое обновление Zapret: официальный HTTPS GitHub release asset, size + SHA-256 digest, staging validation, backup/rollback и отказ от замены файлов активной Windows-службы.
- Production pipeline переведён на `v0.3.4-beta-r2` и `DPopCleaner_Setup_0.3.4_BETA_R2.exe` с silent install-test и live SHA-256 verification.

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
