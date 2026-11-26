#!/usr/bin/env python3
"""Genera docs/diagrama.drawio con UML nativo de draw.io.

Cinco páginas, estilo monocromo:
  1. Componentes (contenedores por mundo, módulos, medio USB y contrato)
  2. Clases     (swimlane con compartimentos, flechas UML nativas)
  3. Secuencia  (umlLifeline)
  4. Estados    (PrepStage y terminales)
  5. Leyenda    (muestras de relaciones y visibilidad)
"""
import os
from xml.sax.saxutils import quoteattr

OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "docs", "diagrama.drawio",
)

PAGE_W, PAGE_H = 1654, 1169

# --- Estilos nativos (los mismos que usa tasks-lens) -------------------------
ST_CLASS = ("swimlane;fontStyle=1;align=center;verticalAlign=top;childLayout=stackLayout;"
            "horizontal=1;startSize=26;horizontalStack=0;resizeParent=1;resizeParentMax=0;"
            "resizeLast=0;collapsible=1;marginBottom=0;whiteSpace=wrap;html=1;")
ST_FIELD = ("text;strokeColor=none;fillColor=none;align=left;verticalAlign=top;"
            "spacingLeft=4;spacingRight=4;overflow=hidden;rotatable=0;"
            "points=[[0,0.5],[1,0.5]];portConstraint=eastwest;whiteSpace=wrap;html=1;")
ST_SEP = ("line;strokeWidth=1;fillColor=none;align=left;verticalAlign=middle;"
          "spacingTop=-1;spacingLeft=3;spacingRight=3;rotatable=0;"
          "labelPosition=right;points=[];portConstraint=eastwest;strokeColor=inherit;")

ST_REAL = "endArrow=block;endFill=0;endSize=12;dashed=1;html=1;rounded=0;"
ST_DEP = "endArrow=open;endSize=12;dashed=1;html=1;rounded=0;"
ST_ASOC = "endArrow=open;endFill=1;endSize=12;html=1;rounded=0;"
ST_COMP = "startArrow=diamondThin;startFill=1;startSize=24;endArrow=none;html=1;rounded=0;"
ST_AGGR = "startArrow=diamondThin;startFill=0;startSize=24;endArrow=none;html=1;rounded=0;"

ST_LIFELINE = ("shape=umlLifeline;perimeter=lifelinePerimeter;whiteSpace=wrap;html=1;"
               "container=1;collapsible=0;recursiveResize=0;outlineConnect=0;"
               "fillColor=#ffffff;strokeColor=#000000;fontColor=#000000;")
ST_MSG = ("endArrow=block;endFill=1;html=1;rounded=0;fontSize=11;"
          "labelBackgroundColor=#ffffff;verticalAlign=bottom;")
ST_RET = ("endArrow=open;endFill=0;dashed=1;html=1;rounded=0;fontSize=11;"
          "labelBackgroundColor=#ffffff;verticalAlign=bottom;")
ST_TEXT = "text;html=1;whiteSpace=wrap;overflow=hidden;rounded=0;"

# --- Estilos de la vista de componentes y de estados ------------------------
ST_TITLE = ("text;html=1;whiteSpace=wrap;overflow=visible;rounded=0;align=left;"
            "verticalAlign=top;fontSize=11;")
ST_PROC = ("swimlane;html=1;startSize=46;collapsible=0;container=1;fontStyle=1;"
           "verticalAlign=top;align=center;fillColor=#f5f5f5;strokeColor=#000000;"
           "strokeWidth=1.2;whiteSpace=wrap;rounded=0;")
ST_COMPONENT = ("shape=component;whiteSpace=wrap;html=1;align=center;verticalAlign=middle;"
                "fillColor=#ffffff;strokeColor=#000000;strokeWidth=1.2;fontSize=11;spacing=4;")
ST_PACKAGE = ("shape=package;whiteSpace=wrap;html=1;align=center;verticalAlign=middle;"
              "fillColor=#ffffff;strokeColor=#000000;strokeWidth=1;fontSize=11;")
ST_NOTE = ("shape=note;whiteSpace=wrap;html=1;size=10;fillColor=#ffffff;"
           "strokeColor=#000000;fontSize=9;align=left;verticalAlign=top;spacing=4;")
