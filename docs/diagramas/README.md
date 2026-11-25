# Vistas previas del diagrama

Las imágenes se exportan desde [`../diagrama.drawio`](../diagrama.drawio) con el
CLI de draw.io. El `.drawio` es el original y se guarda como XML sin comprimir
para poder revisarlo en el repositorio.

El estilo es monocromo: contenedores para los dos mundos y el medio, cajas de
componente para los módulos, `swimlane` con compartimentos para las clases,
`shape=umlLifeline` para la secuencia y flechas UML para las relaciones.

| # | Página | Archivo |
|---|---|---|
| 1 | Componentes | [`01-componentes.png`](01-componentes.png) |
| 2 | Clases | [`02-clases.png`](02-clases.png) |
| 3 | Secuencia | [`03-secuencia.png`](03-secuencia.png) |
| 4 | Estados | [`04-estados.png`](04-estados.png) |
| 5 | Leyenda | [`05-leyenda.png`](05-leyenda.png) |

## Regenerar

El generador está en [`../../tools/generate_diagram.py`](../../tools/generate_diagram.py).
Reescribe `docs/diagrama.drawio`; después se exportan las páginas. El snap de
draw.io no puede escribir en `/tmp`, así que se usa una ruta del proyecto:

```bash
python3 tools/generate_diagram.py

for i in 1 2 3 4 5; do
  case $i in
    1) name=01-componentes ;;
    2) name=02-clases ;;
    3) name=03-secuencia ;;
    4) name=04-estados ;;
    5) name=05-leyenda ;;
  esac
  drawio -x -f png -o "docs/diagramas/$name.png" -p "$i" -s 1.3 docs/diagrama.drawio
done
```

Si se edita el `.drawio` a mano, no se debe comprimir el XML: las páginas tienen
que quedar legibles con `grep` y en el diff.
