#!/usr/bin/env python3
"""Incrusta la escena y el tema de la UI como texto C++.

Se ejecuta en tiempo de compilación, igual que embed-efi.py. El binario no
depende de rutas de assets en runtime y las pruebas no necesitan un directorio
de trabajo concreto.
"""

from __future__ import annotations

import argparse
from pathlib import Path

DELIMITER = "BUI"

HEADER = """\
// Generado por tools/embed-ui.py. No editar a mano.
#ifndef BOOTEABLE_GENERATED_EMBEDDEDUIASSETS_HPP
#define BOOTEABLE_GENERATED_EMBEDDEDUIASSETS_HPP

namespace booteable::ui::assets {

extern const char kSceneText[];
extern const char kThemeText[];

} // namespace booteable::ui::assets

#endif // BOOTEABLE_GENERATED_EMBEDDEDUIASSETS_HPP
"""


def raw_literal(text: str) -> str:
    if f"){DELIMITER}\"" in text:
        raise SystemExit(
            f"El asset contiene el delimitador raw {DELIMITER!r}; elegí otro.")
    return f'R"{DELIMITER}({text}){DELIMITER}"'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", required=True, type=Path)
    parser.add_argument("--theme", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    scene = args.scene.read_text(encoding="utf-8")
    theme = args.theme.read_text(encoding="utf-8")

    if not scene.strip():
        raise SystemExit(f"La escena está vacía: {args.scene}")
    if not theme.strip():
        raise SystemExit(f"El tema está vacío: {args.theme}")

    args.output_dir.mkdir(parents=True, exist_ok=True)

    header_path = args.output_dir / "EmbeddedUiAssets.hpp"
    source_path = args.output_dir / "EmbeddedUiAssets.cpp"

    header_path.write_text(HEADER, encoding="utf-8")

    source = f"""\
// Generado por tools/embed-ui.py. No editar a mano.
#include "EmbeddedUiAssets.hpp"

namespace booteable::ui::assets {{

extern const char kSceneText[] = {raw_literal(scene)};

extern const char kThemeText[] = {raw_literal(theme)};

}} // namespace booteable::ui::assets
"""
    source_path.write_text(source, encoding="utf-8")

    print(
        f"Incrustados {args.scene.name} ({len(scene)} bytes) y "
        f"{args.theme.name} ({len(theme)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