ST_ARROW = ("edgeStyle=none;rounded=0;orthogonalLoop=0;html=1;strokeColor=#000000;"
            "strokeWidth=1;endArrow=open;endFill=0;labelBackgroundColor=#ffffff;"
            "fontSize=10;align=center;verticalAlign=middle;")
ST_STATE = ("rounded=1;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;"
            "strokeWidth=1.2;fontSize=11;align=center;verticalAlign=middle;spacing=4;")


def klass(cid, label, members, x, y, w, methods=(), dashed=False):
    """Clase UML nativa: swimlane + compartimentos como celdas hijas."""
    style = ST_CLASS + ("dashed=1;" if dashed else "")
    h = 26 + 26 * len(members)
    if methods:
        h += 8 + 26 * len(methods)
    out = [
        f'<mxCell id="{cid}" parent="1" style="{style}" value={quoteattr(label)} vertex="1">'
        f'<mxGeometry height="{h}" width="{w}" x="{x}" y="{y}" as="geometry"/></mxCell>'
    ]
    row_y = 26
    for i, text in enumerate(members):
        out.append(
            f'<mxCell id="{cid}_r{i}" parent="{cid}" style="{ST_FIELD}" '
            f'value={quoteattr(text)} vertex="1">'
            f'<mxGeometry height="26" width="{w}" y="{row_y}" as="geometry"/></mxCell>'
        )
        row_y += 26
    if methods:
        out.append(
            f'<mxCell id="{cid}_sep" parent="{cid}" style="{ST_SEP}" value="" vertex="1">'
            f'<mxGeometry height="8" width="{w}" y="{row_y}" as="geometry"/></mxCell>'
        )
        row_y += 8
        for i, text in enumerate(methods):
            out.append(
                f'<mxCell id="{cid}_m{i}" parent="{cid}" style="{ST_FIELD}" '
                f'value={quoteattr(text)} vertex="1">'
                f'<mxGeometry height="26" width="{w}" y="{row_y}" as="geometry"/></mxCell>'
            )
            row_y += 26
    return out, h


def box(cid, style, value, x, y, w, h):
    """Celda rectangular simple (componente, contenedor, nota o estado)."""
    return (f'<mxCell id="{cid}" parent="1" style="{style}" value={quoteattr(value)} '
            f'vertex="1"><mxGeometry height="{h}" width="{w}" x="{x}" y="{y}" '
            f'as="geometry"/></mxCell>')


def rel(cid, src, dst, style, value="", exit_xy=None, entry_xy=None, points=None):
    extra = ""
    if exit_xy:
        extra += f"exitX={exit_xy[0]};exitY={exit_xy[1]};exitDx=0;exitDy=0;"
    if entry_xy:
        extra += f"entryX={entry_xy[0]};entryY={entry_xy[1]};entryDx=0;entryDy=0;"
    geom = '<mxGeometry relative="1" as="geometry">'
    if points:
        pts = "".join(f'<mxPoint x="{px}" y="{py}"/>' for px, py in points)
        geom += f'<Array as="points">{pts}</Array>'
    geom += "</mxGeometry>"
    return (f'<mxCell id="{cid}" edge="1" parent="1" source="{src}" target="{dst}" '
            f'style="{style}{extra}" value={quoteattr(value)}>{geom}</mxCell>')


def page(pid, name, cells):
    body = "\n        ".join(cells)
    return (
        f'  <diagram id="{pid}" name={quoteattr(name)}>\n'
        f'    <mxGraphModel dx="1400" dy="900" grid="1" gridSize="10" guides="1" '
        f'tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" '
        f'pageWidth="{PAGE_W}" pageHeight="{PAGE_H}" math="0" shadow="0">\n'
        f'      <root>\n        <mxCell id="0"/>\n        <mxCell id="1" parent="0"/>\n'
        f'        {body}\n      </root>\n    </mxGraphModel>\n  </diagram>'
    )


pages = []

# =============================================================== Página 1
# Componentes: dos mundos (preparador Linux y firmware UEFI) unidos por el
# medio preparado; el contrato de formato se muestra aparte, como en mem-test.
c = []
c.append(box(
    "cmp_title", ST_TITLE,
    "<b><font style='font-size:18px'>Arquitectura de componentes · booteable</font></b><br>"
    "<font style='font-size:10px'>El preparador construye y verifica el medio; el firmware "
    "ejecuta el programa EFI. Dos mundos, un contrato de formato.</font>",
    40, 12, 1120, 50))

