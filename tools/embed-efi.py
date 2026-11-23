#!/usr/bin/env python3
"""Genera el arreglo C++ con el binario EFI incrustado y su SHA-256.

Se ejecuta en tiempo de compilación. No participa en la preparación del medio:
el binario resultante queda dentro del preparador.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

HEADER = """\
// Generado por tools/embed-efi.py. No editar a mano.
#ifndef BOOTEABLE_GENERATED_EMBEDDEDBOOTIMAGE_HPP
#define BOOTEABLE_GENERATED_EMBEDDEDBOOTIMAGE_HPP

#include <cstddef>

namespace booteable {

extern const unsigned char kBootEfi[];
extern const std::size_t kBootEfiSize;
extern const char kBootEfiSha256[];
extern const char kBootEfiBuildId[];

} // namespace booteable

#endif // BOOTEABLE_GENERATED_EMBEDDEDBOOTIMAGE_HPP
"""


def byte_lines(data: bytes, width: int = 12) -> str:
    lines = []
    for start in range(0, len(data), width):
        chunk = data[start : start + width]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    data = args.input.read_bytes()
    if not data:
        raise SystemExit(f"El archivo EFI está vacío: {args.input}")

    digest = hashlib.sha256(data).hexdigest()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    header_path = args.output_dir / "EmbeddedBootImage.hpp"
    source_path = args.output_dir / "EmbeddedBootImage.cpp"

    header_path.write_text(HEADER, encoding="utf-8")

    source = f"""\
// Generado por tools/embed-efi.py. No editar a mano.
#include "EmbeddedBootImage.hpp"

namespace booteable {{

extern const unsigned char kBootEfi[] = {{
{byte_lines(data)}
}};

extern const std::size_t kBootEfiSize = {len(data)}ull;

extern const char kBootEfiSha256[] = "{digest}";

extern const char kBootEfiBuildId[] = "{digest[:12]}";

}} // namespace booteable
"""
    source_path.write_text(source, encoding="utf-8")

    print(f"Incrustado {args.input} ({len(data)} bytes, sha256 {digest[:12]})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
