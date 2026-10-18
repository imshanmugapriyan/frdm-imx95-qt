"""Create a verified source ZIP for the separate, approved Qt build environment.

--ubuntu normalizes portable text to LF and marks shell helpers executable.
No compiler, Qt installation or third-party Python package is required.
"""
import argparse
import hashlib
from pathlib import Path
import stat
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
from check_structure import check_structure

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "FRDM_IMX95_QT_DSW/"
TEXT_SUFFIXES = {".cmake", ".cpp", ".h", ".qml", ".js", ".cjs", ".json",
                 ".py", ".sh", ".md", ".txt", ".svg"}
ROOT_FILES = (".gitignore", ".gitattributes", "CMakeLists.txt", "README.md", "README_UBUNTU.md")


def payload(path, ubuntu):
    data = path.read_bytes()
    if ubuntu and (path.suffix in TEXT_SUFFIXES or path.name in ROOT_FILES):
        # Deliberately leave binary artwork and Windows .cmd helpers unchanged.
        data = data.decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")
    return data


def archive_entry(relative):
    entry = ZipInfo(PREFIX + relative)
    entry.create_system = 3  # Store POSIX modes even when packaging on Windows.
    mode = 0o755 if relative.endswith(".sh") else 0o644
    entry.external_attr = (stat.S_IFREG | mode) << 16
    entry.compress_type = ZIP_DEFLATED
    return entry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ubuntu", action="store_true", help="Produce the Ubuntu/Linux source transfer ZIP")
    args = parser.parse_args()
    check_structure()
    files = [ROOT / name for name in ROOT_FILES]
    for folder in ("cmake", "src", "tests", "docs", "scripts", "importedcontent"):
        files.extend(sorted(path for path in (ROOT / folder).rglob("*")
                            if path.is_file() and "__pycache__" not in path.parts
                            and path.suffix != ".pyc"))
    contents = {}
    for path in files:
        if path.is_symlink() or not path.resolve().is_relative_to(ROOT.resolve()):
            raise ValueError(f"Source package must not follow external links: {path}")
        contents[path.relative_to(ROOT).as_posix()] = payload(path, args.ubuntu)
    # The hashes describe extracted bytes, including LF conversion in Ubuntu mode.
    contents["SOURCE_SHA256SUMS"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n"
        for name, data in sorted(contents.items())
    ).encode("utf-8")
    name = "FRDM_IMX95_QT_DSW-ubuntu-source.zip" if args.ubuntu else "FRDM_IMX95_QT_DSW-source.zip"
    archive = ROOT / "build/transfer" / name
    archive.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(archive, "w", compression=ZIP_DEFLATED, compresslevel=9) as zipped:
        for relative, data in sorted(contents.items()):
            zipped.writestr(archive_entry(relative), data, compresslevel=9)
    with ZipFile(archive) as zipped:
        if zipped.testzip() is not None or len(zipped.namelist()) != len(contents):
            raise RuntimeError("Source ZIP integrity check failed")
        for relative, data in contents.items():
            if zipped.read(PREFIX + relative) != data:
                raise RuntimeError(f"Source ZIP differs from {relative}")
            if zipped.getinfo(PREFIX + relative).external_attr != archive_entry(relative).external_attr:
                raise RuntimeError(f"Incorrect Unix permissions: {relative}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    checksum = archive.with_suffix(archive.suffix + ".sha256")
    checksum.write_bytes(f"{digest}  {archive.name}\n".encode("ascii"))
    print(f"Verified {len(files)} source files plus SHA-256 manifest in {archive} ({archive.stat().st_size:,} bytes).")
    print(f"SHA256: {digest}\nChecksum file: {checksum}")


if __name__ == "__main__":
    main()