c.append(box(
    "cmp_prep", ST_PROC,
    "<b>Preparador · proceso Linux</b><br>"
    "<font style='font-size:10px;color:#555555'>TUI Guayaba · syscalls Linux · "
    "construir no requiere root; escribir sí</font>",
    40, 70, 780, 700))
c.append(box(
    "cmp_fw", ST_PROC,
    "<b>Equipo de prueba · firmware UEFI</b><br>"
    "<font style='font-size:10px;color:#555555'>x86_64 · sin SO · "
    "solo servicios de arranque</font>",
    1050, 70, 550, 700))

# Módulos del preparador
c.append(box(
    "cmp_ui", ST_COMPONENT,
    "<b>ui/</b><br><font style='font-size:9px;color:#666666'>sin lógica de negocio</font><br>"
    "MainScene · ConfirmDialog · ProgressScene<br>ResultScene · UiFormat<br>"
    "Solo incluye app/ y domain/",
    70, 140, 720, 95))
c.append(box(
    "cmp_app", ST_COMPONENT,
    "<b>app/</b><br><font style='font-size:9px;color:#666666'>dueño del ciclo de vida</font><br>"
    "PrepController · PrepRunner · CancellationToken<br>FakePrepController<br>"
    "Orquesta etapas, progreso y cancelación",
    70, 270, 720, 95))
c.append(box(
    "cmp_image", ST_COMPONENT,
    "<b>image/</b><br><font style='font-size:9px;color:#666666'>la imagen se construye, "
    "no se copia</font><br>BootImageBuilder · GptWriter · Fat32Writer<br>"
    "ImageVerifier · ImageLayout",
    70, 405, 325, 105))
c.append(box(
    "cmp_device", ST_COMPONENT,
    "<b>device/</b><br><font style='font-size:9px;color:#666666'>el único que toca "
    "/sys y /dev</font><br>SysfsBlockDeviceSource · SafetyPolicy<br>"
    "MountManager · PosixDeviceWriter",
    435, 405, 315, 105))
c.append(box(
    "cmp_core", ST_COMPONENT,
    "<b>core/</b><br><font style='font-size:9px;color:#666666'>sin Linux ni Guayaba</font><br>"
    "Sha256 · Crc32 · SizeFormat · Bytes · usado por image/ y device/",
    70, 545, 680, 75))
c.append(box(
    "cmp_cli", ST_COMPONENT,
    "<b>cli/ · booteable-image</b><br><font style='font-size:9px;color:#666666'>"
    "entrada headless</font><br>build · verify · flash · compone booteable_core sin UI",
    70, 655, 680, 75))

# Medio preparado: el puente entre los dos mundos
c.append(box(
    "cmp_usb", ST_PACKAGE,
    "<b>Medio USB preparado</b><br><font style='font-size:9px;color:#666666'>"
    "imagen dispersa y determinista</font><br>GPT primaria + respaldo<br>ESP FAT32 sin LFN<br>"
    "<font face='monospace' style='font-size:9px'>\\EFI\\BOOT\\<br>BOOTX64.EFI</font>",
    800, 390, 230, 170))

# Cadena del firmware
c.append(box(
    "cmp_fwinfo", ST_COMPONENT,
    "<b>Firmware UEFI</b><br><font style='font-size:9px;color:#666666'>"
    "OVMF en QEMU o equipo real</font><br>Busca <font face='monospace'>\\EFI\\BOOT\\"
    "BOOTX64.EFI</font> en la ESP",
    1080, 140, 490, 80))
c.append(box(
    "cmp_svc", ST_COMPONENT,
    "<b>EFI_BOOT_SERVICES · EFI_SYSTEM_TABLE</b><br>"
    "<font style='font-size:9px;color:#666666'>protocolos de arranque</font><br>"
    "ConOut · ConIn · Stall · SerialIo<br>RuntimeServices-&gt;ResetSystem",
    1080, 255, 490, 100))
c.append(box(
    "cmp_boot", ST_COMPONENT,
    "<b>boot/ · BOOTX64.EFI</b><br><font style='font-size:9px;color:#666666'>"
    "PE32+ · -nostdlib · sin EDK2</font><br>efi_main · efi.cpp",
    1080, 390, 490, 100))
