# Vistas previas del diagrama

Las imágenes se exportan desde [`../diagrama.drawio`](../diagrama.drawio) con el
CLI de draw.io. El `.drawio` es el original y se guarda como XML sin comprimir
para poder revisarlo en el repositorio.

El estilo es monocromo y usa UML nativo de draw.io: `swimlane` con
compartimentos para las clases, `shape=umlLifeline` para la secuencia y flechas
UML para las relaciones.

| # | Página | Archivo |
|---|---|---|
| 1 | Clases | [`01-clases.png`](01-clases.png) |
| 2 | Secuencia | [`02-secuencia.png`](02-secuencia.png) |
| 3 | Leyenda | [`03-leyenda.png`](03-leyenda.png) |

## Regenerar

El generador está en [`../../tools/generate_diagram.py`](../../tools/generate_diagram.py).
Reescribe `docs/diagrama.drawio`; después se exportan las páginas. El snap de
draw.io no puede escribir en `/tmp`, así que se usa una ruta del proyecto:

```bash
python3 tools/generate_diagram.py

for i in 1 2 3; do
  case $i in
    1) name=01-clases ;;
    2) name=02-secuencia ;;
    3) name=03-leyenda ;;
  esac
  drawio -x -f png -o "docs/diagramas/$name.png" -p "$i" -s 1.3 docs/diagrama.drawio
done
```

Si se edita el `.drawio` a mano, no se debe comprimir el XML: las páginas tienen
que quedar legibles con `grep` y en el diff.
