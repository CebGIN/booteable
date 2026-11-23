# Glosario · preparador UEFI y programa de arranque

Este documento reúne los términos que aparecen en la documentación, en la TUI y
en los mensajes de error. El código y los nombres técnicos van en inglés; la
prosa y las etiquetas de la interfaz, en español.

---

## 1. Firmware y arranque

| Término | Significado |
|---|---|
| **UEFI** | *Unified Extensible Firmware Interface*. Interfaz entre el firmware y el software de arranque. Sustituye al BIOS legado en equipos modernos. |
| **BIOS** | *Basic Input/Output System*. Firmware legado de 16 bits. El MVP no lo usa; un equipo con solo BIOS no puede arrancar esta USB. |
| **CSM** | *Compatibility Support Module*. Modo de algunos firmware UEFI que emula BIOS. No se usa. |
| **Firmware** | Programa grabado en la placa que inicializa el hardware y busca un gestor de arranque. |
| **Secure Boot** | Función UEFI que solo ejecuta binarios firmados con claves autorizadas. Un `BOOTX64.EFI` sin firma no arranca con Secure Boot activo. |
| **MOK** | *Machine Owner Key*. Mecanismo para que el dueño del equipo enrolle una clave propia y firme binarios. Trabajo futuro, no MVP. |
| **EFI System Partition (ESP)** | Partición FAT requerida por UEFI que contiene las aplicaciones de arranque. En este proyecto es FAT32 y se identifica con el GUID `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`. |
| **`BOOTX64.EFI`** | Nombre que busca el firmware en `\EFI\BOOT\` de un medio extraíble para arrancar en x86_64. Es el programa propio del grupo. |
| **PE32+** | Formato ejecutable de Windows/EFI de 64 bits. La aplicación EFI se compila como PE32+ con subsistema 10. |
| **`efi_main`** | Punto de entrada de la aplicación EFI. Equivale al `main` de un programa común. |
| **`EFI_BOOT_SERVICES`** | Tabla de servicios UEFI (consola, memoria, protocolos) disponible mientras el firmware controla el equipo. |
| **`EFI_SYSTEM_TABLE`** | Estructura raíz que el firmware pasa a `efi_main`; contiene consola, entrada y boot services. |
| **`ConOut`** | `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`: salida de texto a la consola UEFI. |
| **`ConIn`** | `EFI_SIMPLE_TEXT_INPUT_PROTOCOL`: lectura del teclado en modo UEFI. |
| **`SerialIo`** | `EFI_SERIAL_IO_PROTOCOL`: puerto serie. Se usa para capturar el mensaje en QEMU sin ventana. |
| **GOP** | *Graphics Output Protocol*. Salida gráfica UEFI. El MVP no la usa; se queda en modo texto. |
| **`ExitBootServices`** | Llamada UEFI que transfiere el control al SO. El programa propio **no** la usa: se queda en el entorno de arranque. |
| **OVMF** | *Open Virtual Machine Firmware*. Implementación de UEFI para QEMU. Permite probar la imagen sin USB física. |
| **QEMU** | Emulador de máquinas. Se usa con OVMF y la imagen como disco para la prueba virtual. |

## 2. Discos, particiones y sistemas de archivos

| Término | Significado |
|---|---|
| **Dispositivo de bloque** | Nodo en `/dev` que representa un disco o partición (`/dev/sdb`, `/dev/sdb1`). |
| **LBA** | *Logical Block Address*. Número de sector lógico dentro de un disco. Cada LBA mide el sector lógico del dispositivo: 512 o 4096 bytes. |
| **Sector lógico** | Unidad mínima direccionable del dispositivo. El MVP acepta 512 y 4096 bytes; otro tamaño se rechaza. |
| **512e / 4Kn** | Discos con sectores físicos de 4096 bytes que exponen 512 (`512e`) o 4096 (`4Kn`) bytes lógicos. En 4Kn la ESP mínima es 260 MiB por el mínimo de clústeres FAT32. |
| **MBR** | *Master Boot Record*. Primer sector del disco. En GPT se usa un **MBR protector** para que herramientas viejas no confundan el disco con uno vacío. |
| **MBR protector** | MBR con una sola entrada tipo `0xEE` que cubre todo el disco y oculta las particiones GPT a software legado. |
| **GPT** | *GUID Partition Table*. Esquema de particionado moderno con cabecera y entradas primarias y una copia de respaldo al final del disco. |
| **CRC32** | Suma de verificación de 32 bits. GPT valida con CRC32 la cabecera y la tabla de entradas. |
| **GUID** | *Globally Unique Identifier*. Identificador de 128 bits. GPT usa GUID para el disco, cada partición y cada tipo de partición. |
| **Tabla de particiones** | Arreglo de 128 entradas de 128 bytes en GPT; ocupa 16 KiB (32 sectores de 512 o 4 de 4096) y se escribe dos veces. |
| **Espacio alineado a 1 MiB** | La ESP empieza en el LBA 2048 para respetar la alineación típica de SSD/USB y simplificar los cálculos. |
| **FAT32** | Sistema de archivos de la ESP. Usa una tabla de asignación de archivos (FAT) para encadenar clústeres. |
| **BPB** | *BIOS Parameter Block*. Cabecera del sector de arranque FAT con tamaño de sector, clúster, cantidad de FAT, etc. |
| **Clúster** | Unidad de asignación de FAT. En este proyecto es de 1 sector lógico (512 o 4096 bytes). |
| **FSInfo** | Sector de FAT32 con cantidad de clústeres libres y próximo clúster libre. |
| **FAT primaria / de respaldo** | Las dos copias idénticas de la tabla de asignación. Se escriben ambas. |
| **8.3** | Nombre corto de FAT: hasta 8 caracteres de nombre y 3 de extensión. `BOOTX64.EFI` entra en ese formato. |
| **Etiqueta de volumen** | Nombre del sistema de archivos. En este proyecto es `BOOTEABLE`. |
| **Imagen de disco** | Archivo que contiene byte a byte lo que se escribirá en la USB: MBR, GPT, ESP y archivo EFI. |
| **Imagen raw** | Imagen sin metadatos de formato adicional; se escribe tal cual en el dispositivo. |

## 3. Linux, syscalls y dispositivos

| Término | Significado |
|---|---|
| **sysfs** | Sistema de archivos virtual en `/sys` que expone dispositivos y atributos del kernel. De ahí se leen `removable`, `size`, `serial` y el modelo. |
| **`/proc/self/mountinfo`** | Archivo del kernel con los montajes del proceso actual. Se usa para detectar particiones montadas y el disco raíz. |
| **`by-id`** | Enlace estable en `/dev/disk/by-id/` que identifica un disco por fabricante, modelo y serie, no por el orden de detección. |
| **`removable`** | Atributo sysfs que indica si el medio es extraíble (`1`) o fijo (`0`). |
| **`holders`** | Directorio sysfs que lista subsistemas que usan un dispositivo (por ejemplo `dm` o `md`). Si no está vacío, no se escribe. |
| **`major:minor`** | Par de números que identifica un dispositivo en el kernel. Se usa para relacionar montajes con discos. |
| **`open`** | Syscall para abrir un archivo o dispositivo. La escritura usa `O_RDWR`, `O_CLOEXEC` y `O_EXCL`. |
| **`O_EXCL`** | Flags que, junto con `O_RDWR` en un dispositivo de bloque, hacen fallar la apertura si el dispositivo está en uso. |
| **`ioctl`** | Syscall de control específica de un dispositivo. Aquí se usa `BLKGETSIZE64` y `BLKSSZGET`. |
| **`BLKGETSIZE64`** | `ioctl` que devuelve el tamaño del dispositivo de bloque en bytes. |
| **`BLKSSZGET`** | `ioctl` que devuelve el tamaño del sector lógico. |
| **`BLKFLSBUF`** | `ioctl` que descarta la caché del dispositivo de bloque. Ayuda a que la relectura no vea datos viejos. |
| **`fsync`** | Syscall que fuerza a bajar a disco las escrituras pendientes. |
| **`umount2`** | Syscall que desmonta un sistema de archivos. Con `MNT_DETACH` lo hace de forma diferida y segura. |
| **`errno`** | Código de error de la última syscall. La UI lo traduce a un mensaje comprensible. |
| **`EACCES` / `EPERM`** | Falta de permisos. Escribir un dispositivo de bloque requiere `root`. |
| **`EBUSY`** | El recurso está en uso (por ejemplo, el dispositivo está montado). |
| **`ENODEV` / `ENXIO`** | El dispositivo desapareció o no existe; típico de una USB desconectada. |
| **`EIO`** | Error de entrada/salida; puede indicar un medio defectuoso. |
| **`ENOSPC`** | No hay espacio suficiente. |
| **`root`** | Usuario con privilegios para abrir y escribir dispositivos de bloque. |
| **`sudo`** | Comando para ejecutar la TUI con privilegios. La herramienta no usa `setuid`. |

## 4. Seguridad y operación

| Término | Significado |
|---|---|
| **Destino** | Dispositivo de bloque seleccionado para escribir. |
| **Destino seguro** | USB extraíble, con serie y `by-id` verificables, que no contiene el sistema operativo ni está en uso. |
| **Confirmación explícita** | Marcar la casilla de advertencia y pulsar `Enter`; sin ella no se abre el dispositivo. |
| **Desmontaje** | Quitar los sistemas de archivos montados del destino antes de escribir. |
| **Verificación** | Releer el dispositivo después de escribir y comparar SHA-256 contra la imagen. |
| **`Verified`** | Resultado final correcto: escritura sincronizada y relectura idéntica. |
| **`Incomplete`** | La operación se canceló o interrumpió; el medio puede servir o no, pero no se presenta como verificado. |
| **`Disconnected`** | La USB se retiró durante la operación. |
| **SHA-256** | Función hash de 256 bits. Identifica la imagen y detecta cualquier diferencia en la relectura. |
| **Hash de compilación** | Prefijo del hash del binario EFI; aparece en el mensaje de arranque y en la UI. |
| **Prueba virtual** | Arranque de la imagen en QEMU con OVMF antes de usar hardware. |
| **Prueba física** | Arranque real de la USB en un equipo UEFI. |

## 5. Etapas y resultados de la TUI

### Etapas (`Etapa:`)

| Etapa | Qué está pasando |
|---|---|
| `Inactivo` | No hay preparación en curso. |
| `Enumerando` | Leyendo `/sys/block` y montajes. |
| `Validando` | Revisando configuración y política de seguridad. |
| `Confirmando` | Esperando la casilla de advertencia y `Enter`. |
| `Construyendo` | Generando GPT, FAT32 y el archivo EFI en memoria. |
| `Abriendo` | Abriendo el dispositivo con `O_EXCL`. |
| `Desmontando` | Desmontando las particiones del destino. |
| `Escribiendo` | Copiando la imagen al dispositivo. |
| `Sincronizando` | Ejecutando `fsync` y `BLKFLSBUF`. |
| `Verificando` | Releyendo y comparando SHA-256. |
| `Listo` | Preparación verificada. |
| `Cancelado` | Cancelación solicitada por el usuario. |
| `Incompleto` | La operación no terminó o no se pudo verificar. |
| `Falló` | Error con una causa concreta. |

### Resultados (`Resultado:`)

| Resultado | Significado |
|---|---|
| `Verificado` | Todo coincide; la USB está lista para probar. |
| `No verificado` | La escritura terminó pero la relectura no coincide. |
| `Cancelado` | El usuario canceló; el medio puede quedar incompleto. |
| `Incompleto` | Se interrumpió (desconexión, error) antes de verificar. |
| `Sin permisos` | Falta `root` para escribir el dispositivo. |
| `Destino inválido` | No es una USB válida o no se puede identificar. |
| `Capacidad insuficiente` | La imagen no entra en el dispositivo. |
| `Desconectado` | La USB desapareció durante la operación. |
| `Error de dispositivo` | Falló una lectura o escritura. |
| `Error de construcción` | No se pudo generar la imagen GPT/FAT32. |
| `Error de desmontaje` | No se pudieron liberar las particiones montadas. |

## 6. Unidades y abreviaturas

| Unidad | Significado |
|---|---|
| **B** | Byte. |
| **KiB / MiB / GiB** | 1024 B / 1024² B / 1024³ B. La UI usa estas unidades binarias. |
| **LBA** | Número de sector lógico. |
| **sector** | 512 o 4096 bytes, según el dispositivo. |
| **clúster** | 512 o 4096 bytes (1 sector lógico) en la FAT de este proyecto. |
| **SHA-256** | 64 caracteres hexadecimales. |
| **CRC32** | 8 caracteres hexadecimales. |
| **GUID** | 36 caracteres con guiones (`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`). |
| **%** | Porcentaje de bytes escritos o verificados sobre el total de la imagen. |

## 7. Teclas de la TUI

| Tecla | Acción |
|---|---|
| `F5` | Volver a enumerar dispositivos. |
| `Tab` / `Shift+Tab` | Cambiar de panel. |
| `↑` / `↓` | Mover la selección en la lista. |
| `Enter` | Abrir la preparación del dispositivo seleccionado. |
| `F8` | Exportar la imagen sin escribir la USB. |
| `Esc` | Cerrar el diálogo o cancelar la preparación (con confirmación). |
| `F10` | Salir. |
| `Espacio` | Marca o desmarca la casilla de confirmación. |

## 8. Mensajes frecuentes

| Mensaje | Explicación |
|---|---|
| *"Se requieren privilegios para escribir /dev/sdX."* | La TUI se abrió sin `sudo`. |
| *"Este dispositivo no es una USB extraíble."* | La política rechazó un disco interno, NVMe, MMC o virtual. |
| *"No se pudo verificar la identidad del dispositivo."* | Falta serie o `by-id`, o el enlace no coincide. |
| *"El disco contiene el sistema operativo."* | El destino incluye `/` o `/boot`. No se puede forzar. |
| *"El dispositivo tiene particiones montadas."* | Hay que desmontarlas; la herramienta intenta hacerlo antes de escribir. |
| *"El dispositivo está en uso por LVM/RAID."* | `holders` no está vacío; se rechaza. |
| *"El sector lógico de N bytes no está soportado."* | El dispositivo no reporta 512 ni 4096 bytes lógicos. |
| *"La imagen no entra en el dispositivo."* | Capacidad insuficiente para GPT + ESP. |
| *"La relectura no coincide con la imagen."* | Falló la verificación; el medio quedó incompleto. |
| *"La USB se desconectó durante la escritura."* | `ENODEV`/`ENXIO` en la escritura o en la verificación. |
| *"Secure Boot puede impedir el arranque."* | El binario EFI no está firmado. |
| *"Preparación cancelada; la USB puede quedar incompleta."* | Se canceló después de empezar a escribir. |

## 9. Siglas de un vistazo

| Sigla | Expansión |
|---|---|
| **UEFI** | Unified Extensible Firmware Interface |
| **BIOS** | Basic Input/Output System |
| **CSM** | Compatibility Support Module |
| **ESP** | EFI System Partition |
| **GPT** | GUID Partition Table |
| **MBR** | Master Boot Record |
| **LBA** | Logical Block Address |
| **BPB** | BIOS Parameter Block |
| **FAT** | File Allocation Table |
| **GUID** | Globally Unique Identifier |
| **CRC** | Cyclic Redundancy Check |
| **SHA** | Secure Hash Algorithm |
| **PE** | Portable Executable |
| **OVMF** | Open Virtual Machine Firmware |
| **GOP** | Graphics Output Protocol |
| **MOK** | Machine Owner Key |
| **TUI** | Terminal User Interface |
| **GUI** | Graphical User Interface |
