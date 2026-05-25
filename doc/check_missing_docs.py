#!/usr/bin/env python3
"""Check that all public functions in blosc2.h and b2nd.h appear in the docs.

Compares BLOSC_EXPORT / static inline declarations in the public headers
against doxygenfunction directives in doc/reference/*.rst.

Exit 0 if all are covered, 1 with a report otherwise.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADERS = [ROOT / "include" / "blosc2.h", ROOT / "include" / "b2nd.h"]
RST_DIR = ROOT / "doc" / "reference"

# Functions that are deliberately excluded from the reference docs
INTENTIONAL_SKIP = {
    # Context-free convenience wrappers; _ctx variants are documented
    "blosc2_compress",
    "blosc2_decompress",
    "blosc2_getitem",
    "blosc2_cbuffer_sizes",
    # Low-level inline helper
    "swap_store",
}


def parse_header_functions(path: Path) -> set[str]:
    """Return a set of public function names declared in the header."""
    text = path.read_text()
    funcs: set[str] = set()
    for m in re.finditer(
        r"(?:BLOSC_EXPORT|static inline)\s+(?:const\s+)?\w+\s*\*?\s+(\w+)\s*\(",
        text,
    ):
        funcs.add(m.group(1))
    return funcs


def parse_rst_functions(rst_dir: Path) -> set[str]:
    """Return a set of function names referenced in the RST files."""
    funcs: set[str] = set()
    for rst in sorted(rst_dir.glob("*.rst")):
        for line in rst.read_text().splitlines():
            m = re.match(r"\.\.\s+doxygenfunction::\s+(\w+)", line)
            if m:
                funcs.add(m.group(1))
    return funcs


def main() -> int:
    header_funcs: set[str] = set()
    for h in HEADERS:
        header_funcs |= parse_header_functions(h)

    rst_funcs = parse_rst_functions(RST_DIR)

    missing = sorted(header_funcs - rst_funcs - INTENTIONAL_SKIP)

    if missing:
        print("Public functions missing from the reference docs:")
        for name in missing:
            print(f"  {name}")
        print("\nAdd them to the appropriate .rst file under doc/reference/.")
        print(
            "If a function is intentionally omitted, add it to INTENTIONAL_SKIP"
            " in doc/check_missing_docs.py."
        )
        return 1

    print("All public functions are documented.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