c.append(box(
    "cmp_out", ST_COMPONENT,
    "<b>Salida visible</b><br><font style='font-size:9px;color:#666666'>"
    "consola de texto y puerto serie</font><br>Mensaje del grupo + "
    "<b>BOOTEABLE-BOOT-OK</b>",
    1080, 525, 490, 90))

# Notas del mundo UEFI
c.append(box(
    "cmp_note_secure", ST_NOTE,
    "<b>Secure Boot</b> · el PE no está firmado: con Secure Boot activo no arranca. "
    "Se documenta y se prueba en un equipo de laboratorio con autorización.",
    1080, 645, 490, 55))
c.append(box(
    "cmp_note_embed", ST_NOTE,
    "<b>Incrustación</b> · el EFI se compila con MinGW-w64 y se enlaza como arreglo "
    "en el preparador; no se copia un archivo externo.",
    1080, 710, 490, 50))

# Contrato compartido (solo formato, no código)
c.append(box(
    "cmp_format", ST_PACKAGE,
    "<b>Formato compartido · contrato del medio (no se comparte código)</b><br>"
    "GPT primaria y de respaldo · ESP FAT32 sin LFN · ruta \\EFI\\BOOT\\BOOTX64.EFI · "
    "sectores 512 / 4096 · etiqueta BOOTEABLE<br>"
    "El preparador usa syscalls de Linux; el arranque usa EFI_BOOT_SERVICES. "
    "Solo coinciden en el formato byte a byte.",
    40, 820, 1560, 95))

# Flujo dentro del preparador
c.append(rel("cmp_e_ui_app", "cmp_ui", "cmp_app", ST_ARROW,
             "config + confirmación", (0.5, 1), (0.5, 0)))
c.append(rel("cmp_e_app_image", "cmp_app", "cmp_image", ST_ARROW,
             "build(spec)", (0.3, 1), (0.5, 0)))
c.append(rel("cmp_e_app_device", "cmp_app", "cmp_device", ST_ARROW,
             "validar · desmontar · escribir", (0.7, 1), (0.5, 0)))
c.append(rel("cmp_e_image_device", "cmp_image", "cmp_device", ST_ARROW + "dashed=1;",
             "manifiesto", (1.0, 0.5), (0.0, 0.5)))
c.append(rel("cmp_e_image_core", "cmp_image", "cmp_core", ST_ARROW,
             "CRC32 · SHA-256", (0.5, 1), (0.25, 0)))
c.append(rel("cmp_e_device_core", "cmp_device", "cmp_core", ST_ARROW,
             "SHA-256", (0.5, 1), (0.75, 0)))
c.append(rel("cmp_e_cli_core", "cmp_cli", "cmp_core", ST_ARROW,
             "mismas piezas, sin UI", (0.5, 0), (0.5, 1)))

# Del preparador al medio y del medio al firmware
c.append(rel("cmp_e_dev_usb", "cmp_device", "cmp_usb", ST_ARROW,
             "write · fsync · relectura", (1.0, 0.5), (0.0, 0.40)))
c.append(rel("cmp_e_usb_boot", "cmp_usb", "cmp_boot", ST_ARROW,
             "", (1.0, 0.30), (0.0, 0.5)))

# Cadena del firmware
c.append(rel("cmp_e_fw_svc", "cmp_fwinfo", "cmp_svc", ST_ARROW,
             "carga", (0.5, 1), (0.5, 0)))
c.append(rel("cmp_e_svc_boot", "cmp_svc", "cmp_boot", ST_ARROW,
             "invoca efi_main", (0.5, 1), (0.5, 0)))
c.append(rel("cmp_e_boot_out", "cmp_boot", "cmp_out", ST_ARROW,
             "escribe", (0.5, 1), (0.5, 0)))

# Ambos mundos dependen del contrato de formato
c.append(rel("cmp_e_prep_fmt", "cmp_prep", "cmp_format", ST_ARROW + "dashed=1;",
             "constantes", (0.35, 1), (0.18, 0)))
c.append(rel("cmp_e_fw_fmt", "cmp_fw", "cmp_format", ST_ARROW + "dashed=1;",
             "", (0.5, 1), (0.82, 0)))

pages.append(page("components", "1 · Componentes", c))

# =============================================================== Página 2
c = []
classes = {}


def add(*args, **kwargs):
    cells, h = klass(*args, **kwargs)
    c.extend(cells)
    classes[args[0]] = (args[3], args[4], args[5], h)  # x, y, w, h


