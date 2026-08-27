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
    return sorted({
        value for _, value, _ in values
        if any(token in value.lower() for token in (
            "zapret", "winws", "service.bat", "strategy", "strateg", "update", "updater"
        ))
    })


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
        sections.append({
            "name": name,
            "rva": rva,
            "virtual_size": virtual_size,
            "raw_size": raw_size,
            "raw_offset": raw_offset,
        })
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
    # Prefer UTF-16LE when the first few code units have zero high bytes.
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
        if byte == 0:
            break
        if not (0x20 <= byte <= 0x7E):
            break
        chars.append(chr(byte))
        pos += 1
    value = "".join(chars)
    return value if len(value) >= 4 else None


def rip_relative_lea_refs(data, sections, start_offset, end_offset):
    refs = []
    start_offset = max(0, start_offset)
    end_offset = min(len(data), end_offset)
    pos = start_offset
    while pos + 7 <= end_offset:
        rex = data[pos]
        opcode = data[pos + 1]
        modrm = data[pos + 2]
        # x64 LEA reg,[RIP+disp32]: REX 8D modrm(mod=00,r/m=101) disp32
        if 0x40 <= rex <= 0x4F and opcode == 0x8D and (modrm & 0xC7) == 0x05:
            insn_rva = file_offset_to_rva(sections, pos)
            if insn_rva is not None:
                disp = struct.unpack_from("<i", data, pos + 3)[0]
                target_rva = insn_rva + 7 + disp
                target_offset = rva_to_file_offset(sections, target_rva)
                value = decode_string_at(data, target_offset)
                if value:
                    refs.append((pos, target_offset, value))
            pos += 7
            continue
        pos += 1
    return refs


def print_updater_code_refs():
    data = CORE.read_bytes()
    sections = parse_pe_sections(data)
    needle = "Zapret updater module is missing. Reinstall DPopCleaner.".encode("utf-16le")
    error_offset = data.find(needle)
    print("FROZEN_CORE_ZAPRET_UPDATER_CODE_REFS_BEGIN")
    print(f"ERROR_STRING_OFFSET=0x{error_offset:08x}")
    if error_offset >= 0:
        error_rva = file_offset_to_rva(sections, error_offset)
        print(f"ERROR_STRING_RVA=0x{error_rva:08x}" if error_rva is not None else "ERROR_STRING_RVA=NONE")
        text = next((s for s in sections if s["name"] == ".text"), None)
        if text and error_rva is not None:
            refs = rip_relative_lea_refs(
                data,
                sections,
                text["raw_offset"],
                text["raw_offset"] + text["raw_size"],
            )
            xrefs = [item for item in refs if item[1] == error_offset]
            print("ERROR_XREFS=" + ",".join(f"0x{item[0]:08x}" for item in xrefs))
            for xref, _, _ in xrefs:
                print(f"XREF_CONTEXT=0x{xref:08x}")
                nearby = rip_relative_lea_refs(data, sections, xref - 2048, xref + 2048)
                seen = set()
                for insn_offset, target_offset, value in nearby:
                    if value in seen:
                        continue
                    seen.add(value)
                    lower = value.lower()
                    if (
                        any(token in lower for token in ("zapret", "winws", "service", "update", "general", "module"))
                        or re.search(r"(?i)\.(?:exe|bat|cmd|ps1|dll)$", value)
                    ):
                        print(f"LEA@0x{insn_offset:08x}->0x{target_offset:08x}: {value!r}")
    print("FROZEN_CORE_ZAPRET_UPDATER_CODE_REFS_END")


class DPop0417BundledZapretContractTests(unittest.TestCase):
    def test_frozen_core_zapret_expectations_are_visible_in_ci_log(self):
        values = extract_core_zapret_strings()
        print("FROZEN_CORE_ZAPRET_STRINGS_BEGIN")
        for value in values:
            print(repr(value))
        print("FROZEN_CORE_ZAPRET_STRINGS_END")
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


if __name__ == "__main__":
    unittest.main()
