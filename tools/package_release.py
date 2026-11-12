#!/usr/bin/env python3
"""Package game builds, including canonical configuration only for releases."""

import argparse
import io
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import zipfile


FILENAMES = {
    "win32": "orion2re-win-win32.zip",
    "win64": "orion2re-win-win64.zip",
    "osx-arm64": "orion2re-osx-arm64.zip",
}
SOURCE_MODS = Path(__file__).resolve().parents[1] / "data" / "mods"
SOURCE_CONFIG = Path(__file__).resolve().parents[1] / "data" / "orion2re.cfg"
CONFIG_PATH = "data/orion2re.cfg"
APP = "Orion2 Redone.app"
APP_LINK = f"{APP}/Contents/MacOS/orion2re"
LINK_TARGET = "../../../orion2re"
UNWANTED_SUFFIXES = {
    ".pdb", ".obj", ".o", ".log", ".tmp", ".pyc",
    ".c", ".cpp", ".h", ".hpp", ".a", ".lib",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def executable_name(platform):
    return "orion2re" if platform == "osx-arm64" else "orion2re.exe"


def archive_filename(platform, version=None):
    name = FILENAMES[platform]
    version = "main" if version is None else version
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", version)
            and ".." not in version, "Unsafe release version")
    return f"{name[:-4]}-{version}.zip"


def check_architecture(data, platform):
    if platform == "osx-arm64":
        require(len(data) >= 32, "Truncated Mach-O header")
        magic, cpu, _, filetype = struct.unpack_from("<IIII", data)
        require(
            (magic, cpu, filetype) == (0xFEEDFACF, 0x0100000C, 2),
            "Expected a native ARM64 Mach-O executable",
        )
    else:
        require(len(data) >= 64 and data[:2] == b"MZ", "Missing DOS/PE header")
        offset = struct.unpack_from("<I", data, 0x3C)[0]
        require(offset >= 64 and offset + 26 <= len(data), "Truncated PE header")
        require(data[offset:offset + 4] == b"PE\0\0", "Missing PE signature")
        machine = struct.unpack_from("<H", data, offset + 4)[0]
        optional_size, flags, magic = struct.unpack_from("<HHH", data, offset + 20)
        expected = (0x14C, 0x10B) if platform == "win32" else (0x8664, 0x20B)
        require((machine, magic) == expected, f"Wrong PE architecture for {platform}")
        require(flags & 2 and not flags & 0x2000, "PE is not a game executable")
        require(optional_size >= 2 and offset + 24 + optional_size <= len(data),
                "Truncated PE optional header")


def safe_name(name, allow_config=False):
    path = PurePosixPath(name)
    require(name and not path.is_absolute() and ".." not in path.parts
            and "\\" not in name and ":" not in name and "\0" not in name,
            f"Unsafe archive path: {name!r}")
    require((allow_config and name == CONFIG_PATH)
            or not any(part.lower() in {"orion2re.cfg", "._orion2re.cfg"}
                       for part in path.parts), f"Player configuration included: {name}")
    require(path.suffix.lower() not in UNWANTED_SUFFIXES,
            f"Non-distributable file: {name}")
    return path.as_posix()


def check_zip(archive, allow_app_link=False, allow_config=False):
    require(archive.infolist(), "Empty ZIP archive")
    seen = set()
    for entry in archive.infolist():
        name = safe_name(entry.filename, allow_config=allow_config)
        require(name.casefold() not in seen, f"Duplicate archive path: {name}")
        seen.add(name.casefold())
        mode = entry.external_attr >> 16
        kind = stat.S_IFMT(mode)
        require(kind in (0, stat.S_IFREG, stat.S_IFDIR, stat.S_IFLNK),
                f"Unsupported archive entry: {name}")
        if stat.S_ISLNK(mode):
            require(allow_app_link and name == APP_LINK
                    and archive.read(entry) == LINK_TARGET.encode(),
                    f"Unexpected symlink: {name}")
    require(archive.testzip() is None, "ZIP CRC validation failed")


def supported_mods(source_mods):
    require(source_mods.is_dir(), f"Missing source mod directory: {source_mods}")
    return {f"data/mods/{path.name}.zip" for path in source_mods.iterdir()
            if path.is_dir()}


def extract_verified(archive_path, destination, platform):
    if platform == "osx-arm64" and sys.platform == "darwin":
        subprocess.run(["ditto", "-x", "-k", str(archive_path), str(destination)],
                       check=True)
        return
    # zipfile does not restore Unix modes or symlinks. Only validated entries
    # reach this fallback, used for Windows and portable regression fixtures.
    with zipfile.ZipFile(archive_path) as archive:
        for entry in archive.infolist():
            target = destination / entry.filename
            mode = entry.external_attr >> 16
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                if stat.S_ISLNK(mode):
                    target.symlink_to(archive.read(entry).decode())
                else:
                    target.write_bytes(archive.read(entry))
                    if mode:
                        target.chmod(stat.S_IMODE(mode))


