# Proyecto: USB booteable con programa propio

## Descripción del proyecto

El grupo construye un programa que prepara una memoria USB para ejecutar un
**programa de arranque propio**, sin cargar un sistema operativo existente. Al
seleccionar la USB en el menú de arranque de un equipo compatible debe aparecer
el resultado del programa desarrollado por el grupo, por ejemplo un mensaje.

El proyecto tiene dos partes: **el programa que arranca** y **la herramienta que
prepara la USB**. La herramienta usa llamadas al sistema (syscalls) o las API del
sistema operativo para trabajar con la memoria USB. El programa de arranque usa
los servicios de BIOS o UEFI, o accede al hardware directamente; en ese momento
no está ejecutándose el sistema operativo desde el que se preparó la USB.

La ruta elegida es **UEFI x86_64**: una aplicación EFI en
`\EFI\BOOT\BOOTX64.EFI`, dentro de una ESP FAT32, sobre una tabla GPT.

## Requerimientos

Los identificadores `RF-*` se usan en `Arquitectura.md` y en las pruebas.

### Programa de arranque

- **RF-01 · Código propio.** El comportamiento visible lo produce una aplicación
  EFI desarrollada por el grupo. No se usa una imagen de SO ya preparada ni un
  gestor de arranque de terceros como contenido principal.
- **RF-02 · Sin SO.** La aplicación no depende de un kernel, de un sistema de
  archivos montado por Linux ni de librerías del sistema. Solo usa los servicios
  de arranque y de ejecución de UEFI.
- **RF-03 · Resultado visible e identificable.** Debe mostrar un mensaje estable
  con el nombre del grupo, la materia, la frase "arranque UEFI propio" y un
  identificador de compilación. Debe verse tanto en la consola UEFI como en el
  puerto serie.
- **RF-04 · No destructivo.** No escribe en discos internos, no modifica variables
  UEFI persistentes y no cambia la configuración del firmware. Espera una tecla y
  devuelve el control al firmware.

### Preparador

- **RF-05 · Enumeración.** Lista los dispositivos de bloque disponibles con ruta
  (`/dev/sdX`), modelo, número de serie, tamaño, transporte, carácter extraíble,
  `by-id` y puntos de montaje.
- **RF-06 · Selección y destino seguro.** Permite elegir una USB y muestra su
  identidad completa. Rechaza discos del sistema, dispositivos no USB, no
  extraíbles, sin serie estable, de solo lectura, con holds de LVM/RAID o cuyo
  tamaño no alcance.
- **RF-07 · Confirmación de sobrescritura.** Antes de escribir, muestra el
  dispositivo, el tamaño, el programa que se va a instalar y el hash de la
  imagen, advierte que se perderá todo el contenido y exige marcar una casilla
  de advertencia y pulsar `Enter`.
- **RF-08 · Construcción propia del medio.** Genera MBR protector, GPT primaria y
  de respaldo, ESP FAT32 y el archivo `EFI/BOOT/BOOTX64.EFI`. Se permiten
  librerías y APIs del sistema operativo, pero las operaciones de detectar,
  particionar, formatear, escribir, sincronizar y verificar las ejecuta el
  código del grupo; no se delegan en `parted`, `mkfs.*`, `dd` o un grabador
  externo.
- **RF-09 · Formato coherente.** La GPT cumple los CRC, la ESP usa el tipo
  `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`, la FAT32 tiene dos tablas y el nombre
  de arranque es exactamente `\EFI\BOOT\BOOTX64.EFI`.
- **RF-10 · Desmontaje.** Detecta las particiones montadas del destino y las
  desmonta de forma controlada antes de escribir. Si no puede desmontarlas,
  cancela sin tocar el dispositivo.
- **RF-11 · Progreso.** Muestra la etapa actual (construir, abrir, desmontar,
  escribir, sincronizar, verificar), bytes o porcentaje, y mantiene la interfaz
  operativa durante la preparación.
- **RF-12 · Cancelación.** Permite cancelar antes de empezar o durante la
  escritura. Si el medio queda incompleto, lo informa explícitamente y no lo
  presenta como verificado.
