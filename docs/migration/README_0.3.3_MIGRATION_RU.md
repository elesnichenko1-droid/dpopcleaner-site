# DPopCleaner 0.3.3 — migration notes

0.3.3 собирается по схеме **faithful UX/поведение 0.2.14 → recovered source → функциональное ядро 0.3.2 → DPopCleaner 0.3.3 BETA R1**.

Канонические файлы: `tools/dpop033_core.py`, `tools/dpop033_migrate.py`, `tests/test_dpop033_migrate.py`, `tests/test_dpop033_recovery_controls.py` и два 0.3.3 workflow.

Локально запускайте `.\scripts\RUN_DPopCleaner_0.3.3_MIGRATION.ps1`.

Публикационный workflow формирует `DPopCleaner_Setup_0.3.3_BETA_R1.exe`, release `v0.3.3-beta-r1`, фактический `update/beta.json` и GitHub Pages. Старый R4 не используется как fallback.