# Enumeraciones
add("cls_stage", "«enumeration» PrepStage", [
    "Idle", "Enumerating", "Validating", "Confirming", "Building", "Opening",
    "Unmounting", "Writing", "Syncing", "Verifying", "Done", "Failed",
    "Cancelled", "Incomplete",
], 40, 40, 240)
add("cls_outcome", "«enumeration» PrepOutcome", [
    "None", "Verified", "VerificationFailed", "Cancelled", "Incomplete",
    "PermissionDenied", "InvalidTarget", "Disconnected", "InsufficientSpace",
    "BuildError", "DeviceError", "UnmountError",
], 310, 40, 260)

# Dominio
add("cls_device", "«struct» DeviceInfo", [
    "+ path: string", "+ by_id: string", "+ model: string", "+ serial: string",
    "+ size_bytes: u64", "+ logical_sector: u32", "+ removable: bool",
    "+ transport: DeviceTransport", "+ read_only: bool", "+ partitions: vector",
], 600, 40, 260)
add("cls_config", "«struct» PrepConfig", [
    "+ device: DeviceInfo", "+ image_path: string", "+ esp_bytes: u64",
    "+ volume_label: string", "+ overwrite_confirmed: bool",
], 890, 40, 240)
add("cls_safety", "«struct» SafetyReport", [
    "+ verdict: SafetyVerdict", "+ reasons: vector<string>",
    "+ warnings: vector<string>",
], 1160, 40, 220)
add("cls_policy", "«class» SafetyPolicy", [
    "+ evaluate(device, ctx): SafetyReport",
], 1160, 180, 220)
add("cls_context", "«struct» SystemContext", [
    "+ root_device: string", "+ boot_device: string",
    "+ mounted_device_names: vector",
], 40, 460, 280)

add("cls_progress", "«struct» PrepProgress", [
    "+ stage: PrepStage", "+ bytes_done: u64", "+ bytes_total: u64",
    "+ percent: int", "+ detail: string",
], 600, 380, 240)
add("cls_manifest", "«struct» ImageManifest", [
    "+ disk_bytes: u64", "+ regions: vector<ImageRegion>",
    "+ manifest_sha256: string",
], 890, 380, 240)
add("cls_result", "«struct» PrepResult", [
    "+ outcome: PrepOutcome", "+ failed_stage: PrepStage", "+ message: string",
    "+ image_sha256: string", "+ device_sha256: string", "+ bytes_written: u64",
    "+ synchronized: bool", "+ verified: bool",
], 1160, 380, 220)

# Contratos
add("if_ctrl", "«interface» IPrepController", [], 40, 700, 260, methods=[
    "+ devices(): vector<DeviceInfo>", "+ safety(device): SafetyReport",
    "+ start(config): bool", "+ cancel(): void",
    "+ progress(): PrepProgress", "+ result(): PrepResult",
])
add("if_src", "«interface» IBlockDeviceSource", [], 330, 700, 260, methods=[
    "+ list(): vector<DeviceInfo>",
    "+ refresh(path): optional<DeviceInfo>",
    "+ system_context(): SystemContext",
])
add("if_writer", "«interface» IDeviceWriter", [], 620, 700, 240, methods=[
    "+ open(device): bool", "+ write(data): bool", "+ sync(): bool",
    "+ read_back(data): bool", "+ close(): void",
])
add("if_verify", "«interface» IImageVerifier", [], 890, 700, 240, methods=[
    "+ verify(image): ImageManifest",
])
add("cls_builder", "«class» BootImageBuilder", [], 1160, 700, 220, methods=[
    "+ build(spec): ImageManifest",
])

# Implementaciones
add("impl_ctrl", "«class» PrepController", [], 40, 930, 260, methods=[
    "+ start(config): bool", "+ cancel(): void",
])
add("impl_src", "«class» SysfsBlockDeviceSource", [], 330, 930, 260, methods=[
    "+ list(): vector<DeviceInfo>",
])
add("impl_writer", "«class» PosixDeviceWriter", [], 620, 930, 240, methods=[
    "+ open(device): bool", "+ write(data): bool", "+ sync(): bool",
    "+ read_back(data): bool", "+ close(): void",
])
add("impl_verify", "«class» ImageVerifier", [], 890, 930, 240, methods=[
    "+ verify(image): ImageManifest",
])

