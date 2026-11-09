import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "audit_test_links", Path(__file__).resolve().parents[2] / "tools/audit_test_links.py"
)
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class LinkAuditTests(unittest.TestCase):
    def test_reads_quoted_paths_and_ignores_non_test_commands(self):
        commands = audit.link_commands(
            ': && "/compiler path/c++" -g "source file.o" -o test_example -Wl,-dead_strip && :\n'
            ': && c++ source.o -o orion2re && :\n'
            'c++ -c source.cpp -o source.o\n'
        )
        self.assertEqual(list(commands), ["test_example"])
        self.assertEqual(commands["test_example"][0], "/compiler path/c++")
        self.assertIn("source file.o", commands["test_example"])

    def test_rejects_missing_tests(self):
        with self.assertRaisesRegex(ValueError, "No unit-test"):
            audit.link_commands("ninja: no work to do.")

    def test_rejects_unrecognized_wrapper(self):
        with self.assertRaisesRegex(ValueError, "Unsupported"):
            audit.link_commands("cd elsewhere && c++ source.o -o test_example")

    def test_rejects_duplicate_target(self):
        command = ": && c++ source.o -o test_example && :\n"
        with self.assertRaisesRegex(ValueError, "Ambiguous"):
            audit.link_commands(command * 2)

    def test_relink_keeps_sanitizers_and_reports_failure_without_overwriting_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            normal = directory / "test_example"
            normal.write_bytes(b"original binary")
            output_directory = directory / "audit"
            output_directory.mkdir()
            arguments = [
                "c++", "-fsanitize=address,undefined", "source.o", "-o", "test_example",
                "-Wl,-dead_strip", "-Wl,--gc-sections", "libproduction.a",
            ]
            with patch.object(audit.subprocess, "run", return_value=subprocess.CompletedProcess(
                [], 1, "undefined symbol: missing_dependency"
            )) as run:
                result = audit.audit_one(directory, output_directory, "test_example", arguments)
            command = run.call_args.args[0]
            self.assertIn("-fsanitize=address,undefined", command)
            self.assertIn("libproduction.a", command)
            self.assertNotIn("-Wl,-dead_strip", command)
            self.assertNotIn("-Wl,--gc-sections", command)
            self.assertEqual(command[command.index("-o") + 1], str(output_directory / "test_example"))
            self.assertEqual(normal.read_bytes(), b"original binary")
            self.assertEqual(arguments[arguments.index("-o") + 1], "test_example")
            self.assertEqual(result["returncode"], 1)
            self.assertIn("undefined symbol", result["output"])


if __name__ == "__main__":
    unittest.main()
