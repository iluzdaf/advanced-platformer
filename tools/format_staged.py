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
JSON_DIRECTORIES = ("assets", "tests/fixtures", ".vscode")
JSON_ROOT_FILES = ("CMakePresets.json", ".luarc.json", ".prettierrc")
YAML_ROOT_FILES = (".clang-format", ".clang-tidy", ".clangd", ".gersemirc")
LUA_DIRECTORIES = ("assets", "tests/fixtures")


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


def find_clang_format():
    major = llvm_major()
    directories = [entry.format(major=major) for entry in LLVM_BIN_DIRECTORIES]
    search_path = os.pathsep.join([*directories, os.environ.get("PATH", "")])
    for name in (f"clang-format-{major}", "clang-format"):
        found = shutil.which(name, path=search_path)
        if found and major_version_of(found) == major:
            return [found, "-i"], None
    return None, f"no clang-format {major}, which CI uses"


def find_tool(name, *arguments):
    found = shutil.which(name)
    if found:
        return [found, *arguments], None
    return None, f"{name} is not installed"


def commands():
    return {
        "clang-format": find_clang_format(),
        "gersemi": find_tool("gersemi", "--in-place"),
        "prettier": find_tool("prettier", "--write", "--log-level", "warn"),
        "ruff": find_tool("ruff", "format", "--quiet"),
        "stylua": find_tool("stylua"),
    }


def report(message):
    print(f"pre-commit: {message}", file=sys.stderr)


def main():
    staged = git("diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR")
    unstaged = set(git("diff", "--name-only", "-z"))

    groups = {}
    for path in staged:
        formatter = formatter_for(path)
        if not formatter:
            continue
        if path in unstaged:
            report(f"not formatted, it has unstaged edits: {path}")
            continue
        groups.setdefault(formatter, []).append(path)

    if not groups:
        return 0

    available = commands()
    for formatter, paths in groups.items():
        command, missing = available[formatter]
        if not command:
            report(f"{missing}, so these are not formatted: {' '.join(paths)}")
            continue
        if subprocess.run([*command, *paths], cwd=ROOT, check=False).returncode != 0:
            report(f"{formatter} failed, so nothing was committed")
            return 1
        git("add", "--", *paths)

    return 0


if __name__ == "__main__":
    sys.exit(main())
