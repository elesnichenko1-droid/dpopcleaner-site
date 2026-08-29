from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "downloads" / "DPopCleaner_0.2.14_BETA.exe"


def extract_strings_with_offsets(data):
    values = []
    for match in re.finditer(rb"[\x20-\x7e]{4,}", data):
        values.append((match.start(), match.group().decode("ascii", errors="ignore"), "ascii"))
    for match in re.finditer(rb"(?:[\x20-\x7e]\x00){4,}", data):
        values.append((match.start(), match.group().decode("utf-16le", errors="ignore"), "utf16"))
    return sorted(values, key=lambda item: item[0])


def extract_core_zapret_strings():
    values = extract_strings_with_offsets(CORE.read_bytes())
    return sorted({value for _, value, _ in values if any(token in value.lower() for token in (
        "zapret", "winws", "service.bat", "strategy", "strateg", "update", "updater"
    ))})


def parse_pe_sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise AssertionError("Frozen core is not a PE image")
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    table = pe + 24 + optional_size
    sections = []
    for index in range(section_count):
        off = table + index * 40
        name = data[off:off + 8].split(b"\0", 1)[0].decode("ascii", errors="replace")
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, off + 8)
        sections.append({"name": name, "rva": rva, "virtual_size": virtual_size, "raw_size": raw_size, "raw_offset": raw_offset})
    return sections


def file_offset_to_rva(sections, file_offset):
    for section in sections:
        start = section["raw_offset"]
        end = start + section["raw_size"]
        if start <= file_offset < end:
            return section["rva"] + (file_offset - start)
    return None


def rva_to_file_offset(sections, rva):
    for section in sections:
        start = section["rva"]
        span = max(section["virtual_size"], section["raw_size"])
        if start <= rva < start + span:
            delta = rva - start
            if delta < section["raw_size"]:
                return section["raw_offset"] + delta
    return None


def decode_string_at(data, offset):
    if offset is None or offset < 0 or offset >= len(data):
        return None
    if offset + 8 <= len(data) and data[offset + 1] == 0 and data[offset + 3] == 0:
        chars = []
        pos = offset
        while pos + 1 < len(data) and len(chars) < 300:
            code = struct.unpack_from("<H", data, pos)[0]
            if code == 0:
                break
            if code < 0x20 and code not in (9, 10, 13):
                break
            chars.append(chr(code))
            pos += 2
        value = "".join(chars)
        if len(value) >= 4:
            return value
    chars = []
    pos = offset
    while pos < len(data) and len(chars) < 300:
        byte = data[pos]
        if byte == 0 or not (0x20 <= byte <= 0x7E):
            break
        chars.append(chr(byte))
        pos += 1
    value = "".join(chars)
    return value if len(value) >= 4 else None


def rip_relative_lea_refs(data, sections, start_offset, end_offset):
    refs = []
    pos = max(0, start_offset)
    end_offset = min(len(data), end_offset)
    while pos + 7 <= end_offset:
        rex, opcode, modrm = data[pos], data[pos + 1], data[pos + 2]
        if 0x40 <= rex <= 0x4F and opcode == 0x8D and (modrm & 0xC7) == 0x05:
            insn_rva = file_offset_to_rva(sections, pos)
            if insn_rva is not None:
                disp = struct.unpack_from("<i", data, pos + 3)[0]
                target_offset = rva_to_file_offset(sections, insn_rva + 7 + disp)
                value = decode_string_at(data, target_offset)
                if value:
                    refs.append((pos, target_offset, value))
            pos += 7
            continue
        pos += 1
    return refs


def find_string_offsets(data, value):
    needles = [value.encode("ascii", errors="ignore"), value.encode("utf-16le")]
    offsets = []
    for needle in needles:
        start = 0
        while True:
            pos = data.find(needle, start)
            if pos < 0:
                break
            offsets.append(pos)
            start = pos + 1
    return sorted(set(offsets))


def print_updater_code_refs():
    data = CORE.read_bytes()
    sections = parse_pe_sections(data)
    text = next((s for s in sections if s["name"] == ".text"), None)
    refs = rip_relative_lea_refs(data, sections, text["raw_offset"], text["raw_offset"] + text["raw_size"])
    print("FROZEN_CORE_ZAPRET_UPDATER_CODE_REFS_BEGIN")
    for label, value in (
        ("UPDATER_ERROR", "Zapret updater module is missing. Reinstall DPopCleaner."),
        ("DPOPUPDATE", "DPopUpdate.bat"),
        ("ZAPRET_FOLDER", "zapret-discord-youtube"),
        ("NO_STRATEGIES", "No strategies found"),
        ("GENERAL_BAT", "general.bat"),
        ("BAT_GLOB", "*.bat"),
        ("WINWS", "bin\\winws.exe"),
        ("SERVICE", "service.bat"),
        ("LEGACY_ZAPRET_VERSION", "1.9.9d"),
    ):
        offsets = find_string_offsets(data, value)
        print(label + "_STRING_OFFSETS=" + ",".join(f"0x{x:08x}" for x in offsets))
        xrefs = [item[0] for item in refs if item[1] in offsets]
        print(label + "_XREFS=" + ",".join(f"0x{x:08x}" for x in xrefs))
    print("FROZEN_CORE_ZAPRET_UPDATER_CODE_REFS_END")


class DPop0417BundledZapretContractTests(unittest.TestCase):
    def test_frozen_core_zapret_expectations_are_visible_in_ci_log(self):
        values = extract_core_zapret_strings()
        print_updater_code_refs()
        self.assertTrue(values, "Frozen core should contain discoverable Zapret integration strings")
        self.assertIn("bin\\winws.exe", values)
        self.assertIn("service.bat", values)

    def test_release_stages_real_zapret_runtime_not_only_screen_fix(self):
        allowlist = (ROOT / "v0417" / "stage-allowlist.txt").read_text(encoding="utf-8")
        stage = (ROOT / "tools" / "dpop0417_stage.ps1").read_text(encoding="utf-8")
        install_smoke = (ROOT / "tools" / "dpop0417_install_smoke.ps1").read_text(encoding="utf-8")
        publisher = (ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.17.yml").read_text(encoding="utf-8")
        self.assertIn("ZapretScreenFix.exe", allowlist)
        self.assertIn("ZapretScreenFix.exe", stage)
        self.assertIn("winws.exe", stage, "Stage must require the real Zapret runtime")
        self.assertIn("WinDivert64.sys", stage, "Stage must require the WinDivert driver")
        self.assertIn("service.bat", stage, "Stage must require Flowseal service management")
        self.assertIn("general.bat", stage, "Stage must require at least one real strategy")
        self.assertIn("winws.exe", install_smoke, "Fresh installed package must prove winws.exe exists")
        self.assertIn("WinDivert64.sys", install_smoke)
        self.assertIn("service.bat", install_smoke)
        self.assertIn("prepare_zapret", publisher.lower(), "Release workflow must prepare a pinned Zapret payload")

    def test_authentic_ui_smoke_accepts_populated_combo_without_default_selection(self):
        smoke = (ROOT / "tools" / "dpop0417_zapret_ui_smoke.ps1").read_text(encoding="utf-8")
        self.assertIn("ComboBoxItems", smoke, "Smoke must inspect actual ComboBox entries, not only window text")
        self.assertIn("strategy_entries", smoke, "Evidence report must record the strategies exposed by the authentic UI")
        self.assertNotIn(
            "Zapret strategy ComboBox has entries but no selected strategy text.",
            smoke,
            "A populated old ComboBox is valid even when the legacy UI has no default selection",
        )


if __name__ == "__main__":
    unittest.main()
