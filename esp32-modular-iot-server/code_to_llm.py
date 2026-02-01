from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Union


# -----------------------------
# Types
# -----------------------------
Tree = Dict[str, Union["Tree", Path]]


# -----------------------------
# Config
# -----------------------------
@dataclass(frozen=True)
class ScanConfig:
    root: Path
    include_file: Callable[[Path], bool]
    exclude_dir: Callable[[Path], bool] = lambda _p: False  # exclude if True
    follow_symlinks: bool = False


def default_match_files(p: Path) -> bool:
    """Default: only .h and .cpp files."""
    return p.is_file() and p.suffix.lower() in {".h", ".cpp"}


def make_exclude_dir_by_name(names: Iterable[str]) -> Callable[[Path], bool]:
    """
    Returns a predicate that excludes directories whose name matches any of `names`.
    Example: {'.git', '.pio', 'build', 'dist'}
    """
    name_set = {n.lower() for n in names}

    def _exclude(p: Path) -> bool:
        return p.is_dir() and p.name.lower() in name_set

    return _exclude


# -----------------------------
# Tree construction
# -----------------------------
def ensure_node(tree: Tree, rel_parts: Iterable[str]) -> Tree:
    """Ensure nested dict structure exists and return the node for rel_parts."""
    node: Tree = tree
    for part in rel_parts:
        node = node.setdefault(part, {})  # type: ignore[assignment]
    return node


def build_tree(cfg: ScanConfig) -> Tree:
    """
    Walk cfg.root and build a nested dict mirroring the directory structure.
    Directories -> dict
    Files -> Path
    """
    root = cfg.root.resolve()
    tree: Tree = {}

    for dirpath, dirnames, filenames in os.walk(root, followlinks=cfg.follow_symlinks):
        dirpath_p = Path(dirpath)

        # Prune excluded directories *in-place* so os.walk won't descend into them.
        kept: List[str] = []
        for d in dirnames:
            dp = dirpath_p / d
            if not cfg.exclude_dir(dp):
                kept.append(d)
        dirnames[:] = kept

        # Find node for current directory
        rel_parts = dirpath_p.relative_to(root).parts
        node = ensure_node(tree, rel_parts)

        # Add matching files
        for name in filenames:
            fp = (dirpath_p / name).resolve()
            if cfg.include_file(fp):
                node[name] = fp

    return tree


def iter_files(tree: Tree) -> List[Path]:
    """Collect all files (Path values) from the nested tree."""
    out: List[Path] = []

    def _walk(node: Tree) -> None:
        for v in node.values():
            if isinstance(v, dict):
                _walk(v)
            else:
                out.append(v)

    _walk(tree)
    return out


# -----------------------------
# Formatting / output
# -----------------------------
def read_text_safely(path: Path, encodings: Iterable[str] = ("utf-8",)) -> str:
    """
    Read text safely with fallback:
    - Try given encodings (default: utf-8)
    - Fall back to system default
    - Finally, replace undecodable characters
    """
    for enc in encodings:
        try:
            return path.read_text(encoding=enc)
        except UnicodeDecodeError:
            pass
        except Exception:
            break

    try:
        return path.read_text()
    except Exception:
        return path.read_text(encoding="utf-8", errors="replace")


def format_blocks(files: Iterable[Path], base: Path, sep_len: int = 24) -> str:
    """
    Create blocks like:
      path/to/file.cpp
      ------------------------
      [file contents]
    """
    base = base.resolve()
    sep = "-" * sep_len

    # Stable ordering
    files_sorted = sorted(files, key=lambda p: str(p.resolve().relative_to(base)))

    blocks: List[str] = []
    for p in files_sorted:
        rel = p.resolve().relative_to(base)
        header = str(rel).replace("\\", "/")
        contents = read_text_safely(p)
        blocks.append(f"{header}\n{sep}\n{contents}")

    return "\n\n".join(blocks)


# -----------------------------
# High-level runner
# -----------------------------
def scan_and_print(
    main_folder: str | Path,
    *,
    include_file: Callable[[Path], bool] = default_match_files,
    exclude_dir: Optional[Callable[[Path], bool]] = None,
    exclude_dir_names: Optional[Iterable[str]] = None,
    follow_symlinks: bool = False,
) -> None:
    """
    High-level helper:
    - main_folder: root directory to scan
    - include_file: predicate for including files
    - exclude_dir: predicate for excluding directories
    - exclude_dir_names: convenience list of folder names to exclude (merged with exclude_dir)
    """
    root = Path(main_folder).expanduser().resolve()
    if not root.is_dir():
        raise ValueError(f"'{root}' is not a valid directory")

    # Build a combined exclude predicate (easy to extend)
    preds: List[Callable[[Path], bool]] = []
    if exclude_dir is not None:
        preds.append(exclude_dir)
    if exclude_dir_names is not None:
        preds.append(make_exclude_dir_by_name(exclude_dir_names))

    def combined_exclude(p: Path) -> bool:
        return any(pred(p) for pred in preds)

    cfg = ScanConfig(
        root=root,
        include_file=include_file,
        exclude_dir=combined_exclude if preds else (lambda _p: False),
        follow_symlinks=follow_symlinks,
    )

    tree = build_tree(cfg)
    files = iter_files(tree)
    print(format_blocks(files, base=root))


# -----------------------------
# CLI
# -----------------------------
if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} <main_folder>")
        raise SystemExit(2)

    scan_and_print(
        sys.argv[1],
        exclude_dir_names={".git", ".pio", "build", "dist", ".vscode"},
    )
