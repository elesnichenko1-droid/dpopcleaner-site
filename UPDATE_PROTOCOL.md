# Протокол обновления DPopCleaner

`DPopCleaner.exe` получает JSON по HTTPS из `UpdateConfig.h`.

Обязательные поля:

- `version` — версия для отображения;
- `version_code` — целое число для сравнения (`0.2.15` → `215`);
- `download_url` — HTTPS-ссылка на GitHub Release или CDN;
- `sha256` — SHA-256 именно опубликованного EXE;
- `signed` — `true` только после Authenticode-подписи.

Логика безопасности:

1. Манифест загружается по HTTPS.
2. Если `version_code <= 214`, обновление не предлагается.
3. Пакет скачивается как `.part`.
4. Считается SHA-256 и сравнивается с манифестом.
5. При несовпадении файл немедленно удаляется.
6. Если `signed=false`, DPopCleaner **не запускает** EXE автоматически. Пользователь получает проверанный по SHA-256 файл для ручной установки.
7. Если `signed=true`, дополнительно выполняется `WinVerifyTrust`.
8. Только после успешной Authenticode-проверки запускается `DPopUpdater.exe`.
9. `DPopUpdater` ждёт закрытия DPopCleaner и ещё раз проверяет подпись перед запуском установщика через UAC.

После покупки собственного домена поменяйте `kManifestUrl` в `src/update/UpdateConfig.h` на, например:

`https://dpopcleaner.app/update/beta.json`
