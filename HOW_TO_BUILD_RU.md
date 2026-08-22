# Как собрать в Visual Studio 2022

1. Установите Visual Studio 2022 Community.
2. В Visual Studio Installer отметьте **Desktop development with C++** и Windows 10/11 SDK.
3. Откройте Visual Studio → **Open a local folder** → выберите папку `DPopCleaner-Reconstructed`.
4. Visual Studio обнаружит `CMakePresets.json`.
5. Выберите preset `vs2022-x64` и конфигурацию `Release`.
6. `Build` → `Build All`.
7. Результат появится в `out/build/vs2022-x64/bin/Release/` или `.../bin/` в зависимости от версии VS/CMake.

Должны получиться:

- `DPopCleaner.exe`
- `DPopUpdater.exe`

До появления code-signing сертификата оставляйте в `beta.json`:

```json
"signed": false
```

Тогда программа сможет проверить и скачать обновление, но не будет автоматически запускать неподписанный установщик.