def verify_archive(archive_path, platform, source_mods=SOURCE_MODS, staged=None,
                   version=None):
    require(archive_path.name == archive_filename(platform, version),
            "Unexpected output filename")
    exe = executable_name(platform)
    release = version not in (None, "main")
    with zipfile.ZipFile(archive_path) as archive:
        check_zip(archive, allow_app_link=platform == "osx-arm64", allow_config=release)
        names = set(archive.namelist())
        required = {exe, "data/base.zip"} | supported_mods(source_mods)
        if release:
            required.add(CONFIG_PATH)
        require(required <= names, f"Missing runtime assets: {sorted(required - names)}")
        if release:
            require(archive.read(CONFIG_PATH) == SOURCE_CONFIG.read_bytes(),
                    "Release configuration differs from canonical source")
        for entry in archive.infolist():
            path = PurePosixPath(entry.filename)
            allowed_roots = {exe, "data"}
            if platform == "osx-arm64":
                allowed_roots.update({APP, "__MACOSX"})
            require(path.parts and path.parts[0] in allowed_roots,
                    f"Unexpected distribution entry: {entry.filename}")
            if not entry.is_dir() and path.suffix.lower() == ".zip":
                with zipfile.ZipFile(io.BytesIO(archive.read(entry))) as runtime:
                    check_zip(runtime)
        check_architecture(archive.read(exe), platform)
        if staged is not None:
            for path in staged.rglob("*"):
                if path.is_file() and not path.is_symlink():
                    name = path.relative_to(staged).as_posix()
                    require(archive.read(name) == path.read_bytes(),
                            f"Staged file changed or missing: {name}")
        if platform == "osx-arm64":
            require(archive.getinfo(exe).external_attr >> 16 & 0o111,
                    "macOS executable has no executable permission")
            require(APP_LINK in names, "Missing app executable symlink")
            require(stat.S_ISLNK(archive.getinfo(APP_LINK).external_attr >> 16)
                    and archive.read(APP_LINK) == LINK_TARGET.encode(),
                    "App executable must remain a relative symlink")
            info = plistlib.loads(archive.read(f"{APP}/Contents/Info.plist"))
            require(info.get("CFBundleExecutable") == "orion2re", "Invalid app metadata")
    with tempfile.TemporaryDirectory(prefix="orion2re-extract-") as temporary:
        destination = Path(temporary)
        config = destination / "data" / "orion2re.cfg"
        config.parent.mkdir()
        sentinel = b"player configuration\x00\xff\r\n"
        config.write_bytes(sentinel)
        extract_verified(archive_path, destination, platform)
        expected_config = SOURCE_CONFIG.read_bytes() if release else sentinel
        require(config.read_bytes() == expected_config,
                "Extracted configuration differs from expected configuration")
        if platform == "osx-arm64":
            link = destination / APP_LINK
            require(link.is_symlink() and os.readlink(link) == LINK_TARGET,
                    "Extraction did not preserve the bundle symlink")
            require(link.resolve(strict=True) == (destination / exe).resolve(strict=True),
                    "App symlink does not resolve to the packaged executable")
            require((destination / exe).stat().st_mode & 0o111,
                    "Extraction lost executable permissions")


def stage_distribution(platform, build_dir, stage, version=None):
    exe = executable_name(platform)
    source_exe = build_dir / (exe if platform == "osx-arm64" else f"Release/{exe}")
    require(source_exe.is_file() and not source_exe.is_symlink(),
            f"Missing executable: {source_exe}")
    check_architecture(source_exe.read_bytes(), platform)
    data = build_dir / "data"
    require(data.is_dir() and not data.is_symlink(), f"Missing runtime data: {data}")
    (stage / "data").mkdir()
    for source in sorted(data.rglob("*")):
        relative = source.relative_to(data)
        if any(part.lower() == "orion2re.cfg" for part in relative.parts):
            continue
        safe_name(relative.as_posix())
        # Generated mod ZIPs contain source-owned LBX replacements; loose LBX
        # installation data must never be copied from a game/build directory.
        require(source.suffix.lower() != ".lbx", f"Loose installation data: {source}")
        require(not source.is_symlink(), f"Unexpected runtime symlink: {source}")
        destination = stage / "data" / relative
        if source.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        else:
            require(source.is_file(), f"Not a regular runtime file: {source}")
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    if version not in (None, "main"):
        require(SOURCE_CONFIG.is_file() and not SOURCE_CONFIG.is_symlink(),
                f"Missing canonical configuration: {SOURCE_CONFIG}")
        shutil.copy2(SOURCE_CONFIG, stage / CONFIG_PATH)
    if platform == "osx-arm64":
        require(sys.platform == "darwin", "macOS packaging requires Apple's ditto")
        app = build_dir / APP
        require(app.is_dir() and not app.is_symlink(), f"Missing app bundle: {app}")
        for source in (source_exe, app):
            subprocess.run(["ditto", str(source), str(stage / source.name)], check=True)
    else:
        shutil.copy2(source_exe, stage / exe)


def package(platform, build_dir, output_dir, source_mods=SOURCE_MODS, version=None):
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = archive_filename(platform, version)
    result = output_dir.resolve() / filename
    require(not result.exists() and not result.is_symlink(),
            f"Refusing to replace existing package: {result}")
    with tempfile.TemporaryDirectory(prefix="orion2re-package-") as temporary:
        temporary = Path(temporary)
        stage = temporary / "stage"
        stage.mkdir()
        stage_distribution(platform, build_dir.resolve(), stage, version=version)
        archive_path = temporary / filename
        if platform == "osx-arm64":
            subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc",
                            str(stage), str(archive_path)], check=True)
        else:
            with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
                for path in sorted(stage.rglob("*")):
                    archive.write(path, path.relative_to(stage).as_posix())
        verify_archive(archive_path, platform, source_mods, staged=stage, version=version)
        # Exclusive creation also rejects a collision introduced during assembly.
        with result.open("xb") as destination, archive_path.open("rb") as source:
            shutil.copyfileobj(source, destination)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=FILENAMES)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--version", default="main",
                        help="ZIP version suffix (default: main; release tags include canonical config)")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    try:
        if args.verify_only:
            archive = args.output_dir / archive_filename(args.platform, args.version)
            verify_archive(archive, args.platform, version=args.version)
        else:
            archive = package(args.platform, args.build_dir, args.output_dir,
                              version=args.version)
        print(f"Verified {archive}")
    except (OSError, ValueError, KeyError, struct.error, zipfile.BadZipFile,
            RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()