# DPopCleaner 0.4.17 rev.5

DPopCleaner 0.4.17 rev.5 исправляет регрессию revision 4, из-за которой свежая установка содержала интерфейс Zapret, но не содержала сам runtime Zapret. Оригинальный `DPopCleaner.exe` 0.2.14 по-прежнему не изменяется; `SimpleUpdate.exe`, прокручиваемые Настройки revision 4 и `ZapretScreenFix.exe` revision 2 сохраняются.

## Revision 5

- В установщик включён полный pinned **Flowseal Zapret 1.10.2**.
- Рядом с `DPopCleaner.exe` устанавливается каталог `Zapret\`, который ожидает старый интерфейс DPopCleaner.
- Стратегии и управление службой находятся в `Zapret\service.bat`, `Zapret\general.bat` и остальных `Zapret\general*.bat`.
- Runtime находится в `Zapret\bin\winws.exe`; рядом с ним устанавливаются `WinDivert.dll` и `WinDivert64.sys`.
- Внутри `Zapret\` также устанавливаются `.service`, `lists` и `utils` из проверенного Flowseal runtime.
- Архив Flowseal принимается только после проверки pinned URL, размера и SHA-256.
- Windows CI открывает настоящий старый **Zapret Center** и проверяет, что стратегии реально обнаружены; релиз без `Zapret\bin\winws.exe` или `Zapret\general*.bat` больше не проходит.

## Сохранено из revision 4

- На странице **«Настройки»** остаётся прокручиваемый блок **«Дополнительные настройки»**.
- «Включить автообновление», ручная проверка и лицензия не перекрываются.
- Устаревшая надпись `v0.2.11 BETA` скрывается UI-bridge без патча ядра.
- `SimpleUpdate.exe` проверяет stable manifest по HTTPS, сравнивает `version_code + revision`, а скачанный установщик принимает только после совпадения размера и SHA-256.

## Сохранено из revision 2/3

- **ZapretScreenFix.exe** и команда `zapret-screen-fix` сохраняют отдельное исправление демонстрации экрана и продолжают входить в пакет.
- Анализатор диска и Центр восстановления остаются отдельными companion-модулями.
- Основные ярлыки и post-install запуск ведут через `SimpleUpdate.exe`.

## Проверка релиза

Windows CI проверяет immutable core размером 389 632 байта и Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`, SimpleUpdate, companion-модули, pinned Flowseal Zapret 1.10.2, настоящий Zapret Center UI smoke, stage, Inno Setup и уже установленный пакет. Stable manifest публикуется только после вычисления SHA-256 и размера фактически собранного установщика.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe`. После установки рядом с неизменным `DPopCleaner.exe` находятся `SimpleUpdate.exe` и каталог `Zapret\` с полным Flowseal runtime: `Zapret\service.bat`, стратегии `Zapret\general*.bat`, `Zapret\bin\winws.exe`, WinDivert, `.service`, `lists` и `utils`.

Перед системными изменениями рекомендуется иметь актуальную точку восстановления Windows или резервную копию важных данных.
