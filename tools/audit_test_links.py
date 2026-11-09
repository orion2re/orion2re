"""Relink built Ninja unit tests without section garbage collection on Unix hosts."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


STRIP_OPTIONS = {"-Wl,-dead_strip", "-Wl,--gc-sections"}


def link_commands(commands):
    result = {}
    for line in commands.splitlines():
        arguments = shlex.split(line)
        if "-o" not in arguments:
            continue
        output = arguments[arguments.index("-o") + 1]
        if not re.fullmatch(r"test_[A-Za-z0-9_]+", output):
            continue
        if arguments[:2] != [":", "&&"] or arguments[-2:] != ["&&", ":"]:
            raise ValueError(f"Unsupported Ninja link command for {output}")
        arguments = arguments[2:-2]
        if arguments.count("-o") != 1 or output in result:
            raise ValueError(f"Ambiguous Ninja link command for {output}")
        result[output] = arguments
    if not result:
        raise ValueError("No unit-test link commands found; configure and build the tests first")
    return result


def audit_one(build, output_directory, name, arguments):
    command = [argument for argument in arguments if argument not in STRIP_OPTIONS]
    output = output_directory / name
    command[command.index("-o") + 1] = str(output)
    try:
        process = subprocess.run(
            command, cwd=build, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, timeout=120,
        )
        return {"target": name, "returncode": process.returncode, "output": process.stdout}
    finally:
        output.unlink(missing_ok=True)


def audit(build, commands, jobs):
    # Do not replace the normal test binaries or run any of the audit outputs.
    with tempfile.TemporaryDirectory(prefix="orion-test-links-") as temporary:
        output_directory = Path(temporary)
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = [
                pool.submit(audit_one, build, output_directory, name, arguments)
                for name, arguments in commands.items()
            ]
            results = []
            for future in futures:
                result = future.result()
                results.append(result)
                if result["returncode"]:
                    print(f"FAIL {result['target']}", flush=True)
            return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--target", action="append", help="Audit only this target (repeatable)")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if sys.platform == "win32":
        parser.error("Use this audit on Linux/macOS; native MSVC already rejects incomplete links")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    build = args.build_dir.resolve()
    report = args.report or build / "test-link-audit.json"
    try:
        commands = link_commands(subprocess.check_output(
            ["ninja", "-C", str(build), "-t", "commands", "all"], text=True,
        ))
        if args.target:
            unknown = set(args.target) - commands.keys()
            if unknown:
                parser.error("Unknown test targets: " + ", ".join(sorted(unknown)))
            commands = {name: command for name, command in commands.items() if name in args.target}
        results = audit(build, commands, args.jobs)
        report.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Link audit failed: {error}", file=sys.stderr)
        return 2
    failed = sum(result["returncode"] != 0 for result in results)
    print(f"{len(results) - failed}/{len(results)} linked without dead stripping; report: {report}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
