"""Regression tests for the shared build version and release tag gate."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "cmake").mkdir()
        shutil.copy2(ROOT / "cmake/version.cmake", self.root / "cmake")

    def check_version(self, version, tag=None):
        (self.root / "VERSION").write_text(version, encoding="utf-8")
        command = ["cmake"]
        if tag is not None:
            command.append(f"-DRELEASE_TAG={tag}")
        command.extend(["-P", str(self.root / "cmake/version.cmake")])
        return subprocess.run(command, capture_output=True, text=True)

    def test_valid_versions(self):
        for version in ("0.0.0", "2.0.1", "12.34.56"):
            with self.subTest(version=version):
                result = self.check_version(version + "\n", "v" + version)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_invalid_versions(self):
        for version in ("", "2.0", "v2.0.1", "02.0.1", "2.00.1", "2.0.01",
                        "2.0.1-rc1", "2.0.1.0", "2.0.1\n3.0.0", "2x0x1"):
            with self.subTest(version=version):
                result = self.check_version(version)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("VERSION must contain", result.stderr)

    def test_release_tag_must_match(self):
        for tag in ("v2.0.0", "2.0.1", "main", "v2.0.1-rc1", ""):
            with self.subTest(tag=tag):
                result = self.check_version("2.0.1", tag)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("expected 'v2.0.1'", result.stderr)

    def test_development_build_needs_no_tag(self):
        result = self.check_version("2.0.1")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_repository_version(self):
        result = self.check_version((ROOT / "VERSION").read_text(encoding="utf-8"))
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()