# Relaciones estructurales
c.append(rel("rel_composition", "cls_config", "cls_device", ST_COMP, "",
             (0.0, 0.5), (1.0, 0.32)))
c.append(rel("rel_policy_device", "cls_policy", "cls_device", ST_DEP, "evalúa",
             (0.0, 0.5), (1.0, 0.55)))
c.append(rel("rel_policy_report", "cls_policy", "cls_safety", ST_DEP, "",
             (0.5, 0.0), (0.5, 1.0)))
# Realizaciones (interfaz arriba, implementación abajo)
c.append(rel("rel_ctrl", "impl_ctrl", "if_ctrl", ST_REAL, "",
             (0.5, 0.0), (0.5, 1.0)))
c.append(rel("rel_src", "impl_src", "if_src", ST_REAL, "",
             (0.5, 0.0), (0.5, 1.0)))
c.append(rel("rel_writer", "impl_writer", "if_writer", ST_REAL, "",
             (0.5, 0.0), (0.5, 1.0)))
c.append(rel("rel_verify", "impl_verify", "if_verify", ST_REAL, "",
             (0.5, 0.0), (0.5, 1.0)))
pages.append(page("classes", "2 · Clases", c))

# =============================================================== Página 3
c = []
lifelines = [
    ("life_ui", "UI\nMainScene", 80),
    ("life_ctrl", "PrepController", 340),
    ("life_img", "BootImageBuilder", 600),
    ("life_writer", "PosixDeviceWriter", 860),
    ("life_dev", "Dispositivo\n/dev/sdX", 1120),
]
for lid, label, x in lifelines:
    c.append(
        f'<mxCell id="{lid}" parent="1" style="{ST_LIFELINE}" '
        f'value={quoteattr(label)} vertex="1">'
        f'<mxGeometry height="720" width="160" x="{x}" y="40" as="geometry"/>'
        f'</mxCell>'
    )


def msg(cid, src, dst, label, y, ret=False):
    sx = next(x for lid, _, x in lifelines if lid == src) + 80
    dx = next(x for lid, _, x in lifelines if lid == dst) + 80
    style = ST_RET if ret else ST_MSG
    return (
        f'<mxCell id="{cid}" edge="1" parent="1" style="{style}" value={quoteattr(label)}>'
        f'<mxGeometry relative="1" as="geometry">'
        f'<mxPoint x="{sx}" y="{y}" as="sourcePoint"/>'
        f'<mxPoint x="{dx}" y="{y}" as="targetPoint"/>'
        f'</mxGeometry></mxCell>'
    )


msgs = [
    ("sm1", "life_ui", "life_ctrl", "preparar(config)", 150, False),
    ("sm2", "life_ctrl", "life_img", "build(spec)", 205, False),
    ("sm2r", "life_img", "life_ctrl", "imagen + manifiesto", 245, True),
    ("sm3", "life_ctrl", "life_writer", "desmontar(particiones)", 300, False),
    ("sm3b", "life_ctrl", "life_writer", "open(device)", 340, False),
    ("sm4", "life_ctrl", "life_writer", "write(regiones)", 385, False),
    ("sm4d", "life_writer", "life_dev", "write()", 425, False),
    ("sm5", "life_ctrl", "life_writer", "fsync + BLKFLSBUF", 470, False),
    ("sm6", "life_ctrl", "life_writer", "verificar(manifiesto)", 515, False),
    ("sm6d", "life_writer", "life_dev", "read()", 555, False),
    ("sm7", "life_writer", "life_ctrl", "hashes del dispositivo", 610, True),
    ("sm8", "life_ctrl", "life_ui", "PrepResult", 660, True),
    ("sm9", "life_ui", "life_ctrl", "cancelar()", 710, False),
]
for m in msgs:
    c.append(msg(*m))
pages.append(page("sequence", "3 · Secuencia", c))

# =============================================================== Página 4
# Estados: serpentina Idle..Done y terminales de error/cancelación.
c = []
c.append(box(
    "st_title", ST_TITLE,
    "<b><font style='font-size:18px'>Estados de la preparación</font></b><br>"
    "<font style='font-size:10px'>PrepStage · la cancelación no es un éxito; "
    "el medio cancelado queda Incomplete.</font>",
    40, 12, 1000, 50))

