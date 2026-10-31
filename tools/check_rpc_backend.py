"""Build the pinned P0 backend evaluation and run its no-listener lifecycle probe on macOS."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def Run(command, log_path):
    with log_path.open("w", encoding="utf-8") as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise ValueError(f"command failed ({result.returncode}); see {log_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "out/p0-libwebsockets")
    parser.add_argument("--libuv-library", type=Path,
                        default=ROOT / "out/build/test-debug/vendors/libuv/libuv.a")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("this evaluation command is macOS-only; other platform checks remain open")
    try:
        profile = json.loads((ROOT / "specs/rpc/backend.json").read_text())
        source = args.source.resolve()
        actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
        if actual != profile["revision"]:
            raise ValueError("backend checkout does not match candidate pin")
        uv_revision = subprocess.check_output(["git", "rev-parse", "HEAD"],
                                              cwd=ROOT / "vendors/libuv", text=True).strip()
        if uv_revision != profile["libuvRevision"] or not args.libuv_library.is_file():
            raise ValueError("build the pinned project libuv using tools/run-tests.sh first")
        build = ROOT / "out/p0-libwebsockets-build"
        flags = dict(profile["cmake"], LWS_LIBUV_INCLUDE_DIRS=str(ROOT / "vendors/libuv/include"),
                     LWS_LIBUV_LIBRARIES=str(args.libuv_library.resolve()))
        Run(["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug"]
            + [f"-D{key}={value}" for key, value in sorted(flags.items())], ROOT / "out/p0-libwebsockets-configure.log")
        Run(["cmake", "--build", str(build), "--parallel", "4"], ROOT / "out/p0-libwebsockets-build.log")
        probe = ROOT / "out/p0-backend-probe"
        Run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
             f"-I{source / 'include'}", f"-I{build}", f"-I{ROOT / 'vendors/libuv/include'}",
             str(ROOT / "tests/rpc/backend_probe.c"), str(build / "lib/libwebsockets.a"),
             str(args.libuv_library.resolve()), "-o", str(probe)], ROOT / "out/p0-backend-link.log")
        subprocess.run([str(probe)], cwd=ROOT, check=True, timeout=10)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Backend evaluation failed: {error}", file=sys.stderr)
        return 1
    print("Native backend evaluation passed; other platforms and live socket limits are NOT verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
