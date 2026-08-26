ЧИСТЫЙ УСТАНОВЩИК DPopCleaner 0.2.14 + АВТООБНОВЛЕНИЕ САЙТА

Старый Setup физически содержит WinDivert/Zapret-компоненты. Новый installer
упаковывает ТОЛЬКО оригинальный DPopCleaner.exe.

В новый Setup НЕ входят:
- WinDivert.sys / WinDivert64.sys
- WinDivert.dll
- winws.exe
- zapret BAT/службы

После успешной сборки workflow:
1. создаёт GitHub Release v0.2.14-clean;
2. загружает DPopCleaner_Setup_0.2.14_BETA_CLEAN.exe;
3. считает SHA-256;
4. записывает update/beta.json;
5. меняет ссылку Download на сайте;
6. сам публикует GitHub Pages.

Нужно один раз:
A) загрузить release/DPopCleaner_CLEAN.iss
B) загрузить .github/workflows/build-clean-installer.yml
C) создать app/ и положить туда ОРИГИНАЛЬНЫЙ standalone EXE:
   app/DPopCleaner.exe

Ожидаемый SHA-256 оригинального EXE:
7d5e0a510189db31ef7ee1aca72dc182332a8020d994c81be40a519c5960515c

Затем:
Actions -> Build CLEAN DPopCleaner installer -> Run workflow -> main

После зелёной галочки сайт сам будет скачивать CLEAN Setup.

Если Defender блокирует даже app/DPopCleaner.exe отдельно, НЕ отключать защиту:
нужно открыть Windows Security -> Protection history и прислать точное имя
детекта (например Trojan:Win32/... или PUA:Win32/...).
