import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("version_hook", ROOT / "docs/hooks/version.py")
hook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hook)


class VersionTests(unittest.TestCase):
    def test_reads_repository_version(self):
        self.assertEqual(
            hook.on_page_markdown("Engine {{ engine_version }}"),
            "Engine " + (ROOT / "VERSION").read_text(encoding="utf-8").strip(),
        )

    def test_version_changes_are_not_cached(self):
        with tempfile.TemporaryDirectory() as temporary:
            version_file = Path(temporary) / "VERSION"
            with patch.object(hook, "VERSION_FILE", version_file):
                for version in ("2.0.1", "2.0.2"):
                    version_file.write_text(version + "\n", encoding="utf-8")
                    self.assertEqual(
                        hook.on_page_markdown("{{ engine_version }} / {{ engine_version }}"),
                        f"{version} / {version}",
                    )

    def test_literal_versions_are_unchanged(self):
        markdown = "## 2.0.0\n\nSince 2.00; requires >=2.0.0 <3.0.0; Lua API version 1.\n"
        self.assertEqual(hook.on_page_markdown(markdown), markdown)


if __name__ == "__main__":
    unittest.main()
