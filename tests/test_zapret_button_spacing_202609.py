from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = (ROOT / "v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs").read_text(encoding="utf-8")
LOWER = LAYOUT.lower()


def test_zapret_button_layout_keeps_existing_contract_helpers():
    for token in (
        "LayoutZapretRow",
        "LayoutUpdateRowWithCompactToggles",
        "LayoutCompactServiceRow",
        "StrategyRowButtonIds",
        "PrimaryUpdateButtonIds",
        "CompactUpdateToggleButtonIds",
        "BridgeActionButtonIds",
        "PrimaryAdditionalRowButtonIds",
        "ServiceActionButtonIds",
    ):
        assert token.lower() in LOWER


def test_zapret_button_spacing_is_intentionally_roomy():
    assert "private const int responsive row gap = 10;" not in LOWER
    assert "private const int responsive row gap = 10;".replace("responsive row gap", "ResponsiveRowGap").lower() in LOWER
    assert "private const int ResponsiveColumnGap = 10;".lower() in LOWER
    assert "private const int ResponsiveButtonHeight = 40;".lower() in LOWER
    assert "private const int ResponsiveMinimumButtonWidth = 100;".lower() in LOWER
    assert "private const int ResponsiveTextPadding = 30;".lower() in LOWER
