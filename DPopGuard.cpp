#include "modules/DPopGuard.h"
#include "modules/StartupManager.h"
#include <algorithm>
#include <cwctype>

namespace dpop::guard {
ScanResult QuickScan() {
    ScanResult result{};
    // В старом EXE видны строки "signatures + AMSI + persistence + miner heuristics",
    // но сам алгоритм из машинного кода надёжно восстановить нельзя. Здесь оставлена
    // безопасная стартовая реализация: только обзор автозапуска без удаления файлов.
    for (const auto& e : dpop::startup::EnumerateCurrentUserRun()) {
        std::wstring lower = e.command;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        if (lower.find(L"\\temp\\") != std::wstring::npos || lower.find(L"appdata\\local\\temp") != std::wstring::npos) {
            result.findings.push_back({L"Автозапуск из временной папки", e.name + L": " + e.command});
        }
    }
    result.note = L"Реконструированная безопасная база. AMSI/сигнатурный движок нужно переносить отдельно.";
    return result;
}
}
