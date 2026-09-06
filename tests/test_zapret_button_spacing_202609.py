from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = (ROOT / "v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs").read_text(encoding="utf-8")
LOWER = LAYOUT.lower()

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
    assert token.lower() in LOWER, token

assert "private const int responsiverowgap = 10;" in LOWER
assert "private const int responsivecolumngap = 10;" in LOWER
assert "private const int responsivebuttonheight = 40;" in LOWER
assert "private const int responsiveminimumbuttonwidth = 100;" in LOWER
assert "private const int responsivetextpadding = 30;" in LOWER
