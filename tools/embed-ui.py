#!/usr/bin/env python3
"""Incrusta las escenas y el tema de la UI como texto C++.

El formato .gscene admite un solo nodo raíz por archivo y no tiene include ni
composición declarativa. Por eso la UI se parte en varias escenas que AppScene
injerta bajo slots comunes; este script las incrusta todas con un tema único.

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

#include <cstddef>

namespace booteable::ui::assets {

struct EmbeddedScene {
    const char* name;
    const char* source;
    const char* text;
};

extern const EmbeddedScene kScenes[];
extern const std::size_t kSceneCount;
extern const char kThemeText[];

} // namespace booteable::ui::assets

#endif // BOOTEABLE_GENERATED_EMBEDDEDUIASSETS_HPP
"""


def raw_literal(text: str) -> str:
    if f"){DELIMITER}\"" in text:
        raise SystemExit(
            f"El asset contiene el delimitador raw {DELIMITER!r}; elegí otro.")
    return f'R"{DELIMITER}({text}){DELIMITER}"'


def cpp_string(text: str) -> str:
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def parse_scene_arg(value: str) -> tuple[str, Path]:
    name, separator, path = value.partition("=")
    if not separator or not name or not path:
        raise SystemExit(
            f"--scene espera nombre=ruta; recibido: {value!r}")
    return name, Path(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scene",
        required=True,
        action="append",
        metavar="NOMBRE=RUTA",
        help="Escena a incrustar; se puede repetir.",
    )
    parser.add_argument("--theme", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    scenes = [parse_scene_arg(value) for value in args.scene]
    names = [name for name, _ in scenes]
    if len(set(names)) != len(names):
        raise SystemExit("Los nombres de escena deben ser únicos.")

    theme = args.theme.read_text(encoding="utf-8")
    if not theme.strip():
        raise SystemExit(f"El tema está vacío: {args.theme}")

    loaded: list[tuple[str, Path, str]] = []
    for name, path in scenes:
        text = path.read_text(encoding="utf-8")
        if not text.strip():
            raise SystemExit(f"La escena está vacía: {path}")
        loaded.append((name, path, text))

    args.output_dir.mkdir(parents=True, exist_ok=True)

    header_path = args.output_dir / "EmbeddedUiAssets.hpp"
    source_path = args.output_dir / "EmbeddedUiAssets.cpp"

    header_path.write_text(HEADER, encoding="utf-8")

    entries = []
    for name, path, text in loaded:
        entries.append(
            f"    {{{cpp_string(name)}, {cpp_string(path.name)}, "
            f"{raw_literal(text)}}},\n")
    entries.append("")

    source = f"""\
// Generado por tools/embed-ui.py. No editar a mano.
#include "EmbeddedUiAssets.hpp"

namespace booteable::ui::assets {{

extern const EmbeddedScene kScenes[] = {{
{''.join(entries)}}};

extern const std::size_t kSceneCount =
    sizeof(kScenes) / sizeof(kScenes[0]);

extern const char kThemeText[] = {raw_literal(theme)};

}} // namespace booteable::ui::assets
"""
    source_path.write_text(source, encoding="utf-8")

    for name, path, text in loaded:
        print(f"Incrustada {path.name} como {name!r} ({len(text)} bytes)")
    print(f"Incrustado {args.theme.name} ({len(theme)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