states = [
    ("st_idle", "<b>Idle</b><br><font style='font-size:9px'>sin preparación activa</font>",
     60, 110, 180, 80, ""),
    ("st_enum", "<b>Enumerating</b><br><font style='font-size:9px'>lee /sys/block</font>",
     280, 110, 180, 80, ""),
    ("st_val", "<b>Validating</b><br><font style='font-size:9px'>SafetyPolicy + validación</font>",
     500, 110, 180, 80, ""),
    ("st_conf", "<b>Confirming</b><br><font style='font-size:9px'>casilla + Enter</font>",
     720, 110, 180, 80, ""),
    ("st_build", "<b>Building</b><br><font style='font-size:9px'>GPT + FAT32 + EFI</font>",
     940, 110, 180, 80, ""),
    ("st_unmount", "<b>Unmounting</b><br><font style='font-size:9px'>umount2(MNT_DETACH)</font>",
     940, 280, 180, 80, ""),
    ("st_open", "<b>Opening</b><br><font style='font-size:9px'>O_RDWR | O_EXCL</font>",
     720, 280, 180, 80, ""),
    ("st_write", "<b>Writing</b><br><font style='font-size:9px'>bloques de 1 MiB</font>",
     500, 280, 180, 80, ""),
    ("st_sync", "<b>Syncing</b><br><font style='font-size:9px'>fsync + BLKFLSBUF</font>",
     280, 280, 180, 80, ""),
    ("st_verify", "<b>Verifying</b><br><font style='font-size:9px'>relee y compara SHA-256</font>",
     60, 280, 180, 80, ""),
    ("st_done", "<b>Done</b><br><font style='font-size:9px'>PrepOutcome::Verified</font>",
     60, 450, 180, 90, "#f5f5f5"),
    ("st_incomplete", "<b>Incomplete</b><br><font style='font-size:9px'>medio no verificado</font>",
     320, 450, 180, 90, "#f5f5f5"),
    ("st_cancelled", "<b>Cancelled</b><br><font style='font-size:9px'>cancelación pedida</font>",
     580, 450, 180, 90, "#f5f5f5"),
    ("st_failed", "<b>Failed</b><br><font style='font-size:9px'>error con etapa exacta</font>",
     840, 450, 180, 90, "#f5f5f5"),
]
for sid, label, x, y, w, h, fill in states:
    style = ST_STATE + (f"fillColor={fill};" if fill else "")
    c.append(box(sid, style, label, x, y, w, h))

# Camino feliz en serpentina
c.append(rel("st_e_idle_enum", "st_idle", "st_enum", ST_ARROW,
             "F5 / iniciar", (1.0, 0.5), (0.0, 0.5)))
c.append(rel("st_e_enum_val", "st_enum", "st_val", ST_ARROW,
             "dispositivos", (1.0, 0.5), (0.0, 0.5)))
c.append(rel("st_e_val_conf", "st_val", "st_conf", ST_ARROW,
             "apto", (1.0, 0.5), (0.0, 0.5)))
c.append(rel("st_e_conf_build", "st_conf", "st_build", ST_ARROW,
             "casilla + Enter", (1.0, 0.5), (0.0, 0.5)))
c.append(rel("st_e_build_unmount", "st_build", "st_unmount", ST_ARROW,
             "build listo", (0.5, 1.0), (0.5, 0.0)))
c.append(rel("st_e_unmount_open", "st_unmount", "st_open", ST_ARROW,
             "desmontado", (0.0, 0.5), (1.0, 0.5)))
c.append(rel("st_e_open_write", "st_open", "st_write", ST_ARROW,
             "O_EXCL", (0.0, 0.5), (1.0, 0.5)))
c.append(rel("st_e_write_sync", "st_write", "st_sync", ST_ARROW,
             "bloques 1 MiB", (0.0, 0.5), (1.0, 0.5)))
c.append(rel("st_e_sync_verify", "st_sync", "st_verify", ST_ARROW,
             "fsync + BLKFLSBUF", (0.0, 0.5), (1.0, 0.5)))
c.append(rel("st_e_verify_done", "st_verify", "st_done", ST_ARROW,
             "SHA-256 coincide", (0.5, 1.0), (0.5, 0.0)))

# Terminales de error y cancelación
c.append(rel("st_e_open_failed", "st_open", "st_failed", ST_ARROW + "dashed=1;",
             "EACCES / EIO", (0.5, 1.0), (0.5, 0.0)))
