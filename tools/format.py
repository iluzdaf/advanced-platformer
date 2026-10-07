import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
LLVM_BIN_DIRECTORIES = ("/opt/homebrew/opt/llvm/bin", "/usr/lib/llvm-{major}/bin")

CPP_DIRECTORIES = ("app", "scripting", "src", "tests")
HEADER_DIRECTORIES = ("app", "include", "scripting", "tests")
JSON_DIRECTORIES = ("app", "assets", "tests/fixtures", ".vscode")
JSON_ROOT_FILES = ("CMakePresets.json", ".luarc.json", ".prettierrc")
YAML_ROOT_FILES = (".clang-format", ".clang-tidy", ".clangd", ".gersemirc")
LUA_DIRECTORIES = ("app", "assets", "tests/fixtures")


def is_under(path, directories):
    return any(path.is_relative_to(directory) for directory in directories)


def is_root_file(path, names):
    return len(path.parts) == 1 and path.name in names


def formatter_for(path):
    path = PurePosixPath(path)
    suffix = path.suffix

    if suffix == ".cpp" and is_under(path, CPP_DIRECTORIES):
        return "clang-format"
    if suffix == ".hpp" and is_under(path, HEADER_DIRECTORIES):
        return "clang-format"
    if is_root_file(path, ("CMakeLists.txt",)):
        return "gersemi"
    if suffix == ".cmake" and is_under(path, ("cmake",)):
        return "gersemi"
    if is_root_file(path, JSON_ROOT_FILES):
        return "prettier"
    if suffix == ".json" and is_under(path, JSON_DIRECTORIES):
        return "prettier"
    if is_root_file(path, YAML_ROOT_FILES):
        return "prettier"
    if suffix in (".yml", ".yaml") and is_under(path, (".github",)):
        return "prettier"
    if suffix == ".md" and (len(path.parts) == 1 or is_under(path, ("docs",))):
        return "prettier"
    if suffix == ".py" and is_under(path, ("tools",)):
        return "ruff"
    if suffix == ".lua" and is_under(path, LUA_DIRECTORIES):
        return "stylua"
    return None


def git(*arguments):
    output = subprocess.run(
        ["git", *arguments], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout
    return [entry for entry in output.split("\0") if entry]


def llvm_major():
    workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text()
    return int(re.search(r"^\s*LLVM:\s*(\d+)", workflow, re.MULTILINE).group(1))


def major_version_of(executable):
    reported = subprocess.run(
        [executable, "--version"], capture_output=True, text=True, check=False
    ).stdout
    found = re.search(r"version (\d+)", reported)
    return int(found.group(1)) if found else None


FORMAT = {
    "clang-format": ["-i"],
    "gersemi": ["--in-place"],
    "prettier": ["--write", "--log-level", "warn"],
    "ruff": ["format", "--quiet"],
    "stylua": [],
}

CHECK = {
    "clang-format": ["--dry-run", "--Werror"],
    "gersemi": ["--check"],
    "prettier": ["--check", "--log-level", "warn"],
    "ruff": ["format", "--check", "--quiet"],
    "stylua": ["--check"],
}


def find_clang_format():
    major = llvm_major()
    directories = [entry.format(major=major) for entry in LLVM_BIN_DIRECTORIES]
    search_path = os.pathsep.join([*directories, os.environ.get("PATH", "")])
    for name in (f"clang-format-{major}", "clang-format"):
        found = shutil.which(name, path=search_path)
        if found and major_version_of(found) == major:
            return found, None
    return None, f"no clang-format {major}, which CI uses"


def find(formatter):
    if formatter == "clang-format":
        return find_clang_format()
    found = shutil.which(formatter)
    return (found, None) if found else (None, f"{formatter} is not installed")


def report(message):
    print(f"format: {message}", file=sys.stderr)


def group(paths):
    groups = {}
    for path in paths:
        formatter = formatter_for(path)
        if formatter:
            groups.setdefault(formatter, []).append(path)
    return groups


def tracked():
    return [path for path in git("ls-files", "-z") if (ROOT / path).is_file()]


def staged():
    paths = git("diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR")
    unstaged = set(git("diff", "--name-only", "-z"))
    for path in paths:
        if path in unstaged and formatter_for(path):
            report(f"not formatted, it has unstaged edits: {path}")
    return [path for path in paths if path not in unstaged]


def run(groups, arguments, missing_fails):
    failed = False
    for formatter, paths in groups.items():
        executable, missing = find(formatter)
        if not executable:
            report(f"{missing}, so these are not formatted: {' '.join(paths)}")
            failed = failed or missing_fails
            continue
        command = [executable, *arguments[formatter], *paths]
        if subprocess.run(command, cwd=ROOT, check=False).returncode != 0:
            report(f"{formatter} did not pass")
            failed = True
    return failed


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="change nothing")
    mode.add_argument("--staged", action="store_true", help="format and stage")
    options = parser.parse_args()

    if options.check:
        return 1 if run(group(tracked()), CHECK, missing_fails=True) else 0

    if options.staged:
        groups = group(staged())
        if run(groups, FORMAT, missing_fails=False):
            return 1
        paths = [path for paths in groups.values() for path in paths]
        if paths:
            git("add", "--", *paths)
        return 0

    return 1 if run(group(tracked()), FORMAT, missing_fails=True) else 0


if __name__ == "__main__":
    sys.exit(main())
