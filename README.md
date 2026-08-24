# DPopCleaner 0.3.4 BETA R1

DPopCleaner — Windows x64 utility для очистки, диагностики и повседневного обслуживания системы.

## Архитектура 0.3.4

Версия 0.3.4 строится по схеме:

**UX и логика 0.2.14 → исправленный donor 0.3.3 → отдельный v034 overlay → DPopCleaner 0.3.4.**

Цель 0.3.4 — вернуть удобство и плотность старой версии, сохранив проверяемую современную сборочную цепочку.

## Рабочие разделы

- Обзор.
- Очистка.
- ОЗУ.
- DPopGuard.
- Автозагрузка (HKCU/HKLM Run + Startup folders).
- Диск.
- Приложения.
- Windows.
- Дубликаты.
- Инструменты.
- Zapret Center с официальным Zapret 1.10.1 bundle.
- Обновления через HTTPS manifest + SHA-256 + DPopUpdater.
- Настройки.

## 0.3.4 UI

- возвращена левая вертикальная навигация в духе 0.2.14;
- layout рассчитывается от клиентской области и DPI;
- Zapret Center больше не использует пересекающуюся фиксированную геометрию;
- bundled `general*.bat` стратегии обнаруживаются автоматически;
- остановка standalone `winws.exe` ограничивается точным bundled-путём.

## CI и публикация

`.github/workflows/DPopCleaner_0.3.4_CANDIDATE.yml` проверяет migration/layout/shell/Zapret contracts, собирает Release x64 через Visual Studio 2022, запускает CTest и UI smoke-test.

`.github/workflows/publish-dpopcleaner-0.3.4.yml` дополнительно скачивает и проверяет Zapret 1.10.1, формирует Inno Setup installer, тихо устанавливает его в тестовую папку, повторно проверяет установленное приложение с настоящим bundle, а после merge публикует GitHub Release и GitHub Pages.

Старые установщики не используются как fallback: если manifest 0.3.4 не прошёл проверку, кнопка скачивания остаётся отключённой.