c.append(rel("st_e_write_cancel", "st_write", "st_cancelled", ST_ARROW + "dashed=1;",
             "cancelar", (0.5, 1.0), (0.5, 0.0)))
c.append(rel("st_e_verify_incomplete", "st_verify", "st_incomplete", ST_ARROW + "dashed=1;",
             "cancelado a mitad", (0.5, 1.0), (0.5, 0.0)))

c.append(box(
    "st_note_terms", ST_NOTE,
    "<b>Terminales</b> · <b>Done</b> solo se alcanza con "
    "<font face='monospace'>PrepOutcome::Verified</font>. <b>Failed</b> agrupa "
    "PermissionDenied, InvalidTarget, Disconnected, InsufficientSpace, BuildError, "
    "DeviceError y UnmountError con la etapa exacta.",
    40, 630, 1340, 55))
c.append(box(
    "st_note_restart", ST_NOTE,
    "<b>Incomplete</b> es una cancelación durante Writing o Verifying: el medio no se "
    "declara verificado. Volver a Idle exige nueva enumeración y nueva confirmación.",
    40, 700, 1340, 45))

pages.append(page("states", "4 · Estados", c))

# =============================================================== Página 5
c = []
c.append(
    '<mxCell id="ley_title" parent="1" style="text;html=1;whiteSpace=wrap;'
    'overflow=hidden;rounded=0;fontSize=16;fontStyle=1;" '
    'value="Leyenda UML" vertex="1">'
    '<mxGeometry height="40" width="420" x="40" y="20" as="geometry"/></mxCell>'
)
sample_cells, _ = klass("ley_class", "NombreDeClase", [
    "+ atributoPublico: String",
    "# atributoProtegido: int",
    "- atributoPrivado: double",
], 40, 90, 240, methods=[
    "+ metodoPublico(): void",
    "# metodoProtegido(): void",
    "- metodoPrivado(): void",
])
c.extend(sample_cells)

legend_items = [
    ("Herencia / implements", "La subclase hereda o implementa la interfaz.", 420, ST_REAL),
    ("Asociación", "Una clase usa o conoce a otra.", 490, ST_ASOC),
    ("Dependencia", "Uso puntual; no hay atributo persistente.", 560, ST_DEP),
    ("Composición", "El todo posee la parte y controla su ciclo de vida.", 630, ST_COMP),
    ("Agregación", "El todo contiene partes que pueden compartirse.", 700, ST_AGGR),
]
for i, (title, desc, y, style) in enumerate(legend_items):
    c.append(
        f'<mxCell id="ley_e{i}" edge="1" parent="1" style="{style}" value="">'
        f'<mxGeometry relative="1" as="geometry">'
        f'<mxPoint x="60" y="{y}" as="sourcePoint"/>'
        f'<mxPoint x="220" y="{y}" as="targetPoint"/>'
        f'</mxGeometry></mxCell>'
    )
    c.append(
        f'<mxCell id="ley_t{i}" parent="1" style="{ST_TEXT}" '
        f'value={quoteattr("<b>" + title + "</b><br>" + desc)} vertex="1">'
        f'<mxGeometry height="50" width="380" x="260" y="{y - 25}" as="geometry"/></mxCell>'
    )

c.append(
    f'<mxCell id="ley_vis" parent="1" style="{ST_TEXT}" '
    f'value="&lt;b&gt;Visibilidad&lt;/b&gt;&lt;br&gt;+ público · # protegido · - privado&lt;br&gt;'
    f'«enumeration» valores · «struct» datos · «interface» contrato" vertex="1">'
    f'<mxGeometry height="80" width="420" x="40" y="780" as="geometry"/></mxCell>'
)
c.append(
    f'<mxCell id="ley_note" parent="1" style="{ST_TEXT}" '
    f'value="Diagrama monocromo. Los compartimentos siguen el orden nombre / datos / métodos." vertex="1">'
    f'<mxGeometry height="40" width="620" x="40" y="890" as="geometry"/></mxCell>'
)
pages.append(page("legend", "5 · Leyenda", c))

xml = ('<mxfile host="app.diagrams.net" pages="5">\n' + "\n".join(pages) + "\n</mxfile>\n")
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write(xml)
print("escrito", OUT, len(xml), "bytes")
