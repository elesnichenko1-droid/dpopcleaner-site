#pragma once

namespace dpop::update_config {
inline constexpr wchar_t kManifestUrl[] =
    L"https://elesnichenko1-droid.github.io/dpopcleaner-site/update/beta.json";
inline constexpr wchar_t kUserAgent[] = L"DPopCleaner-Updater/0.3.1";
inline constexpr unsigned long kConnectTimeoutMs = 8000;
inline constexpr unsigned long kReceiveTimeoutMs = 20000;
}
