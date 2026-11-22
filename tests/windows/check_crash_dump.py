"""Exercise the Windows crash handler in an isolated child process."""

from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


def check_dump(path):
    data = path.read_bytes()
    if data[:4] != b"MDMP":
        raise ValueError(f"Not a minidump: {path}")
    count, offset = struct.unpack_from("<II", data, 8)
    for stream, size, start in struct.iter_unpack("<III", data[offset:offset + count * 12]):
        if stream == 6 and size >= 12:  # ExceptionStream; code follows thread ID and alignment.
            return struct.unpack_from("<I", data, start + 8)[0] == 0xE0420001
    return False


def main(executable):
    with tempfile.TemporaryDirectory(prefix="orion2re-dump-") as temp:
        run_dir = Path(temp)
        install_dir = run_dir / "Test installation"
        install_dir.mkdir()
        probe = install_dir / executable.name
        shutil.copy2(executable, probe)
        crashes = install_dir / "crashes"

        subprocess.run([str(probe)], cwd=run_dir, check=True, timeout=30)
        if crashes.exists():
            raise ValueError("Normal exit created a crash directory")

        result = subprocess.run([str(probe), "crash"], cwd=run_dir, timeout=30)
        if result.returncode == 0:
            raise ValueError("The crash probe exited successfully")
        dumps = list(crashes.glob("*.dmp"))
        if len(dumps) != 1 or not re.fullmatch(r"[0-9]{4}(?:-[0-9]{2}){5}\.dmp", dumps[0].name):
            raise ValueError(f"Expected one timestamped minidump next to the executable: {dumps}")
        if not check_dump(dumps[0]):
            raise ValueError("Minidump did not record the unhandled exception")
        print(f"Validated Windows minidump: {dumps[0].name}")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
