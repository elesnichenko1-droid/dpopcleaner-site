# DPopCleaner 0.4.17 rev.6

DPopCleaner 0.4.17 rev.6 исправляет реальный дефект запуска revision 5: старый ярлык или прямой запуск установленного `DPopCleaner.exe` мог обойти `SimpleUpdate`/UI-bridge. В таком запуске оставалась старая надпись `v0.2.11 BETA`, а прокручиваемый блок дополнительных настроек с автообновлением не появлялся.

## Revision 6

- Исторический путь `{app}\DPopCleaner.exe` теперь всегда ведёт в launcher/UI-bridge, поэтому старые ярлыки больше не обходят новые Настройки.
- Оригинальное ядро 0.2.14 сохранено **byte-identical** под именем `{app}\DPopCleaner.Core.exe`; его Git blob остаётся `efd0eff1f4962319282363fa85595c25e0cebe11`.
- `SimpleUpdate.exe` остаётся совместимым вторым именем launcher-а.
- Launcher запускается с `requireAdministrator`, чтобы UI-bridge работал на том же уровне прав, что и ядро.
- На странице **«Настройки»** отображается прокручиваемый блок **«Дополнительные настройки»** с колесом мыши.
- В блоке всегда доступны **«Включить автообновление»**, **«Проверить обновления»** и раздел **«Лицензия»**.
- Устаревшая надпись `v0.2.11 BETA` скрывается UI-bridge.
- Второй экземпляр launcher-а больше не запускает «голое» ядро напрямую и не может обойти bridge.
- Installed-package smoke после настоящей Inno Setup установки запускает именно `{app}\DPopCleaner.exe` и проверяет наличие scroll-area, автообновления, ручной проверки, лицензии, прокрутки колесом и отсутствие видимой `v0.2.11 BETA`.

## Сохранено из revision 5

- В установщик входит полный pinned **Flowseal Zapret 1.10.2**.
- Старое ядро получает ожидаемый каталог `Zapret\` со стратегиями и runtime.
- Управление и стратегии находятся в `Zapret\service.bat`, `Zapret\general.bat` и остальных `Zapret\general*.bat`.
- Runtime находится в `Zapret\bin\winws.exe`; рядом устанавливаются `WinDivert.dll` и `WinDivert64.sys`.
- Windows CI открывает настоящий старый Zapret Center и проверяет обнаружение стратегий.

## Сохранено из revision 2/3/4

- `ZapretScreenFix.exe` и команда `zapret-screen-fix` сохраняют исправление **демонстрации экрана** при использовании Zapret.
- Анализатор диска и Центр восстановления остаются отдельными companion-модулями.
- `SimpleUpdate` проверяет stable manifest по HTTPS, сравнивает `version_code + revision`, а загруженный установщик принимает только после совпадения размера и SHA-256.

## Проверка релиза

Windows CI проверяет immutable core размером 389 632 байта и Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`, SimpleUpdate, новый launcher-layout, installed Settings UI, companion-модули, pinned Flowseal Zapret 1.10.2, настоящий Zapret Center, Inno Setup и итоговый установщик. Stable manifest публикуется только после вычисления SHA-256 и размера фактически собранного `DPopCleaner_Setup_0.4.17.exe`.

## Установка

Запустите `DPopCleaner_Setup_0.4.17.exe`. После установки запускайте обычный `DPopCleaner.exe`: это совместимый вход в UI-bridge, который запускает сохранённое ядро `DPopCleaner.Core.exe`. Полный Zapret находится в `Zapret\`: `Zapret\service.bat`, стратегии `Zapret\general*.bat`, `Zapret\bin\winws.exe`, WinDivert, `.service`, `lists` и `utils`.

Перед системными изменениями рекомендуется иметь актуальную точку восстановления Windows или резервную копию важных данных.
