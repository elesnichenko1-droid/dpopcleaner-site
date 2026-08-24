#!/usr/bin/env python3
"""DPopCleaner 0.3.4 entrypoint over the verified repaired 0.3.3 donor."""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from typing import Sequence

_CORE_PATH = Path(__file__).with_name('dpop034_core.py')
_SPEC = importlib.util.spec_from_file_location('dpop034_core', _CORE_PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError(f'cannot load 0.3.4 migration core: {_CORE_PATH}')
_core = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _core
_SPEC.loader.exec_module(_core)

_BUILD_PATH = Path(__file__).with_name('dpop034_build.py')
_BUILD_SPEC = importlib.util.spec_from_file_location('dpop034_build', _BUILD_PATH)
if _BUILD_SPEC is None or _BUILD_SPEC.loader is None:
    raise RuntimeError(f'cannot load 0.3.4 build helpers: {_BUILD_PATH}')
_build = importlib.util.module_from_spec(_BUILD_SPEC)
sys.modules[_BUILD_SPEC.name] = _build
_BUILD_SPEC.loader.exec_module(_build)

_original_transform_v034_overlay = _core.transform_v034_overlay
_original_migrate_034 = _core.migrate_034


def _run_repaired_donor(repository: Path, donor_output: Path, donor_workspace: Path) -> dict:
    import dpop033_migrate
    return dpop033_migrate.migrate(
        repository,
        donor_output,
        donor_workspace,
        build=False,
        keep_worktree=True,
    )


_run_donor_migration = _run_repaired_donor
_core._run_donor_migration = _run_donor_migration

transform_cmake_for_page_layout = _build.transform_cmake_for_page_layout
transform_cmake_for_zapret_center = _build.transform_cmake_for_zapret_center
transform_cmake_for_zapret_page_layout = _build.transform_cmake_for_zapret_page_layout
transform_cmake_for_shell_parity = _build.transform_cmake_for_shell_parity
_prepare_034_script_text = _build._prepare_034_script_text


def _must_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise ValueError(f'R2 settings donor drifted in {label}: {old[:80]}')
    return text.replace(old, new, 1)


def _insert_page_layout_include(text: str) -> str:
    include = '#include "ui/PageLayout.h"'
    if include in text:
        return text
    lines = text.splitlines()
    include_indexes = [i for i, line in enumerate(lines) if line.startswith('#include ')]
    if include_indexes:
        lines.insert(include_indexes[-1] + 1, include)
    else:
        lines.insert(0, include)
    suffix = '\n' if text.endswith('\n') else ''
    return '\n'.join(lines) + suffix


def transform_page_content_top(text: str) -> str:
    """Replace the recovered hard-coded content origin with the shared DPI boundary."""
    legacy = 'const int top = 54;'
    if legacy not in text:
        return text
    updated = _insert_page_layout_include(text)
    return updated.replace(
        legacy,
        'const int top = dpop::ui::ComputePageContentTop(GetDpiForWindow(Hwnd()));',
    )


def transform_fullcore_header(text: str) -> str:
    old = '''struct Settings {\n    bool confirmDestructive{true};\n    unsigned largeFileMB{500};\n    unsigned duplicateMinMB{10};\n    bool runAtStartup{false};\n};\n\nstd::filesystem::path SettingsPath();\nSettings LoadSettings();\nbool SaveSettings(const Settings& settings, std::wstring& error);\nbool SetRunAtStartup(bool enabled, std::wstring& error);\n'''
    new = '''struct Settings {\n    bool confirmDestructive{true};\n    unsigned largeFileMB{500};\n    unsigned duplicateMinMB{10};\n    bool runAtStartup{false};\n    bool alwaysRunAsAdmin{false};\n    bool checkUpdatesAtStartup{true};\n    bool quickGuardAtStartup{false};\n    bool checkUpdateCacheAtStartup{false};\n    bool minimizeToTray{true};\n    bool monitorInstallations{false};\n    bool backgroundJunkMonitor{false};\n    unsigned memoryAutoTrimPercent{80};\n    std::vector<std::wstring> cleanExclusions;\n};\n\nstd::filesystem::path SettingsPath();\nSettings LoadSettings();\nbool SaveSettings(const Settings& settings, std::wstring& error);\nbool SetRunAtStartup(bool enabled, std::wstring& error);\nbool SetAlwaysRunAsAdmin(bool enabled, std::wstring& error);\nbool IsPathExcluded(const std::filesystem::path& path, const Settings& settings);\n'''
    return _must_replace(text, old, new, 'FullCore.h')


def transform_fullcore_source(text: str) -> str:
    if 'ExtractStringArray(' in text and 'SetAlwaysRunAsAdmin(' in text:
        return text

    text = _must_replace(
        text,
        'std::uint64_t EstimateTree(const fs::path& root, std::stop_token stop) {',
        'std::uint64_t EstimateTree(const fs::path& root, std::stop_token stop, const dpop::full::Settings* settings = nullptr) {',
        'FullCore.cpp EstimateTree signature')
    text = _must_replace(text,
        '    if (root.empty() || !fs::exists(root, ec)) return 0;\n    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;',
        '    if (root.empty() || !fs::exists(root, ec)) return 0;\n    if (settings && dpop::full::IsPathExcluded(root, *settings)) return 0;\n    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;',
        'FullCore.cpp EstimateTree root')
    text = _must_replace(text,
        '        if (ec) { ec.clear(); continue; }\n        if (it->is_regular_file(ec)) {',
        '        if (ec) { ec.clear(); continue; }\n        if (settings && dpop::full::IsPathExcluded(it->path(), *settings)) {\n            if (it->is_directory(ec)) it.disable_recursion_pending();\n            ec.clear();\n            continue;\n        }\n        if (it->is_regular_file(ec)) {',
        'FullCore.cpp EstimateTree entries')

    text = _must_replace(text,
        'void CleanTree(const fs::path& root, dpop::full::CleanSummary& out, std::stop_token stop) {',
        'void CleanTree(const fs::path& root, dpop::full::CleanSummary& out, std::stop_token stop, const dpop::full::Settings* settings = nullptr) {',
        'FullCore.cpp CleanTree signature')
    text = _must_replace(text,
        '    if (root.empty() || !fs::exists(root, ec)) return;\n    std::vector<fs::path> dirs;',
        '    if (root.empty() || !fs::exists(root, ec)) return;\n    if (settings && dpop::full::IsPathExcluded(root, *settings)) return;\n    std::vector<fs::path> dirs;',
        'FullCore.cpp CleanTree root')
    text = _must_replace(text,
        '        if (ec) { ec.clear(); continue; }\n        if (it->is_directory(ec)) { dirs.push_back(it->path()); ec.clear(); continue; }',
        '        if (ec) { ec.clear(); continue; }\n        if (settings && dpop::full::IsPathExcluded(it->path(), *settings)) {\n            if (it->is_directory(ec)) it.disable_recursion_pending();\n            ec.clear();\n            continue;\n        }\n        if (it->is_directory(ec)) { dirs.push_back(it->path()); ec.clear(); continue; }',
        'FullCore.cpp CleanTree entries')

    text = _must_replace(text,
        'std::uint64_t EstimateDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes) {',
        'std::uint64_t EstimateDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes, const dpop::full::Settings* settings = nullptr) {',
        'FullCore.cpp EstimateDirectFiles signature')
    text = _must_replace(text,
        'void CleanDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes, dpop::full::CleanSummary& out) {',
        'void CleanDirectFiles(const fs::path& root, const std::vector<std::wstring>& prefixes, dpop::full::CleanSummary& out, const dpop::full::Settings* settings = nullptr) {',
        'FullCore.cpp CleanDirectFiles signature')

    extract_anchor = '''unsigned ExtractUInt(const std::wstring& text, std::wstring_view key, unsigned fallback) {\n    const auto pos = text.find(std::wstring(L"\\\"") + std::wstring(key) + L"\\\"");\n    if (pos == std::wstring::npos) return fallback;\n    const auto colon = text.find(L':', pos);\n    if (colon == std::wstring::npos) return fallback;\n    const auto begin = text.find_first_of(L"0123456789", colon + 1);\n    if (begin == std::wstring::npos) return fallback;\n    const auto end = text.find_first_not_of(L"0123456789", begin);\n    try { return static_cast<unsigned>(std::stoul(text.substr(begin, end - begin))); }\n    catch (...) { return fallback; }\n}\n'''
    helpers = extract_anchor + '''\nstd::vector<std::wstring> ExtractStringArray(const std::wstring& text, std::wstring_view key) {\n    std::vector<std::wstring> out;\n    const auto pos = text.find(std::wstring(L"\\\"") + std::wstring(key) + L"\\\"");\n    if (pos == std::wstring::npos) return out;\n    const auto begin = text.find(L'[', pos);\n    const auto end = begin == std::wstring::npos ? std::wstring::npos : text.find(L']', begin + 1);\n    if (begin == std::wstring::npos || end == std::wstring::npos) return out;\n    std::size_t i = begin + 1;\n    while (i < end) {\n        const auto quote = text.find(L'\\\"', i);\n        if (quote == std::wstring::npos || quote >= end) break;\n        std::wstring value; bool escaped = false; std::size_t j = quote + 1;\n        for (; j < end; ++j) {\n            const wchar_t c = text[j];\n            if (escaped) { value.push_back(c == L'n' ? L'\\n' : c); escaped = false; continue; }\n            if (c == L'\\\\') { escaped = true; continue; }\n            if (c == L'\\\"') break;\n            value.push_back(c);\n        }\n        if (!value.empty()) out.push_back(value);\n        i = j + 1;\n    }\n    return out;\n}\n\nstd::wstring JsonEscape(std::wstring_view value) {\n    std::wstring out;\n    for (const wchar_t c : value) {\n        if (c == L'\\\\' || c == L'\\\"') out.push_back(L'\\\\');\n        if (c == L'\\n') { out.append(L"\\\\n"); continue; }\n        out.push_back(c);\n    }\n    return out;\n}\n\nstd::wstring NormalizedPath(fs::path path) {\n    std::error_code ec;\n    const auto canonical = fs::weakly_canonical(path, ec);\n    std::wstring value = (ec ? path : canonical).wstring();\n    std::replace(value.begin(), value.end(), L'/', L'\\\\');\n    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });\n    while (value.size() > 3 && value.back() == L'\\\\') value.pop_back();\n    return value;\n}\n'''
    text = _must_replace(text, extract_anchor, helpers, 'FullCore.cpp JSON helpers')

    text = _must_replace(text,
        'std::vector<CleanItem> AnalyzeCleaning(std::stop_token stop) {\n    const std::array<CleanKind, 14> kinds = {',
        'std::vector<CleanItem> AnalyzeCleaning(std::stop_token stop) {\n    const Settings settings = LoadSettings();\n    const std::array<CleanKind, 14> kinds = {',
        'FullCore.cpp AnalyzeCleaning settings')
    text = text.replace('EstimateDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_"})', 'EstimateDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_"}, &settings)')
    text = text.replace('EstimateDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"iconcache_"})', 'EstimateDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"iconcache_"}, &settings)')
    text = text.replace('for (const auto& root : RootsFor(kind)) bytes += EstimateTree(root, stop);', 'for (const auto& root : RootsFor(kind)) bytes += EstimateTree(root, stop, &settings);')

    text = _must_replace(text,
        'CleanSummary CleanSelected(const std::vector<CleanKind>& kinds, std::stop_token stop) {\n    CleanSummary out{};',
        'CleanSummary CleanSelected(const std::vector<CleanKind>& kinds, std::stop_token stop) {\n    const Settings settings = LoadSettings();\n    CleanSummary out{};',
        'FullCore.cpp CleanSelected settings')
    text = text.replace('CleanDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_"}, out);', 'CleanDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"thumbcache_"}, out, &settings);')
    text = text.replace('CleanDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"iconcache_"}, out);', 'CleanDirectFiles(Join(Env(L"LOCALAPPDATA"), L"Microsoft/Windows/Explorer"), {L"iconcache_"}, out, &settings);')
    text = text.replace('for (const auto& root : RootsFor(kind)) CleanTree(root, out, stop);', 'for (const auto& root : RootsFor(kind)) CleanTree(root, out, stop, &settings);')

    load_old = '''    out.confirmDestructive = ExtractBool(text, L"confirm_destructive", true);\n    out.largeFileMB = std::clamp(ExtractUInt(text, L"large_file_mb", 500), 50u, 4096u);\n    out.duplicateMinMB = std::clamp(ExtractUInt(text, L"duplicate_min_mb", 10), 1u, 1024u);\n    out.runAtStartup = ExtractBool(text, L"run_at_startup", false);\n    return out;\n'''
    load_new = '''    out.confirmDestructive = ExtractBool(text, L"confirm_destructive", true);\n    out.largeFileMB = std::clamp(ExtractUInt(text, L"large_file_mb", 500), 50u, 4096u);\n    out.duplicateMinMB = std::clamp(ExtractUInt(text, L"duplicate_min_mb", 10), 1u, 1024u);\n    out.runAtStartup = ExtractBool(text, L"run_at_startup", false);\n    out.alwaysRunAsAdmin = ExtractBool(text, L"always_run_as_admin", false);\n    out.checkUpdatesAtStartup = ExtractBool(text, L"check_updates_at_startup", true);\n    out.quickGuardAtStartup = ExtractBool(text, L"quick_guard_at_startup", false);\n    out.checkUpdateCacheAtStartup = ExtractBool(text, L"check_update_cache_at_startup", false);\n    out.minimizeToTray = ExtractBool(text, L"minimize_to_tray", true);\n    out.monitorInstallations = ExtractBool(text, L"monitor_installations", false);\n    out.backgroundJunkMonitor = ExtractBool(text, L"background_junk_monitor", false);\n    out.memoryAutoTrimPercent = std::clamp(ExtractUInt(text, L"memory_auto_trim_percent", 80), 50u, 98u);\n    out.cleanExclusions = ExtractStringArray(text, L"clean_exclusions");\n    return out;\n'''
    text = _must_replace(text, load_old, load_new, 'FullCore.cpp LoadSettings')

    save_old = '''    out << L"{\\n"\n        << L"  \\\"confirm_destructive\\\": " << (settings.confirmDestructive ? L"true" : L"false") << L",\\n"\n        << L"  \\\"large_file_mb\\\": " << settings.largeFileMB << L",\\n"\n        << L"  \\\"duplicate_min_mb\\\": " << settings.duplicateMinMB << L",\\n"\n        << L"  \\\"run_at_startup\\\": " << (settings.runAtStartup ? L"true" : L"false") << L"\\n"\n        << L"}\\n";\n    return true;\n}\n'''
    save_new = '''    out << L"{\\n"\n        << L"  \\\"confirm_destructive\\\": " << (settings.confirmDestructive ? L"true" : L"false") << L",\\n"\n        << L"  \\\"large_file_mb\\\": " << settings.largeFileMB << L",\\n"\n        << L"  \\\"duplicate_min_mb\\\": " << settings.duplicateMinMB << L",\\n"\n        << L"  \\\"run_at_startup\\\": " << (settings.runAtStartup ? L"true" : L"false") << L",\\n"\n        << L"  \\\"always_run_as_admin\\\": " << (settings.alwaysRunAsAdmin ? L"true" : L"false") << L",\\n"\n        << L"  \\\"check_updates_at_startup\\\": " << (settings.checkUpdatesAtStartup ? L"true" : L"false") << L",\\n"\n        << L"  \\\"quick_guard_at_startup\\\": " << (settings.quickGuardAtStartup ? L"true" : L"false") << L",\\n"\n        << L"  \\\"check_update_cache_at_startup\\\": " << (settings.checkUpdateCacheAtStartup ? L"true" : L"false") << L",\\n"\n        << L"  \\\"minimize_to_tray\\\": " << (settings.minimizeToTray ? L"true" : L"false") << L",\\n"\n        << L"  \\\"monitor_installations\\\": " << (settings.monitorInstallations ? L"true" : L"false") << L",\\n"\n        << L"  \\\"background_junk_monitor\\\": " << (settings.backgroundJunkMonitor ? L"true" : L"false") << L",\\n"\n        << L"  \\\"memory_auto_trim_percent\\\": " << settings.memoryAutoTrimPercent << L",\\n"\n        << L"  \\\"clean_exclusions\\\": [";\n    for (std::size_t i = 0; i < settings.cleanExclusions.size(); ++i) {\n        if (i) out << L", ";\n        out << L"\\\"" << JsonEscape(settings.cleanExclusions[i]) << L"\\\"";\n    }\n    out << L"]\\n}\\n";\n    return true;\n}\n'''
    text = _must_replace(text, save_old, save_new, 'FullCore.cpp SaveSettings')

    marker = '''std::wstring FormatBytes(std::uint64_t bytes) {'''
    additions = '''bool SetAlwaysRunAsAdmin(bool enabled, std::wstring& error) {\n    std::wstring exe(32768, L'\\0');\n    const DWORD n = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));\n    if (!n || n >= exe.size()) { error = L"Не удалось определить путь DPopCleaner.exe."; return false; }\n    exe.resize(n);\n    HKEY key{};\n    constexpr wchar_t kLayers[] = L"Software\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\\\\AppCompatFlags\\\\Layers";\n    const LONG open = RegCreateKeyExW(HKEY_CURRENT_USER, kLayers, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);\n    if (open != ERROR_SUCCESS) { error = L"Не удалось открыть AppCompatFlags Layers."; return false; }\n    LONG rc = ERROR_SUCCESS;\n    if (enabled) {\n        constexpr wchar_t value[] = L"RUNASADMIN";\n        rc = RegSetValueExW(key, exe.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value), sizeof(value));\n    } else {\n        rc = RegDeleteValueW(key, exe.c_str());\n        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;\n    }\n    RegCloseKey(key);\n    if (rc == ERROR_SUCCESS) return true;\n    error = L"Не удалось изменить режим запуска от администратора. Код: " + std::to_wstring(rc);\n    return false;\n}\n\nbool IsPathExcluded(const fs::path& path, const Settings& settings) {\n    if (path.empty()) return false;\n    const std::wstring candidate = NormalizedPath(path);\n    if (candidate.empty()) return false;\n    for (const auto& raw : settings.cleanExclusions) {\n        if (raw.empty()) continue;\n        const std::wstring excluded = NormalizedPath(fs::path(raw));\n        if (excluded.empty()) continue;\n        if (candidate == excluded) return true;\n        if (candidate.size() > excluded.size() && candidate.rfind(excluded, 0) == 0 && candidate[excluded.size()] == L'\\\\') return true;\n    }\n    return false;\n}\n\n'''
    text = _must_replace(text, marker, additions + marker, 'FullCore.cpp R2 settings functions')

    # Direct-file helpers share a compact donor shape; protect both root and individual files.
    direct_blocks = (
        ('std::uint64_t EstimateDirectFiles', 'return 0;', True),
        ('void CleanDirectFiles', 'return;', False),
    )
    for signature, empty_return, is_estimate in direct_blocks:
        start = text.find(signature)
        if start < 0:
            raise ValueError(f'R2 settings donor drifted: {signature}')
        end = text.find('\n}\n', start)
        block = text[start:end + 3]
        root_line = f'    if (root.empty() || !fs::exists(root, ec)) {empty_return}\n'
        if 'IsPathExcluded(root' not in block:
            block = block.replace(root_line, root_line + f'    if (settings && dpop::full::IsPathExcluded(root, *settings)) {empty_return}\n', 1)
        regular = '        if (!it->is_regular_file(ec)) { ec.clear(); continue; }\n'
        if 'IsPathExcluded(it->path()' not in block:
            block = block.replace(regular, regular + '        if (settings && dpop::full::IsPathExcluded(it->path(), *settings)) { ec.clear(); continue; }\n', 1)
        text = text[:start] + block + text[end + 3:]
    return text


def transform_v034_overlay(v034_root: Path) -> dict[str, str]:
    summary = _original_transform_v034_overlay(v034_root)
    cmake_path = v034_root / 'CMakeLists.txt'
    original = cmake_path.read_text(encoding='utf-8')
    updated = original

    page_layout = v034_root / 'ui' / 'PageLayout.cpp'
    if page_layout.is_file():
        updated = transform_cmake_for_page_layout(updated)
        summary['page_layout_cmake_registered'] = 'true'
    else:
        summary['page_layout_cmake_registered'] = 'false'

    zapret_model = v034_root / 'modules' / 'ZapretCenterModel.cpp'
    if zapret_model.is_file():
        updated = transform_cmake_for_zapret_center(updated)
        summary['zapret_center_cmake_registered'] = 'true'
    else:
        summary['zapret_center_cmake_registered'] = 'false'

    zapret_layout = v034_root / 'ui' / 'pages' / 'ZapretPageLayout.cpp'
    if zapret_layout.is_file():
        updated = transform_cmake_for_zapret_page_layout(updated)
        summary['zapret_page_layout_cmake_registered'] = 'true'
    else:
        summary['zapret_page_layout_cmake_registered'] = 'false'

    startup_page = v034_root / 'ui' / 'pages' / 'StartupPage.cpp'
    updates_page = v034_root / 'ui' / 'pages' / 'UpdatesPage.cpp'
    if startup_page.is_file() and updates_page.is_file():
        updated = transform_cmake_for_shell_parity(updated)
        summary['shell_parity_cmake_registered'] = 'true'
    else:
        summary['shell_parity_cmake_registered'] = 'false'

    if updated != original:
        cmake_path.write_text(updated, encoding='utf-8', newline='\n')

    legacy_pages = (
        'MemoryPage.cpp', 'GuardPage.cpp', 'DiskPage.cpp', 'ApplicationsPage.cpp',
        'WindowsPage.cpp', 'DuplicatesPage.cpp', 'ToolsPage.cpp', 'SettingsPage.cpp',
    )
    migrated_pages = 0
    for name in legacy_pages:
        path = v034_root / 'ui' / 'pages' / name
        if not path.is_file():
            continue
        page_text = path.read_text(encoding='utf-8')
        page_updated = transform_page_content_top(page_text)
        if page_updated != page_text:
            path.write_text(page_updated, encoding='utf-8', newline='\n')
            migrated_pages += 1
    summary['shared_page_layout_migrated'] = str(migrated_pages)

    fullcore_h = v034_root / 'modules' / 'FullCore.h'
    fullcore_cpp = v034_root / 'modules' / 'FullCore.cpp'
    if fullcore_h.is_file() and fullcore_cpp.is_file():
        header_text = fullcore_h.read_text(encoding='utf-8')
        source_text = fullcore_cpp.read_text(encoding='utf-8')
        fullcore_h.write_text(transform_fullcore_header(header_text), encoding='utf-8', newline='\n')
        fullcore_cpp.write_text(transform_fullcore_source(source_text), encoding='utf-8', newline='\n')
        summary['settings_backend_restored'] = 'true'
    else:
        summary['settings_backend_restored'] = 'false'
    return summary


_core.transform_v034_overlay = transform_v034_overlay


def migrate_034(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict:
    _core._run_donor_migration = _run_donor_migration
    _core.transform_v034_overlay = transform_v034_overlay
    report = _original_migrate_034(repository, output, workspace, build=False)
    if build:
        build_report = _build.run_windows_build(repository, output, workspace)
        report['build'] = {'requested': True, **build_report}
    else:
        report['build'] = {'requested': False, 'completed': False, 'tests_passed': False}
    return report


_core.migrate_034 = migrate_034


def __getattr__(name: str):
    return getattr(_core, name)


def main(argv: Sequence[str] | None = None) -> int:
    return _core.main(argv)


if __name__ == '__main__':
    raise SystemExit(main())