- **RF-13 · Sincronización y verificación.** Completa las escrituras pendientes
  (`fsync`), relee el dispositivo y compara el contenido con la imagen
  (SHA-256). Además valida GPT, FAT y el archivo de arranque.
- **RF-14 · Manejo de errores.** Informa con mensajes claros: falta de permisos,
  destino inválido, capacidad insuficiente, desconexión, fallo de lectura,
  escritura o verificación, y error al desmontar. Nunca declara éxito ante un
  fallo.
- **RF-15 · Prueba virtual.** Permite construir una imagen y arrancarla en QEMU
  con OVMF antes de usar una USB física.
- **RF-16 · Arranque físico.** La USB preparada inicia en un equipo UEFI
  compatible y muestra el mensaje del grupo.

### Restricciones y supuestos

- **RNF-01 · Plataforma.** Preparador en Linux x86_64. Programa de arranque
  `x86_64` UEFI (PE32+), sin BIOS legado en el MVP.
- **RNF-02 · Compatibilidad.** El equipo de prueba debe admitir UEFI x86_64. Con
  Secure Boot activo, un binario sin firma no arranca: se usa un equipo de
  laboratorio con autorización para desactivarlo o se documenta la firma como
  trabajo futuro. No se alteran claves ni opciones de equipos ajenos.
- **RNF-03 · Sectores.** El MVP admite dispositivos con sectores lógicos de 512
  y 4096 bytes (4Kn). Otro tamaño se rechaza con un mensaje explícito. En 4Kn la
  ESP mínima es 260 MiB por el mínimo de clústeres FAT32.
- **RNF-04 · Privilegios.** Escribir un dispositivo de bloque requiere `root`.
  Sin privilegios la herramienta permite enumerar y exportar la imagen, pero no
  escribir.
- **RNF-05 · Seguridad de datos.** Se usa una USB dedicada de laboratorio. El
  usuario debe respaldar su contenido; la herramienta no crea respaldos.
- **RNF-06 · Núcleo headless.** El núcleo no depende de Guayaba; las pruebas no
  requieren red, root ni USB real.

## Pruebas requeridas

| Prueba | Tipo | Qué demuestra |
|---|---|---|
| **P1 · Conteo de dispositivos** | manual | La lista coincide con `lsblk`/`/sys/block` y muestra identidad y montajes |
| **P2 · Rechazo de destino** | unitaria + manual | El disco raíz, un disco interno, un dispositivo sin serie y una USB montada se rechazan con motivo |
| **P3 · Confirmación** | manual | Sin la casilla marcada y `Enter` no se abre el dispositivo ni se escribe un byte |
| **P4 · Imagen GPT/FAT32** | unitaria | `booteable-image verify` valida MBR protector, GPT primaria y de respaldo, CRC, FAT y archivo |
| **P5 · Integridad** | unitaria | Alterar un byte de la imagen hace fallar la verificación |
| **P6 · Escritura y verificación** | integración | Escribir en un archivo/dispositivo falso y comparar SHA-256; simular desconexión y error de escritura |
| **P7 · Cancelación** | integración | Cancelar durante la escritura deja el resultado `Incomplete` y no `Verified` |
| **P8 · Arranque virtual** | integración | QEMU + OVMF arranca la imagen y el puerto serie contiene `BOOTEABLE-BOOT-OK` |
| **P9 · Arranque físico** | manual | La USB inicia en el equipo de prueba y muestra el mensaje del grupo |
| **P10 · Permisos** | manual | Sin `root`, la escritura se bloquea con un mensaje que sugiere `sudo`; la app no se cae |

## Fuera de alcance del MVP

- Arranque BIOS/CSM, arquitecturas ARM o RISC-V.
- MBR con particiones clásicas, FAT16/FAT32 fuera de la ESP, NTFS, exFAT o ext4.
- Copiar imágenes ISO de sistemas operativos (esa es otra actividad).
- Firma Authenticode/Secure Boot y matrícula automática de claves.
- Arranque híbrido, multiboot, persistencia o instalación de un SO.
- Preparar varias USB en paralelo.
