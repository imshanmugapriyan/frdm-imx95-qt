"""Check source packaging and Linux path casing without importing Qt."""
from pathlib import Path
import os
import re

ROOT = Path(__file__).resolve().parents[1]


def check_exact_path(path):
    # Normalize '..' without resolving the on-disk spelling on Windows.
    relative = Path(os.path.abspath(path)).relative_to(ROOT)
    current = ROOT
    for part in relative.parts:
        if part not in {child.name for child in current.iterdir()}:
            raise ValueError(f"Missing path or incorrect Linux casing: {path}")
        current /= part


def check_structure():
    sources = []
    visited = set()

    def collect(manifest):
        check_exact_path(manifest)
        if manifest in visited:
            raise ValueError(f"Manifest is included twice: {manifest}")
        visited.add(manifest)
        text = manifest.read_text(encoding="utf-8-sig")
        text = re.sub(r"#[^\n]*", "", text)
        for body in re.findall(r"list\(APPEND\s+FRDM_(?:APP_SOURCES|QML_FILES|NATIVE_SOURCES|RESOURCE_FILES)\s+([^)]*)\)", text):
            for quoted, plain in re.findall(r'"([^"]+)"|(\S+)', body):
                source = quoted or plain
                if "$" in source or ";" in source:
                    raise ValueError(f"Use explicit repository-relative source paths: {manifest}: {source}")
                sources.append(source)
        for reference in re.findall(r'include\("([^"]+)"\)', text):
            reference = reference.replace("${FRDM_SOURCE_ROOT}", ROOT.as_posix())
            reference = reference.replace("${CMAKE_CURRENT_LIST_DIR}", manifest.parent.as_posix())
            if "$" in reference:
                raise ValueError(f"Unsupported source-manifest include: {reference}")
            collect(Path(reference))

    collect(ROOT / "cmake/ProjectSources.cmake")
    if len(sources) != len(set(sources)):
        raise ValueError("A source is listed in multiple manifests")
    for relative in sources:
        check_exact_path(ROOT / relative)
    qml_types = set()
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in (".qml", ".js", ".cpp", ".h"):
            continue
        relative = path.relative_to(ROOT).as_posix()
        if relative != "src/shell/qml/boot/Preview.qml" and relative not in sources:
            raise ValueError(f"Source is not in a build manifest: {relative}")
        text = path.read_text(encoding="utf-8-sig")
        for reference in re.findall(r'^\.?import\s+"([^"]+)"', text, re.M):
            check_exact_path(path.parent / reference)
        if path.suffix == ".qml":
            if path.stem in qml_types:
                raise ValueError(f"Duplicate QML type: {path.stem}")
            qml_types.add(path.stem)
            # Delimiter sanity only. Qt compilation is a separate required check.
            stripped = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/', "", text)
            stack = []
            closing = {"(": ")", "[": "]", "{": "}"}
            for char in stripped:
                if char in closing:
                    stack.append(closing[char])
                elif char in ")]}":
                    if not stack or stack.pop() != char:
                        raise ValueError(f"Unbalanced delimiters: {relative}")
            if stack:
                raise ValueError(f"Unclosed delimiters: {relative}")
    registry = ROOT / "src/tools/ToolRegistry.qml"
    for entry in re.findall(r'entryPoint:\s*"([^"]+)"', registry.read_text(encoding="utf-8-sig")):
        path = registry.parent / entry
        check_exact_path(path)
        if path.relative_to(ROOT).as_posix() not in sources:
            raise ValueError(f"Tool entry point is not packaged: {entry}")
        entry_source = path.read_text(encoding="utf-8-sig")
        if not re.search(r"required\s+property\s+var\s+session\b", entry_source) or not re.search(r"property\s+real\s+uiScale\b", entry_source):
            raise ValueError(f"Tool entry point must expose session and uiScale: {entry}")
    print(f"Structure checks passed: {len(sources)} source/resource files, {len(qml_types)} QML types; Linux path casing verified.")


if __name__ == "__main__":
    check_structure()
