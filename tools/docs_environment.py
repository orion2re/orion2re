"""Create a documentation environment without trusting ensurepip's CA bundle."""

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import urllib.request
import venv


PIP_VERSION = "26.2.2"
PIP_SHA256 = "fd7f4dfa8190e014013b9f08f3e973f4e4c26ffd179a204895533a7254813a70"
PIP_URL = (
    "https://files.pythonhosted.org/packages/3f/06/"
    "3e4420302363fbb71be3c7f35ce042f54bfca330e3e66b3a2e3c4cca579f/"
    f"pip-{PIP_VERSION}-py3-none-any.whl"
)


def verify_pip(data):
    if hashlib.sha256(data).hexdigest() != PIP_SHA256:
        raise ValueError("Bootstrap pip wheel checksum mismatch")


def create_environment(directory, requirements=None, minimum_python=(3, 10)):
    if sys.version_info[:len(minimum_python)] < minimum_python:
        version = ".".join(str(part) for part in minimum_python)
        raise ValueError(f"Documentation environment requires Python {version}+")
    directory = Path(directory).resolve()
    venv.EnvBuilder(with_pip=True).create(directory)
    python = directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    environment = {**os.environ, "PIP_CONFIG_FILE": os.devnull}
    pip = [str(python), "-I", "-m", "pip", "--isolated", "--disable-pip-version-check"]
    # Check the wheel against a reviewed digest before executing it, even if the
    # Python installation's original TLS trust store has not yet been repaired.
    with urllib.request.urlopen(PIP_URL, timeout=60) as response:
        data = response.read(4 * 1024 * 1024)
    verify_pip(data)
    with tempfile.TemporaryDirectory() as temporary:
        wheel = Path(temporary) / f"pip-{PIP_VERSION}-py3-none-any.whl"
        wheel.write_bytes(data)
        subprocess.run(pip + ["install", "--no-index", "--no-deps", str(wheel)],
                       env=environment, check=True)
    if requirements:
        subprocess.run(pip + ["install", "--require-hashes", "--only-binary=:all:",
                              "--index-url", "https://pypi.org/simple",
                              "--requirement", str(Path(requirements).resolve())],
                       env=environment, check=True)
        subprocess.run(pip + ["check"], env=environment, check=True)
    return python


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("requirements", type=Path, nargs="?")
    parser.add_argument("--pdf", action="store_true")
    arguments = parser.parse_args()
    create_environment(arguments.directory, arguments.requirements,
                       (3, 11, 5) if arguments.pdf else (3, 10))


if __name__ == "__main__":
    main()
