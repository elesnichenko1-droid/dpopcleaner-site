from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = ROOT / "v0418" / "core" / "Version.h"


def test_phase_a_native_identity_is_initial_revision():
    text = VERSION.read_text(encoding="utf-8")
    assert 'kVersion[] = L"0.4.18"' in text
    assert "kVersionCode = 418" in text
    assert "kRevision = 1" in text
    assert "kChannel[] = L\"stable\"" in text
