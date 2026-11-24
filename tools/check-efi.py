#!/usr/bin/env python3
"""Comprueba que BOOTX64.EFI es una aplicación EFI x86_64 válida.

No usa herramientas externas: lee la cabecera PE32+ y busca dependencias de la
libc de Windows. Se ejecuta como prueba de CTest después de compilar.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

PE32_PLUS_MAGIC = 0x20B
EFI_APPLICATION_SUBSYSTEM = 10
MACHINE_AMD64 = 0x8664
MAX_EFI_BYTES = 2 * 1024 * 1024
MARKER = b"BOOTEABLE-BOOT-OK"


def fail(message: str) -> None:
    print(f"check-efi: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("efi", type=Path)
    args = parser.parse_args()

    if not args.efi.is_file():
        fail(f"no existe {args.efi}")

    data = args.efi.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        fail("no empieza con la firma MZ de un ejecutable PE")

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset : pe_offset + 4] != b"PE\0\0":
        fail("no tiene la firma PE")

    machine, = struct.unpack_from("<H", data, pe_offset + 4)
    if machine != MACHINE_AMD64:
        fail(f"máquina 0x{machine:04x}, se esperaba x86_64 (0x{MACHINE_AMD64:04x})")

    optional_offset = pe_offset + 24
    magic, = struct.unpack_from("<H", data, optional_offset)
    if magic != PE32_PLUS_MAGIC:
        fail(f"magic 0x{magic:x}, se esperaba PE32+ (0x{PE32_PLUS_MAGIC:x})")

    subsystem, = struct.unpack_from("<H", data, optional_offset + 68)
    if subsystem != EFI_APPLICATION_SUBSYSTEM:
        fail(
            "subsistema "
            f"{subsystem}, se esperaba {EFI_APPLICATION_SUBSYSTEM} (EFI application)"
        )

    number_of_sections, = struct.unpack_from("<H", data, pe_offset + 6)
    optional_size, = struct.unpack_from("<H", data, pe_offset + 20)
    number_of_directories, = struct.unpack_from("<I", data, optional_offset + 108)
    if number_of_directories > 1:
        import_rva, import_size = struct.unpack_from("<II", data, optional_offset + 112 + 8)
        if import_size != 0 and import_rva != 0:
            sections = []
            table = optional_offset + optional_size
            for index in range(number_of_sections):
                base = table + index * 40
                virtual_size, virtual_address, raw_size, raw_pointer = (
                    struct.unpack_from("<IIII", data, base + 8)
                )
                sections.append((virtual_address, virtual_size, raw_size, raw_pointer))

            def rva_to_offset(rva: int) -> int:
                for virtual_address, virtual_size, raw_size, raw_pointer in sections:
                    span = max(virtual_size, raw_size)
                    if virtual_address <= rva < virtual_address + span:
                        offset = raw_pointer + (rva - virtual_address)
                        if offset < len(data):
                            return offset
                fail(f"RVA 0x{rva:x} fuera del archivo")

            for descriptor in range(64):
                offset = rva_to_offset(import_rva + descriptor * 20)
                if offset + 20 > len(data):
                    fail("tabla de importación truncada")
                fields = struct.unpack_from("<IIIII", data, offset)
                if fields == (0, 0, 0, 0, 0):
                    break
                name_rva = fields[3]
                if name_rva != 0:
                    name_offset = rva_to_offset(name_rva)
                    end = data.find(b"\0", name_offset)
                    name = data[name_offset:end].decode("ascii", "replace")
                    fail(f"el binario importa {name}; no debe depender de DLLs")

    lowered = data.lower()
    for forbidden in (b"msvcrt", b"api-ms-win-", b"kernel32"):
        if forbidden in lowered:
            fail(f"referencia prohibida a {forbidden.decode('ascii')}")

    if MARKER not in data:
        fail(f"no contiene el marcador {MARKER.decode('ascii')}")

    if len(data) > MAX_EFI_BYTES:
        fail(f"el binario ocupa {len(data)} bytes; es demasiado grande para la ESP")

    print(
        "check-efi: OK · PE32+ · x86_64 · subsistema EFI · "
        f"{len(data)} bytes · sin imports"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
