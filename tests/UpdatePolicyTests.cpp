#include "update/UpdateManifest.h"
#include "update/UpdatePolicy.h"

#include <iostream>
#include <string>

namespace {
int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    } else {
        std::cout << "PASS: " << name << '\n';
    }
}
}

int main() {
    using dpop::update::CheckMode;
    using dpop::update::ResultAction;

    Expect(!dpop::update::IsUpdateOfferAllowed(false, 3013, 9999),
           "unavailable manifest is never offered");
    Expect(!dpop::update::IsUpdateOfferAllowed(true, 3013, 3013),
           "equal version is never offered");
    Expect(!dpop::update::IsUpdateOfferAllowed(true, 3013, 3012),
           "lower version is never offered");
    Expect(dpop::update::IsUpdateOfferAllowed(true, 3013, 3014),
           "higher available version is offered");

    Expect(dpop::update::DecideUpdateResult(CheckMode::Background, true, true) == ResultAction::RecordAvailable,
           "background availability is recorded without install flow");
    Expect(dpop::update::DecideUpdateResult(CheckMode::Background, false, false) == ResultAction::Ignore,
           "background errors are non-interactive");
    Expect(dpop::update::DecideUpdateResult(CheckMode::Interactive, true, true) == ResultAction::OfferInstall,
           "explicit interactive check may offer install");
    Expect(dpop::update::DecideUpdateResult(CheckMode::Interactive, true, false) == ResultAction::ShowCurrent,
           "interactive current version reports success");
    Expect(dpop::update::DecideUpdateResult(CheckMode::Interactive, false, false) == ResultAction::ShowError,
           "interactive failure reports an error");

    const std::wstring arguments = dpop::update::BuildUpdaterArguments(
        42,
        LR"(C:\Temp Folder\DPopCleaner_Setup.exe)",
        std::wstring(64, L'a'),
        true,
        L"/SILENT /NORESTART");
    Expect(arguments ==
               L"--parent 42 --package \"C:\\Temp Folder\\DPopCleaner_Setup.exe\" --sha256 \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\" --allow-unsigned --args \"/SILENT /NORESTART\"",
           "updater arguments contain only the approved explicit-install fields");
    Expect(arguments.find(L"--restart") == std::wstring::npos,
           "updater arguments never request an application restart");

    const std::string validManifest = R"({
        "product":"DPopCleaner",
        "channel":"beta",
        "version":"0.3.2",
        "version_code":3020,
        "revision":1,
        "mandatory":false,
        "download_url":"https://example.test/setup.exe",
        "sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "size":12345,
        "signed":false,
        "available":true,
        "notes_url":"https://example.test/notes",
        "install_args":"/SILENT /NORESTART"
    })";
    dpop::update::Manifest manifest{};
    std::wstring error;
    Expect(dpop::update::ParseManifestUtf8(validManifest, manifest, error),
           "complete HTTPS manifest parses");
    Expect(manifest.available && manifest.size == 12345 && manifest.versionCode == 3020,
           "parser preserves availability size and version code");

    auto invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("DPopCleaner"), std::string("DPopCleaner").size(), "OtherProduct");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "wrong manifest product is rejected");

    invalidManifest = validManifest;
    invalidManifest.erase(invalidManifest.find("\"product\":\"DPopCleaner\","),
                          std::string("\"product\":\"DPopCleaner\",").size());
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "missing manifest product is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("\"beta\""), std::string("\"beta\"").size(), "\"stable\"");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "wrong manifest channel is rejected");

    invalidManifest = validManifest;
    invalidManifest.erase(invalidManifest.find("\"channel\":\"beta\","),
                          std::string("\"channel\":\"beta\",").size());
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "missing manifest channel is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("https://example.test/setup.exe"),
                            std::string("https://example.test/setup.exe").size(),
                            "http://example.test/setup.exe");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "HTTP package URL is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find(std::string(64, 'a')), 64, "abcd");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "short SHA-256 is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("12345"), 5, "0");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "zero package size is rejected");

    invalidManifest = validManifest;
    invalidManifest.erase(invalidManifest.find("\"available\":true,"), std::string("\"available\":true,").size());
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "missing availability flag is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("3020"), 4,
                            "99999999999999999999999999999999999999999999999999");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "out-of-range manifest integers are rejected without terminating the app");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("3020"), 4, "2147483648");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "version code outside the native int range is rejected");

    invalidManifest = validManifest;
    invalidManifest.replace(invalidManifest.find("\"revision\":1"),
                            std::string("\"revision\":1").size(),
                            "\"revision\":2147483648");
    Expect(!dpop::update::ParseManifestUtf8(invalidManifest, manifest, error),
           "revision outside the native int range is rejected");

    if (failures != 0) {
        std::cerr << failures << " update policy test(s) failed.\n";
        return 1;
    }
    std::cout << "All update policy tests passed.\n";
    return 0;
}
