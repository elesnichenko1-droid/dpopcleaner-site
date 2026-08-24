from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "tools" / "dpop035_core.py"


def load_core():
    spec = importlib.util.spec_from_file_location("dpop035_core", CORE)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_035_identity_is_monotonic_and_exact():
    m = load_core()
    assert m.TARGET_VERSION == "0.3.5"
    assert m.TARGET_DISPLAY_VERSION == "0.3.5 BETA R1"
    assert m.TARGET_VERSION_CODE == "3051"
    assert m.TARGET_REVISION == "1"
    assert m.TARGET_RESOURCE_VERSION == "0.3.5.1"


def test_backend_allowlist_cannot_replace_ui_host():
    m = load_core()
    assert set(m.MODERN_BACKEND_ROOTS) == {"core", "modules", "update"}
    assert "ui" not in m.MODERN_BACKEND_ROOTS
    assert "MainWindow.cpp" not in m.MODERN_BACKEND_ROOTS


def test_overlay_empty_is_safe(tmp_path):
    m = load_core()
    source = tmp_path / "overlay"
    target = tmp_path / "target"
    source.mkdir()
    target.mkdir()
    assert m.apply_overlay(source, target) == []


def test_overlay_rejects_symlink(tmp_path):
    m = load_core()
    source = tmp_path / "overlay"
    target = tmp_path / "target"
    source.mkdir()
    target.mkdir()
    real = source / "real.txt"
    real.write_text("x", encoding="utf-8")
    link = source / "link.txt"
    try:
        link.symlink_to(real)
    except OSError:
        return
    try:
        m.apply_overlay(source, target)
    except ValueError as exc:
        assert "symlink" in str(exc).lower()
    else:
        raise AssertionError("symlink overlay must be rejected")


def test_guarded_replace_rejects_donor_drift():
    m = load_core()
    try:
        m.guarded_replace("abc", "missing", "x", "unit")
    except ValueError as exc:
        assert "expected marker missing" in str(exc)
    else:
        raise AssertionError("donor drift must fail")
