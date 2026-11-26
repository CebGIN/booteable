# Prueba física P9 · arranque en el equipo de laboratorio

Esta prueba cierra RF-16: la USB preparada por la herramienta inicia en un
equipo UEFI real y muestra el mensaje del grupo. Es la única prueba de WP5 que
necesita hardware; las demás corren headless.

## Antes de empezar

- Equipo de laboratorio **x86_64 con UEFI**, autorizado para la prueba. No usar
  un equipo ajeno ni desactivar Secure Boot sin permiso.
- USB dedicada y respaldada. La herramienta no crea respaldos.
- Tamaño mínimo: 64 MiB para la imagen de 512 bytes; 320 MiB para la imagen 4Kn
  con ESP de 260 MiB.
- `sudo` para escribir el dispositivo de bloque.
- Anotar el estado de Secure Boot. Si está activo, el binario sin firma será
  bloqueado; eso es un resultado esperado, no un fallo de la imagen.

## 1. Preparar la imagen y verificar el destino

```bash
cmake -S . -B build
cmake --build build

./build/booteable-image build --output build/booteable.img \
  --size-mb 64 --label BOOTEABLE
./build/booteable-image verify --image build/booteable.img
```

Identificar la USB **por modelo, serie y tamaño**, nunca solo por `/dev/sdX`:

```bash
lsblk -o NAME,TRAN,RM,SERIAL,SIZE,MOUNTPOINTS
```

Anotar la salida. Comprobar que el nodo elegido es la USB y no el disco del
sistema. Guardar el SHA-256 de la imagen:

```bash
sha256sum build/booteable.img
```

## 2. Escribir desde la TUI

```bash
sudo ./build/booteable
```

1. Verificar que la lista coincide con `lsblk` (modelo, serie, tamaño y
   montajes).
2. Elegir la USB y pulsar `Enter`.
3. Marcar la casilla de advertencia y pulsar `Enter`.
4. Esperar a que la etapa termine en **VERIFICADO**.

Si se cancela o se desconecta la USB, el resultado debe ser **INCOMPLETO** o
**DESCONECTADO**; nunca **VERIFICADO**. En ese caso, repetir desde el paso 1.

Alternativa headless (también válida para la evidencia):

```bash
sudo ./build/booteable-image flash --device /dev/sdX \
  --image build/booteable.img
```

La CLI pide confirmación explícita sin `--yes`. Solo imprimirá `VERIFICADO` si
la relectura reproduce el manifiesto.

## 3. Arrancar en el equipo

1. Apagar el equipo y conectar la USB.
2. Abrir el menú de arranque del firmware.
3. Elegir la entrada **UEFI** de la USB (no la entrada BIOS/CSM).
4. Verificar el mensaje:

```text
Grupo: booteable
Build: <hash corto>
BOOTEABLE-BOOT-OK
Apagando el equipo.
```

El programa espera una tecla con límite y se apaga solo con
`ResetSystem(EfiResetShutdown)`.

## 4. Registro de evidencia

| Campo | Valor |
|---|---|
| Fecha y operador | |
| Equipo y versión de firmware | |
| Secure Boot (activo/inactivo) | |
| USB: marca, modelo, serie, tamaño | |
| Nodo usado (`/dev/sdX`) | |
| Salida de `lsblk` | |
| SHA-256 de la imagen | |
| Resultado de la TUI/CLI | |
| Marcador en pantalla | |
| Foto o log serie | |
| Observaciones | |

Adjuntar la foto de la pantalla o el log serie. No hace falta subir la imagen
`.img` ni la captura completa al repositorio; alcanza con el registro y el hash.

## 5. Si algo falla

| Síntoma | Qué revisar |
|---|---|
| `Access Denied` / `Security Violation` | Secure Boot activo. Registrar y usar un equipo autorizado con Secure Boot desactivado o una clave enrolada; no modificar claves ajenas. |
| La USB no aparece en el menú | Confirmar que el firmware está en modo UEFI, que el medio es la USB correcta y que `verify` pasó. |
| Arranca pero no muestra el marcador | Revisar que se eligió la entrada UEFI correcta y que la imagen se escribió completa (`VERIFICADO`). |
| La escritura queda incompleta | Repetir desde la preparación; no declarar éxito sin relectura. |
| 4Kn no arranca | Probar la imagen de 512 y registrar el soporte del equipo; el 4Kn es opcional si el hardware no lo expone. |

**Criterio de aprobación:** P9 pasa solo si el equipo muestra el mensaje del
grupo y `BOOTEABLE-BOOT-OK`. Si no, se registra el fallo con la etapa y el
mensaje exactos; no se marca WP5 como cerrado en hardware.
