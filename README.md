# DPopCleaner 0.3.3 BETA R1

DPopCleaner — Windows x64 utility для очистки, диагностики и повседневного обслуживания системы.

## Текущая архитектура

Версия 0.3.3 строится по схеме:

**faithful UX/поведение 0.2.14 → recovered source → функциональное ядро 0.3.2 → DPopCleaner 0.3.3.**

Это позволяет сохранить более удачную структуру интерфейса 0.2.14 и при этом не терять новые модули.

## Что входит

- Очистка и системная диагностика.
- ОЗУ и сведения о системе.
- DPopGuard QuickScan + AMSI.
- Диски, приложения, Windows, дубликаты и инструменты.
- Zapret Center с официальным Zapret 1.10.1.
- Нативная проверка обновлений через `update/beta.json`.

## CI и публикация

`.github/workflows/DPopCleaner_0.3.3_REVERSE_MIGRATION.yml` проверяет мигратор, собирает Release x64 через Visual Studio 2022, запускает CTest и UI smoke-test.

`.github/workflows/publish-dpopcleaner-0.3.3.yml` формирует Inno Setup installer, GitHub Release, согласованный `update/beta.json` и GitHub Pages.

Старые установщики не используются как fallback: если manifest 0.3.3 не прошёл проверку, кнопка скачивания отключается.
