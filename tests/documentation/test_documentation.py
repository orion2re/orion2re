import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pdf = load_tool("build_documentation_pdf")
environment = load_tool("docs_environment")
audit = load_tool("audit_documentation")


class ResourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.site = self.root / "site"
        self.site.mkdir()

    def test_local_and_inline_resources(self):
        pdf.check_resource((self.site / "image.png").as_uri(), self.site)
        pdf.check_resource("data:image/png;base64,AAAA", self.site)

    def test_remote_and_outside_resources_are_rejected(self):
        for url in ("https://example.com/image.png", "http://127.0.0.1/",
                    "ftp://example.com/file", "file://example.com/etc/passwd",
                    (self.root / "secret").as_uri(),
                    self.site.as_uri() + "/%2e%2e/secret"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                pdf.check_resource(url, self.site)

    def test_symlink_escape_is_rejected(self):
        secret = self.root / "secret"
        secret.write_text("not for publication", encoding="utf-8")
        link = self.site / "image.png"
        link.symlink_to(secret)
        with self.assertRaises(ValueError):
            pdf.check_resource(link.as_uri(), self.site)

    def test_bootstrap_checksum_is_checked(self):
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            environment.verify_pip(b"tampered wheel")
        data = b"test wheel"
        with patch.object(environment, "PIP_SHA256", hashlib.sha256(data).hexdigest()):
            environment.verify_pip(data)

    def test_pdf_python_minimum_includes_patch_version(self):
        for version in ((3, 10, 16), (3, 11, 0), (3, 11, 4)):
            with self.subTest(version=version), \
                 patch.object(environment.sys, "version_info", version), \
                 patch.object(environment.venv, "EnvBuilder") as builder, \
                 self.assertRaisesRegex(ValueError, r"Python 3\.11\.5\+"):
                environment.create_environment(self.root / "venv", minimum_python=(3, 11, 5))
            builder.assert_not_called()

        for version in ((3, 11, 5), (3, 11, 9), (3, 12, 0)):
            with self.subTest(version=version), \
                 patch.object(environment.sys, "version_info", version), \
                 patch.object(environment.venv, "EnvBuilder") as builder, \
                 patch.object(environment.urllib.request, "urlopen", return_value=io.BytesIO(b"wheel")), \
                 patch.object(environment, "verify_pip"), \
                 patch.object(environment.subprocess, "run"):
                environment.create_environment(self.root / "venv", minimum_python=(3, 11, 5))
                builder.return_value.create.assert_called_once()

    def test_pdf_bootstrap_uses_secure_python_minimum(self):
        with patch.object(environment.sys, "argv", ["docs_environment.py", "venv", "--pdf"]), \
             patch.object(environment, "create_environment") as create:
            environment.main()
        create.assert_called_once_with(Path("venv"), None, (3, 11, 5))


class PdfValidationTests(unittest.TestCase):
    def setUp(self):
        from pypdf import PdfWriter

        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "documentation.pdf"
        self.writer = PdfWriter()
        self.writer.add_blank_page(width=595, height=842)
        self.writer.add_outline_item("Documentation", 0)

    def test_valid_pdf(self):
        self.writer.write(self.path)
        self.assertEqual(pdf.validate_pdf(self.path), 1)

    def test_attachments_are_rejected(self):
        self.writer.add_attachment("secret.txt", b"private")
        self.writer.write(self.path)
        with self.assertRaisesRegex(ValueError, "attachments"):
            pdf.validate_pdf(self.path)

    def test_javascript_is_rejected(self):
        with self.assertWarns(DeprecationWarning):
            self.writer.add_js("app.alert('unexpected')")
        self.writer.write(self.path)
        with self.assertRaisesRegex(ValueError, "JavaScript"):
            pdf.validate_pdf(self.path)

    def test_encryption_is_rejected(self):
        self.writer.encrypt("password")
        self.writer.write(self.path)
        with self.assertRaisesRegex(ValueError, "unencrypted"):
            pdf.validate_pdf(self.path)

    def test_malformed_pdf_is_rejected(self):
        self.path.write_bytes(b"not a PDF")
        with self.assertRaises(Exception):
            pdf.validate_pdf(self.path)

    def test_missing_bookmarks_are_rejected(self):
        from pypdf import PdfWriter

        writer = PdfWriter()
        writer.add_blank_page(width=595, height=842)
        writer.write(self.path)
        with self.assertRaisesRegex(ValueError, "bookmarks"):
            pdf.validate_pdf(self.path)


class BuildGuardTests(unittest.TestCase):
    def run_build(self, markdown, hook=None):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "content").mkdir()
            (root / "content/index.md").write_text(markdown, encoding="utf-8")
            config = (
                "site_name: PDF regression test\ndocs_dir: content\n"
                "theme:\n  name: material\n  font: false\n"
                "plugins:\n  - to-pdf:\n      output_path: documentation.pdf\n"
            )
            if hook:
                (root / "hook.py").write_text(hook, encoding="utf-8")
                config += "hooks:\n  - hook.py\n"
            (root / "mkdocs.yml").write_text(config, encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/build_documentation_pdf.py"),
                 "--config", str(root / "mkdocs.yml"),
                 "--site-dir", str(root / "site"),
                 "--output", str(root / "result.pdf")],
                cwd=root, capture_output=True, text=True, timeout=60,
            )
            return result, (root / "result.pdf").exists()

    def test_remote_image_fails_without_publishing_pdf(self):
        result, published = self.run_build("# Documentation\n\n![Blocked](https://example.invalid/image.png)\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(published)
        self.assertIn("PDF resource outside the generated site", result.stderr)

    def test_socket_connection_is_blocked_in_worker(self):
        result, published = self.run_build(
            "# Documentation\n",
            "import socket\ndef on_pre_build(**kwargs):\n"
            "    with socket.socket() as connection:\n"
            "        connection.connect(('127.0.0.1', 9))\n",
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(published)
        self.assertIn("Network access is disabled", result.stderr)


class AnchorTests(unittest.TestCase):
    def test_generated_anchors_survive_exporter_cleanup(self):
        from bs4 import BeautifulSoup
        from mkdocs_to_pdf.generator import Generator

        spec = importlib.util.spec_from_file_location("pdf_hook", ROOT / "docs/hooks/pdf.py")
        hook = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(hook)
        html = '<p><a id="parameter-build-queue"></a></p><h2 id="automatic-build-queues">Automatic build queues</h2>'
        soup = BeautifulSoup(hook.on_page_content(html), "html.parser")
        Generator._remove_empty_tags(soup)
        self.assertIsNotNone(soup.find(id="parameter-build-queue"))
        self.assertIsNotNone(soup.find(id="automatic-build-queues"))


class AdvisoryTests(unittest.TestCase):
    def test_pinned_versions_and_markers(self):
        packages = audit.locked_packages([ROOT / "docs/requirements-pdf.lock"])
        self.assertIn(("mkdocs-to-pdf", "0.12.1"), packages)
        self.assertIn(("certifi", "2026.10.19"), packages)
        self.assertEqual([version for name, version in packages if name == "soupsieve"], ["4.1"])
        self.assertNotIn(("uv", "0.12.0"), packages)

    def test_advisory_failures_are_not_ignored(self):
        for results in ([], [{"error": "unavailable"}], [{"next_page_token": "more"}],
                        [{"vulns": [{"id": "TEST-ADVISORY"}]}]):
            response = io.BytesIO(json.dumps({"results": results}).encode())
            with self.subTest(results=results), \
                 patch.object(audit, "locked_packages", return_value=[("test", "1.0")]), \
                 patch.object(audit.urllib.request, "urlopen", return_value=response), \
                 self.assertRaises(ValueError):
                audit.audit([])

    def test_clean_advisory_response(self):
        response = io.BytesIO(b'{"results": [{}]}')
        with patch.object(audit, "locked_packages", return_value=[("test", "1.0")]), \
             patch.object(audit.urllib.request, "urlopen", return_value=response):
            audit.audit([])


if __name__ == "__main__":
    unittest.main()
