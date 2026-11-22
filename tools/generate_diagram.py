#!/usr/bin/env python3
"""Genera docs/diagrama.drawio con UML nativo de draw.io.

Tres páginas, estilo monocromo, siguiendo el diagrama de tasks-lens:
  1. Clases    (swimlane con compartimentos, flechas UML nativas)
  2. Secuencia (umlLifeline)
  3. Leyenda   (muestras de relaciones y visibilidad)
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
pages.append(page("classes", "1 · Clases", c))

# =============================================================== Página 2
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
pages.append(page("sequence", "2 · Secuencia", c))

# =============================================================== Página 3
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
        f'<mxGeometry height="50" width="380" x="260" y="{y - 25}" as="geometry"/>'
        f'</mxCell>'
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
pages.append(page("legend", "3 · Leyenda", c))

xml = ('<mxfile host="app.diagrams.net" pages="3">\n' + "\n".join(pages) + "\n</mxfile>\n")
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write(xml)
print("escrito", OUT, len(xml), "bytes")
