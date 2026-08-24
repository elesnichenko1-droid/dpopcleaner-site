from __future__ import annotations

import base64
import gzip
import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "dpop033_migrate.py"


def load_module():
    spec = importlib.util.spec_from_file_location("dpop033_migrate", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot create module spec")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_workflow(payload: dict) -> str:
    raw = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    encoded = base64.b64encode(gzip.compress(raw)).decode("ascii")
    chunks = [encoded[i : i + 76] for i in range(0, len(encoded), 76)]
    block = "\n".join(f"            {chunk}" for chunk in chunks)
    return (
        "name: test\n"
        "jobs:\n"
        "  master:\n"
        "    steps:\n"
        "      - name: payload\n"
        "        shell: pwsh\n"
        "        env:\n"
        "          DPOP_PAYLOAD_B64_GZIP: |\n"
        f"{block}\n"
        "        run: |\n"
        "          Write-Host ok\n"
    )


class PayloadExtractionTests(unittest.TestCase):
    def test_extract_embedded_payload_round_trip(self):
        m = load_module()
        expected = {
            "files": [
                {
                    "path": "v032/Version.h",
                    "content_base64": base64.b64encode(b"version").decode("ascii"),
                }
            ],
            "delete": ["scripts/old.ps1"],
        }
        actual = m.extract_embedded_payload(make_workflow(expected))
        self.assertEqual(actual, expected)

    def test_extract_rejects_missing_block(self):
        m = load_module()
        with self.assertRaisesRegex(ValueError, "DPOP_PAYLOAD_B64_GZIP"):
            m.extract_embedded_payload("name: no-payload\n")

    def test_extract_rejects_invalid_base64(self):
        m = load_module()
        workflow = (
            "env:\n"
            "  DPOP_PAYLOAD_B64_GZIP: |\n"
            "    this-is-not-base64***\n"
            "run: echo no\n"
        )
        with self.assertRaisesRegex(ValueError, "base64"):
            m.extract_embedded_payload(workflow)


class PayloadPathTests(unittest.TestCase):
    def test_accepts_only_v032_and_scripts(self):
        m = load_module()
        self.assertEqual(str(m.validate_payload_path("v032/ui/Shell.cpp")), "v032/ui/Shell.cpp")
        self.assertEqual(str(m.validate_payload_path("scripts/Prepare-032Source.ps1")), "scripts/Prepare-032Source.ps1")

    def test_rejects_traversal(self):
        m = load_module()
        for candidate in (
            "../index.html",
            "v032/../../index.html",
            "/v032/MainWindow.cpp",
            "v032/../r4/MainWindow.cpp",
        ):
            with self.subTest(candidate=candidate):
                with self.assertRaises(ValueError):
                    m.validate_payload_path(candidate)

    def test_rejects_protected_or_unrelated_paths(self):
        m = load_module()
        for candidate in (
            "r4/MainWindow.cpp",
            "downloads/DPopCleaner.exe",
            "update/beta.json",
            "index.html",
            "MainWindow.cpp",
        ):
            with self.subTest(candidate=candidate):
                with self.assertRaises(ValueError):
                    m.validate_payload_path(candidate)

    def test_apply_payload_writes_and_deletes_only_allowed_paths(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "scripts").mkdir()
            (root / "scripts" / "old.ps1").write_text("old", encoding="utf-8")
            payload = {
                "files": [
                    {
                        "path": "v032/Version.h",
                        "content_base64": base64.b64encode(b"new-version").decode("ascii"),
                    }
                ],
                "delete": ["scripts/old.ps1"],
            }
            writes, deletes = m.apply_payload(root, payload)
            self.assertEqual((writes, deletes), (1, 1))
            self.assertEqual((root / "v032" / "Version.h").read_bytes(), b"new-version")
            self.assertFalse((root / "scripts" / "old.ps1").exists())


class VersionTransformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.mkdtemp()
        self.root = Path(self.temp) / "v033"
        self.root.mkdir(parents=True)
        (self.root / "CMakeLists.txt").write_text(
            "project(DPopCleaner VERSION 0.3.2 LANGUAGES CXX RC)\n"
            "add_executable(ShellModelTests tests/v032/ShellModelTests.cpp src/ui/ShellModel.cpp)\n",
            encoding="utf-8",
        )
        (self.root / "Version.h").write_text(
            '#pragma once\nnamespace dpop::version {\n'
            'inline constexpr wchar_t kVersion[] = L"0.3.2";\n'
            'inline constexpr wchar_t kDisplayVersion[] = L"0.3.2 BETA R1";\n'
            'inline constexpr int kVersionCode = 3021;\n'
            'inline constexpr int kRevision = 1;\n}\n',
            encoding="utf-8",
        )
        (self.root / "version.rc.in").write_text(
            "FILEVERSION 0,3,2,1\n"
            "PRODUCTVERSION 0,3,2,1\n"
            'VALUE "FileVersion", "0.3.2.1\\0"\n'
            'VALUE "ProductVersion", "0.3.2 BETA R1\\0"\n',
            encoding="utf-8",
        )
        (self.root / "ui").mkdir()
        (self.root / "tests").mkdir()
        (self.root / "ui" / "Shell.h").write_text(
            'inline constexpr wchar_t kTitle[] = L"DPopCleaner 0.3.2 BETA R1";\n'
            'inline constexpr wchar_t kInternal[] = L"DPopCleaner032ShellWindow";\n',
            encoding="utf-8",
        )
        (self.root / "ui" / "StatusBar.cpp").write_text(
            'auto version = L"v0.3.2 BETA";\n', encoding="utf-8"
        )
        (self.root / "tests" / "ShellContractTests.cpp").write_text(
            'auto expected = L"DPopCleaner 0.3.2 BETA R1";\n', encoding="utf-8"
        )
        (self.root / "tests" / "SessionLogTests.cpp").write_text(
            'auto expected = L"DPopCleaner 0.3.2 запущен.";\n', encoding="utf-8"
        )

    def tearDown(self):
        shutil.rmtree(self.temp)

    def test_transform_v033_overlay_rewrites_all_version_markers(self):
        m = load_module()
        summary = m.transform_v033_overlay(self.root)
        self.assertEqual(summary["version"], "0.3.3")
        cmake = (self.root / "CMakeLists.txt").read_text(encoding="utf-8")
        header = (self.root / "Version.h").read_text(encoding="utf-8")
        resource = (self.root / "version.rc.in").read_text(encoding="utf-8")
        self.assertIn("VERSION 0.3.3", cmake)
        self.assertIn("tests/v033/", cmake)
        self.assertNotIn("tests/v032/", cmake)
        self.assertIn('kVersion[] = L"0.3.3"', header)
        self.assertIn('kDisplayVersion[] = L"0.3.3 BETA R1"', header)
        self.assertIn("kVersionCode = 3031", header)
        self.assertIn("FILEVERSION 0,3,3,1", resource)
        self.assertIn('"0.3.3.1\\0"', resource)
        self.assertIn('"0.3.3 BETA R1\\0"', resource)
        shell = (self.root / "ui" / "Shell.h").read_text(encoding="utf-8")
        status = (self.root / "ui" / "StatusBar.cpp").read_text(encoding="utf-8")
        shell_test = (self.root / "tests" / "ShellContractTests.cpp").read_text(encoding="utf-8")
        log_test = (self.root / "tests" / "SessionLogTests.cpp").read_text(encoding="utf-8")
        self.assertIn("DPopCleaner 0.3.3 BETA R1", shell)
        self.assertIn("DPopCleaner032ShellWindow", shell, "internal class names must stay stable")
        self.assertIn("v0.3.3 BETA", status)
        self.assertIn("DPopCleaner 0.3.3 BETA R1", shell_test)
        self.assertIn("DPopCleaner 0.3.3 запущен.", log_test)

    def test_transform_fails_closed_when_expected_marker_missing(self):
        m = load_module()
        (self.root / "Version.h").write_text("#pragma once\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "expected marker"):
            m.transform_v033_overlay(self.root)


class WorkspaceSafetyTests(unittest.TestCase):
    def test_workspace_must_not_be_inside_repository(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td) / "repo"
            repo.mkdir()
            with self.assertRaises(ValueError):
                m.validate_workspace(repo, repo / "stage")

    def test_workspace_outside_repository_is_allowed(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            repo = base / "repo"
            workspace = base / "workspace"
            repo.mkdir()
            m.validate_workspace(repo, workspace)


class BuildCommandTests(unittest.TestCase):
    def test_windows_build_commands_use_vs2022_x64_release_and_ctest(self):
        m = load_module()
        commands = m.windows_build_commands(Path("C:/src"), Path("C:/build"))
        flat = [item for command in commands for item in command]
        self.assertIn("Visual Studio 17 2022", flat)
        self.assertIn("x64", flat)
        self.assertIn("-DBUILD_TESTING=ON", flat)
        self.assertIn("Release", flat)
        self.assertTrue(any(command[0] == "ctest" for command in commands))


class WorkflowContractTests(unittest.TestCase):
    def test_workflow_runs_tests_build_ui_capture_and_uploads_evidence(self):
        root = MODULE_PATH.parents[1]
        workflow = (root / ".github" / "workflows" / "DPopCleaner_0.3.3_REVERSE_MIGRATION.yml").read_text(encoding="utf-8")
        self.assertIn("python -m unittest -v tests/test_dpop033_migrate.py", workflow)
        self.assertIn("tools/dpop033_migrate.py", workflow)
        self.assertIn("--build", workflow)
        self.assertIn("tools/dpop033_ui_smoke.ps1", workflow)
        self.assertIn("actions/upload-artifact@v4", workflow)

    def test_ui_smoke_contract_has_three_viewports_and_white_regression_gate(self):
        root = MODULE_PATH.parents[1]
        smoke = (root / "tools" / "dpop033_ui_smoke.ps1").read_text(encoding="utf-8")
        self.assertIn("default-1200x850", smoke)
        self.assertIn("minimum-1100x700", smoke)
        self.assertIn("maximized", smoke)
        self.assertIn("near_white_ratio", smoke)
        self.assertIn("$ratio -gt 0.05", smoke)
        self.assertIn("WM_CLOSE", smoke)


class RecoveryPolicyCommandTests(unittest.TestCase):
    def test_recovery_policy_commands_cover_all_repository_gates(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            for relative in m.RECOVERY_POLICY_SCRIPTS:
                path = stage / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("# fixture\n", encoding="utf-8")
            commands = m.recovery_policy_commands(stage, "pwsh")
            self.assertEqual(len(commands), 5)
            flattened = "\n".join(" ".join(command) for command in commands)
            self.assertIn("CleanReleasePolicy.Tests.ps1", flattened)
            self.assertIn("R3ReleasePolicy.Tests.ps1", flattened)
            self.assertIn("R3WorkflowPolicy.Tests.ps1", flattened)
            self.assertIn("Ui032Policy.Tests.ps1", flattened)
            self.assertIn("FaithfulRecoveryPolicy.Tests.ps1", flattened)
            self.assertTrue(all(command[0] == "pwsh" for command in commands))

    def test_recovery_policy_commands_fail_closed_when_gate_is_missing(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            with self.assertRaisesRegex(RuntimeError, "missing"):
                m.recovery_policy_commands(stage, "pwsh")


class IntegrationMigrationNoBuildTests(unittest.TestCase):
    def test_no_build_migration_is_isolated_and_exports_v033(self):
        m = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            repo = base / "repo"
            output = base / "output"
            workspace = base / "workspace"
            repo.mkdir()

            import subprocess
            subprocess.run(["git", "init"], cwd=repo, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=repo, check=True)
            subprocess.run(["git", "config", "user.name", "DPop Test"], cwd=repo, check=True)

            (repo / ".github" / "workflows").mkdir(parents=True)
            (repo / "scripts").mkdir()
            (repo / "v032" / "tests").mkdir(parents=True)
            (repo / "v032" / "ui").mkdir()
            (repo / "v032" / "modules").mkdir()
            (repo / "downloads").mkdir()

            # The baseline EXE is optional to allow source-only checkouts.
            (repo / "scripts" / "Prepare-R3Source.ps1").write_text("# fixture\n", encoding="utf-8")
            (repo / "scripts" / "R3ReleasePolicy.psm1").write_text("# fixture\n", encoding="utf-8")
            (repo / "v032" / "CMakeLists.txt").write_text(
                "project(DPopCleaner VERSION 0.3.2 LANGUAGES CXX RC)\n"
                "add_executable(ShellModelTests tests/v032/ShellModelTests.cpp)\n",
                encoding="utf-8",
            )
            (repo / "v032" / "Version.h").write_text(
                '#pragma once\nnamespace dpop::version {\n'
                'inline constexpr wchar_t kVersion[] = L"0.3.2";\n'
                'inline constexpr wchar_t kDisplayVersion[] = L"0.3.2 BETA R1";\n'
                'inline constexpr int kVersionCode = 3021;\n'
                'inline constexpr int kRevision = 1;\n}\n',
                encoding="utf-8",
            )
            (repo / "v032" / "version.rc.in").write_text(
                "FILEVERSION 0,3,2,1\n"
                "PRODUCTVERSION 0,3,2,1\n"
                'VALUE "FileVersion", "0.3.2.1\\0"\n'
                'VALUE "ProductVersion", "0.3.2 BETA R1\\0"\n',
                encoding="utf-8",
            )
            (repo / "v032" / "MainWindow.cpp").write_text("old-shell\n", encoding="utf-8")
            (repo / "v032" / "tests" / "ShellModelTests.cpp").write_text("// test\n", encoding="utf-8")
            (repo / "v032" / "ui" / "Shell.cpp").write_text("// ui\n", encoding="utf-8")
            (repo / "v032" / "modules" / "FullCore.cpp").write_text("// core\n", encoding="utf-8")

            payload = {
                "files": [
                    {
                        "path": "v032/MainWindow.cpp",
                        "content_base64": base64.b64encode(b"faithful-0214-shell\n").decode("ascii"),
                    }
                ],
                "delete": [],
            }
            workflow = make_workflow(payload)
            (repo / m.RECOVERY_WORKFLOW).write_text(workflow, encoding="utf-8")

            subprocess.run(["git", "add", "."], cwd=repo, check=True)
            subprocess.run(["git", "commit", "-m", "fixture"], cwd=repo, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

            report = m.migrate(repo, output, workspace, build=False, keep_worktree=False)

            self.assertEqual(report["target_version"], "0.3.3")
            self.assertEqual(report["payload"]["writes"], 1)
            self.assertFalse((repo / "v033").exists(), "source checkout must remain unchanged")
            self.assertEqual((repo / "v032" / "MainWindow.cpp").read_text(encoding="utf-8"), "old-shell\n")

            exported = output / "source-overlay" / "v033"
            self.assertTrue(exported.is_dir())
            self.assertEqual((exported / "MainWindow.cpp").read_text(encoding="utf-8"), "faithful-0214-shell\n")
            self.assertIn("0.3.3", (exported / "Version.h").read_text(encoding="utf-8"))
            self.assertIn("tests/v033/", (exported / "CMakeLists.txt").read_text(encoding="utf-8"))
            self.assertTrue((output / "source-overlay" / "scripts" / "Prepare-033Source.ps1").is_file())
            self.assertTrue((output / "migration-report.json").is_file())
            self.assertFalse((workspace / "repo-stage").exists(), "detached worktree should be removed")

    def test_prepare_033_script_never_uses_v032_overlay_paths(self):
        m = load_module()
        script = m._prepare_033_script_text()
        self.assertIn("v033/CMakeLists.txt", script)
        self.assertIn("tests/v033", script)
        self.assertNotIn("v032/", script)


if __name__ == "__main__":
    unittest.main()
