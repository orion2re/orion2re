"""No game installation or runnable platform binaries are needed for these tests."""

import importlib.util
import io
from pathlib import Path
import plistlib
import stat
import struct
import sys
import tempfile
import unittest
import zipfile


SPEC = importlib.util.spec_from_file_location(
    "package_release", Path(__file__).resolve().parents[2] / "tools/package_release.py"
)
packaging = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packaging)


def synthetic_executable(platform):
    if platform == "osx-arm64":
        return struct.pack("<8I", 0xFEEDFACF, 0x0100000C, 0, 2, 0, 0, 0, 0)
    data = bytearray(128)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 64)
    data[64:68] = b"PE\0\0"
    machine, magic = (0x14C, 0x10B) if platform == "win32" else (0x8664, 0x20B)
    struct.pack_into("<H", data, 68, machine)
    struct.pack_into("<HHH", data, 84, 2, 2, magic)
    return bytes(data)


def runtime_zip():
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        archive.writestr("./", b"")
        archive.writestr("./runtime.txt", b"source-owned runtime fixture")
        archive.writestr("./REPLACEMENT.LBX/0.txt", b"source-owned LBX entry")
        archive.writestr("./MOD.LBX", b"source-owned replacement")
    return buffer.getvalue()


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.output = self.root / "output"
        self.mods = self.root / "source-mods"
        for name in ("one", "two"):
            (self.mods / name).mkdir(parents=True)
        (self.build / "Release").mkdir(parents=True)
        (self.build / "data/mods").mkdir(parents=True)
        for name in ("base.zip", "mods/one.zip", "mods/two.zip"):
            (self.build / "data" / name).write_bytes(runtime_zip())
        self.config = self.build / "data/orion2re.cfg"
        self.config.write_bytes(b"untouched build configuration")
        (self.build / "data/extra.dat").write_bytes(b"additional runtime asset")

    def package(self, platform="win32", version=None):
        return packaging.package(platform, self.build, self.output, self.mods,
                                 version=version)

    def windows_exe(self, platform="win32"):
        (self.build / "Release/orion2re.exe").write_bytes(synthetic_executable(platform))

    def test_windows_packages_and_configuration_preservation(self):
        for platform in ("win32", "win64"):
            with self.subTest(platform=platform):
                self.windows_exe(platform)
                archive = self.package(platform)
                self.assertEqual(archive.name, f"orion2re-win-{platform}-main.zip")
                packaging.verify_archive(archive, platform, self.mods)
                with zipfile.ZipFile(archive) as result:
                    self.assertEqual({name.split("/")[0] for name in result.namelist()},
                                     {"orion2re.exe", "data"})
                    self.assertEqual(result.read("orion2re.exe"), synthetic_executable(platform))
                    self.assertEqual(result.read("data/extra.dat"), b"additional runtime asset")
                    self.assertNotIn("data/orion2re.cfg", result.namelist())
                self.assertEqual(self.config.read_bytes(), b"untouched build configuration")

    def test_missing_runtime_assets(self):
        self.windows_exe()
        for name in ("base.zip", "mods/two.zip"):
            path = self.build / "data" / name
            path.unlink()
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.package()
            path.write_bytes(runtime_zip())
        self.assertFalse(list(self.output.glob("*.zip")))

    def test_versioned_release_packages(self):
        for platform in packaging.FILENAMES:
            self.assertEqual(packaging.archive_filename(platform),
                             packaging.FILENAMES[platform][:-4] + "-main.zip")
            self.assertEqual(packaging.archive_filename(platform, "v2.0.0"),
                             packaging.FILENAMES[platform][:-4] + "-v2.0.0.zip")
        for platform in ("win32", "win64"):
            with self.subTest(platform=platform):
                self.windows_exe(platform)
                archive = self.package(platform, version="v2.0.0")
                packaging.verify_archive(archive, platform, self.mods, version="v2.0.0")
                with zipfile.ZipFile(archive) as result:
                    self.assertEqual(result.read(packaging.CONFIG_PATH),
                                     packaging.SOURCE_CONFIG.read_bytes())
                self.assertEqual(self.config.read_bytes(), b"untouched build configuration")
                with self.assertRaises(ValueError):
                    packaging.verify_archive(archive, platform, self.mods)
        for version in ("", "../v2", "/v2", "v2/tag", "v2\\tag", "v2\n", "-v2"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                packaging.archive_filename("win32", version)

    def test_release_requires_canonical_config(self):
        self.windows_exe()
        self.config.unlink()
        archive = self.package(version="v2.0.0")
        with zipfile.ZipFile(archive) as result:
            entries = [(entry, result.read(entry)) for entry in result.infolist()
                       if entry.filename != packaging.CONFIG_PATH]
        for config in (None, b"player configuration"):
            with zipfile.ZipFile(archive, "w") as result:
                for entry, data in entries:
                    result.writestr(entry, data)
                if config is not None:
                    result.writestr(packaging.CONFIG_PATH, config)
            with self.subTest(config=config), self.assertRaises(ValueError):
                packaging.verify_archive(archive, "win32", self.mods, version="v2.0.0")

    def test_release_rejects_config_outside_canonical_path(self):
        self.windows_exe()
        archive = self.package(version="v2.0.0")
        original = archive.read_bytes()
        for name in ("orion2re.cfg", "data/mods/orion2re.cfg",
                     "data/ORION2RE.CFG", "data/._orion2re.cfg"):
            archive.write_bytes(original)
            with zipfile.ZipFile(archive, "a") as result:
                result.writestr(name, packaging.SOURCE_CONFIG.read_bytes())
            with self.subTest(name=name), self.assertRaises(ValueError):
                packaging.verify_archive(archive, "win32", self.mods, version="v2.0.0")

    def test_wrong_architecture_and_malformed_runtime_zip(self):
        self.windows_exe("win64")
        with self.assertRaises(ValueError):
            self.package("win32")
        self.windows_exe()
        (self.build / "data/base.zip").write_bytes(b"not a zip")
        with self.assertRaises(zipfile.BadZipFile):
            self.package()

    def test_nested_config_is_excluded_but_config_in_runtime_zip_is_rejected(self):
        self.windows_exe()
        (self.build / "data/mods/orion2re.cfg").write_bytes(b"config")
        archive = self.package()
        with zipfile.ZipFile(archive) as result:
            self.assertFalse(any(n.endswith("orion2re.cfg") for n in result.namelist()))
        archive.unlink()
        with zipfile.ZipFile(self.build / "data/base.zip", "w") as runtime:
            runtime.writestr("orion2re.cfg", "config")
        for version in (None, "main", "v2.0.0"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                self.package(version=version)

    def test_unsafe_runtime_archives(self):
        self.windows_exe()
        for name in ("../escape", "/absolute", "C:/drive", "a\\..\\escape", "debug.pdb"):
            with zipfile.ZipFile(self.build / "data/base.zip", "w") as runtime:
                runtime.writestr(name, "bad")
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.package()

    def test_missing_executable_and_existing_output(self):
        with self.assertRaises(ValueError):
            self.package()
        self.windows_exe()
        archive = self.package()
        before = archive.read_bytes()
        with self.assertRaises(ValueError):
            self.package()
        self.assertEqual(archive.read_bytes(), before)

    def test_loose_installation_data_is_rejected(self):
        self.windows_exe()
        (self.build / "data/ORIGINAL.LBX").write_bytes(b"not for distribution")
        with self.assertRaises(ValueError):
            self.package()

    @unittest.skipIf(sys.platform == "win32", "Creating symlinks requires Unix")
    def test_dangling_output_symlink_is_not_followed(self):
        self.windows_exe()
        self.output.mkdir()
        outside = self.root / "outside"
        (self.output / packaging.archive_filename("win32")).symlink_to(outside)
        with self.assertRaises(ValueError):
            self.package()
        self.assertFalse(outside.exists())

    def test_outer_archive_paths_and_filename(self):
        self.windows_exe()
        archive = self.package()
        original = archive.read_bytes()
        for name in ("../escape", "/absolute", "data/orion2re.cfg", "debug.pdb"):
            archive.write_bytes(original)
            with zipfile.ZipFile(archive, "a") as result:
                result.writestr(name, "bad")
            with self.subTest(name=name), self.assertRaises(ValueError):
                packaging.verify_archive(archive, "win32", self.mods)
        wrong_name = archive.with_name("versioned-package.zip")
        wrong_name.write_bytes(original)
        with self.assertRaises(ValueError):
            packaging.verify_archive(wrong_name, "win32", self.mods)

    def test_architecture_headers(self):
        for platform in packaging.FILENAMES:
            with self.subTest(platform=platform):
                packaging.check_architecture(synthetic_executable(platform), platform)
                with self.assertRaises(ValueError):
                    packaging.check_architecture(b"truncated", platform)
                wrong = "win64" if platform == "win32" else "win32"
                with self.assertRaises(ValueError):
                    packaging.check_architecture(synthetic_executable(wrong), platform)

    def mac_archive(self, link_target=packaging.LINK_TARGET, mode=0o755,
                    link_mode=stat.S_IFLNK | 0o755):
        archive = self.root / packaging.archive_filename("osx-arm64")
        with zipfile.ZipFile(archive, "w") as result:
            info = zipfile.ZipInfo("orion2re")
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | mode) << 16
            result.writestr(info, synthetic_executable("osx-arm64"))
            link = zipfile.ZipInfo(packaging.APP_LINK)
            link.create_system = 3
            link.external_attr = link_mode << 16
            result.writestr(link, link_target)
            result.writestr(f"{packaging.APP}/Contents/Info.plist",
                            plistlib.dumps({"CFBundleExecutable": "orion2re"}))
            for name in ("base.zip", "mods/one.zip", "mods/two.zip"):
                result.write(self.build / "data" / name, f"data/{name}")
        return archive

    @unittest.skipIf(sys.platform == "win32", "Symlink extraction requires Unix")
    def test_mac_archive_symlink_permissions_and_layout(self):
        packaging.verify_archive(self.mac_archive(), "osx-arm64", self.mods)
        for kwargs in ({"mode": 0o644}, {"link_target": "../../../../outside"},
                       {"link_mode": stat.S_IFREG | 0o755}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                packaging.verify_archive(self.mac_archive(**kwargs), "osx-arm64", self.mods)

    @unittest.skipUnless(sys.platform == "darwin", "Native ditto requires macOS")
    def test_native_ditto_packaging(self):
        exe = self.build / "orion2re"
        exe.write_bytes(synthetic_executable("osx-arm64"))
        exe.chmod(0o755)
        link = self.build / packaging.APP_LINK
        link.parent.mkdir(parents=True)
        link.symlink_to(packaging.LINK_TARGET)
        (link.parent.parent / "Info.plist").write_bytes(
            plistlib.dumps({"CFBundleExecutable": "orion2re"}))
        for version in (None, "main", "v2.0.0"):
            with self.subTest(version=version):
                archive = self.package("osx-arm64", version=version)
                packaging.verify_archive(archive, "osx-arm64", self.mods, version=version)
                with zipfile.ZipFile(archive) as result:
                    if version == "v2.0.0":
                        self.assertEqual(result.read(packaging.CONFIG_PATH),
                                         packaging.SOURCE_CONFIG.read_bytes())
                    else:
                        self.assertNotIn(packaging.CONFIG_PATH, result.namelist())
                archive.unlink()


if __name__ == "__main__":
    unittest.main()