# Arquitectura · Preparador UEFI con programa de arranque propio

Estado: **diseño para implementación**.
Documentos relacionados: [`Requerimientos.md`](Requerimientos.md) · [`Glosario.md`](Glosario.md) · [`diagrama.drawio`](diagrama.drawio).

---

## 1. Objetivo y principios

La aplicación prepara una memoria USB para que un equipo UEFI x86_64 ejecute un
programa propio sin cargar un sistema operativo. El preparador corre sobre Linux:
enumera dispositivos, valida el destino, construye una imagen GPT/FAT32 con el
programa EFI, la escribe, sincroniza y verifica. El programa EFI corre después en
el firmware y usa únicamente los servicios de UEFI.

Principios que gobiernan el diseño:

1. **Dos mundos separados.** El preparador usa syscalls de Linux; el programa de
   arranque usa `EFI_BOOT_SERVICES`. Comparten el formato del medio
   (constantes), no código ni cabeceras.
2. **La imagen se construye.** El preparador genera MBR protector, GPT, FAT32 y
   el árbol `EFI/BOOT`; no invoca `parted`, `mkfs.vfat` ni `dd`.
3. **El núcleo es headless.** `booteable_core` no incluye Guayaba ni Linux
   headers en `domain/`; se prueba contra archivos y dispositivos falsos.
4. **El éxito se verifica.** Escribir no es preparar: se hace `fsync`, se relee el
   dispositivo y se compara SHA-256. Sin verificación no hay éxito.
5. **El destino se comprueba dos veces.** La política de seguridad se evalúa al
   seleccionar y se revalida por identidad (serie, tamaño, `by-id`) justo antes de
   abrir y después de cerrar.
6. **La UI no decide seguridad.** La UI muestra el veredicto de
   `device/SafetyPolicy` y pide confirmación; no enumera `/sys`, no abre `/dev` y
   no construye geometrías.
7. **Nada de firmware ajeno.** No se firma, no se desactiva Secure Boot y no se
   modifican variables UEFI. Si Secure Boot bloquea el binario, se informa.
8. **Una cancelación no es un éxito.** El medio cancelado o interrumpido queda
   como `Incomplete`; nunca como `Verified`.

## 2. Decisiones de arquitectura (ADR)

| ADR | Decisión | Motivo | Consecuencia |
|---|---|---|---|
| 01 | UEFI x86_64, PE32+ | Es la ruta que admite el equipo de prueba y la más reproducible en QEMU/OVMF. | No hay BIOS/CSM; el binario es `BOOTX64.EFI`. |
| 02 | GPT con ESP FAT32 | Es el esquema estándar de UEFI y el que el firmware localiza sin configuración extra. | Hay que implementar GPT primaria, de respaldo y FAT32. |
| 03 | Ruta extraíble `\EFI\BOOT\BOOTX64.EFI` | El firmware la busca en medios extraíbles sin entradas NVRAM. | El nombre y la ruta son parte del contrato. |
| 04 | Imagen construida por código propio | El profesor permite librerías/APIs, pero detectar, particionar, formatear, escribir, sincronizar y verificar debe hacerlo nuestro código. El caso es acotado. | Se implementan `GptWriter`, `Fat32Writer` y verificadores; no se usan binarios externos. |
| 05 | Aplicación EFI sin EDK2 ni gnu-efi | Solo se necesitan tablas y protocolos básicos; MinGW-w64 ya está disponible. | Cabeceras EFI mínimas propias y compilación `-nostdlib`. |
| 06 | El binario EFI se incrusta en el preparador | Un solo ejecutable, sin depender de la ruta de `BOOTX64.EFI` en disco. | CMake genera un arreglo de bytes y su SHA-256 en tiempo de compilación. |
| 07 | Escritura en un hilo con estado y cancelación | Mantiene la TUI operativa sin el costo de un proceso hijo y su protocolo. | El controlador expone `poll()`; el hilo solo escribe y reporta progreso. |
| 08 | Verificación por relectura y SHA-256 | Detecta escrituras parciales, caché sin bajar y dispositivos que cambian. | La etapa de verificación puede tardar tanto como la escritura. |
| 09 | Política de seguridad pura | Se prueba sin hardware y se reutiliza en la CLI. | `SafetyPolicy` recibe `DeviceInfo` + `SystemContext`. |
| 10 | Solo FAT32 sin LFN | Los nombres `EFI`, `BOOT` y `BOOTX64.EFI` son 8.3; no hace falta Unicode. | Menos código y menos fuentes de error; nombres fijos en mayúsculas. |
| 11 | Sectores lógicos de 512 y 4096 B | Hay dispositivos 4Kn y el diseño puede parametrizar GPT, FAT y offsets con `BLKSSZGET`. | Se aceptan ambos; 4Kn exige una ESP mínima de 260 MiB por el mínimo de clústeres FAT32. |
| 12 | Sin firma Secure Boot | No se alteran claves ni opciones de seguridad. | El arranque físico requiere Secure Boot desactivado con autorización; la firma queda como trabajo futuro. |
| 13 | Imagen dispersa + manifiesto de regiones | Una USB real puede tener decenas de GB; no se reserva ese espacio en RAM ni se hashea el disco entero. | La imagen se guarda en un archivo disperso y la verificación compara solo las regiones con contenido. |

## 3. Vista de componentes

La página **1 · Componentes** muestra los módulos agrupados por mundo (proceso
Linux y firmware UEFI), el medio USB como puente y el contrato de formato como
pieza compartida. La página **2 · Clases** muestra los tipos y contratos. La
frontera entre el preparador y el firmware se resume así:

```text
┌──────────────────────── Linux · proceso booteable ─────────────────────────┐
│  ui/ (Guayaba) → IPrepController → app/PrepController                     │
│                                      ├── image/ BootImageBuilder           │
│                                      ├── device/ SafetyPolicy             │
│                                      ├── device/ SysfsBlockDeviceSource    │
│                                      └── device/ PosixDeviceWriter         │
│  core/ SHA-256 · CRC32 · SizeFormat      domain/ tipos y validación        │
└────────────────────────────────────────────────────────────────────────────┘
              │ escribe imagen raw               ▲ lee identidad y contenido
              ▼                                  │
      /dev/sdX  ── GPT + ESP FAT32 ── BOOTX64.EFI (incrustado)
                                              │
┌──────────────────────── Equipo de prueba · firmware UEFI ──────────────────┐
│  Firmware → EFI_BOOT_SERVICES → boot/efi_main → ConOut + SerialIo          │
└────────────────────────────────────────────────────────────────────────────┘
```

Responsabilidades:

| Módulo | Responsabilidad | No hace |
|---|---|---|
| `domain/` | Tipos, enums, validación pura | No lee `/sys`, no abre `/dev`, no dibuja |
| `core/` | SHA-256, CRC32, formato de tamaños, utilidades de bytes | No conoce particiones ni UI |
| `image/` | GPT, FAT32, ensamblado, parseo y verificación de imagen | No conoce dispositivos reales ni Guayaba |
| `device/` | Enumeración sysfs, montajes, política, escritura y relectura | No construye la imagen ni decide la UI |
| `app/` | Orquestación, estado, progreso, cancelación | No dibuja ni usa `ioctl` directamente |
| `ui/` | Lista, detalles, confirmación, progreso, resultado | No abre dispositivos ni calcula CRC |
| `cli/` | Entrada headless para `build`, `verify`, `flash` | No usa Guayaba |
| `boot/` | Aplicación EFI propia | No conoce Linux ni el preparador |

## 4. Programa de arranque UEFI

### 4.1 Entrada y servicios

`boot/main.cpp` define:

```cpp
extern "C" EFI_STATUS EFIAPI
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table);
```

La aplicación:

1. Guarda `ST` y `BS`; valida que `ConOut` y `ConIn` no sean nulos.
2. Limpia la consola y fija colores con `ConOut->SetAttribute`.
3. Imprime el mensaje del grupo y el identificador de compilación.
4. Escribe el mismo texto en el puerto serie si `SerialIo` está disponible.
5. Espera una tecla con `ConIn->ReadKeyStroke`, sondeando con `BS->Stall`
   durante 10 segundos; si no hay tecla, apaga igual.
6. Apaga el equipo con `ST->RuntimeServices->ResetSystem(EfiResetShutdown, ...)`.

Apagar es el comportamiento provisional mientras el resto del proyecto no esté
cerrado; si el firmware no implementa `ResetSystem`, se informa por consola y se
devuelve `EFI_SUCCESS`. No abre protocolos de disco, no escribe archivos y no
modifica variables. No usa `ExitBootServices`: la aplicación vive dentro del
entorno de arranque.

### 4.2 Salida visible e identificable

El mensaje es fijo y se compila con el binario:

```text
Universidad de la República — Facultad de Ingeniería
Sistemas Operativos

        U S B   B O O T E A B L E
        ARRANQUE UEFI PROPIO

Grupo: <nombre del grupo>
Programa: BOOTX64.EFI
Build: <hash corto>
BOOTEABLE-BOOT-OK
```

- `BOOTEABLE-BOOT-OK` es el marcador estable que usa la prueba QEMU.
- El hash corto se genera en CMake a partir de la fuente o del binario; permite
  distinguir compilaciones sin abrir el archivo.
- Los colores se eligen de la paleta de `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`
  (`EFI_LIGHTGRAY` sobre `EFI_BLUE`, por ejemplo). No se depende de GOP, por lo
  que funciona en consola de texto y en modo headless.
- El puerto serie se usa solo si `ST->ConOut` o `SerialIo` existen; la ausencia de
  serie no impide el arranque.

### 4.3 Compilación e incrustación

```bash
x86_64-w64-mingw32-g++ \
  -std=c++17 -ffreestanding -fno-exceptions -fno-rtti \
  -fno-stack-protector -fno-use-cxa-atexit -fno-threadsafe-statics \
  -mno-red-zone -nostdlib -Wl,--subsystem,10 -Wl,-e,efi_main \
  -o BOOTX64.EFI boot/main.cpp boot/efi.cpp
```

- `--subsystem,10` marca el PE32+ como *EFI application*.
- `-nostdlib` evita depender de `msvcrt`; se implementan `memcpy`, `memset` y
  comparaciones mínimas en `boot/efi.cpp`.
- `tools/build-efi.sh` es la única fuente de estos flags; CMake lo invoca.
- `tools/embed-efi.py` genera `build/generated/EmbeddedBootImage.cpp` con el
  arreglo `kBootEfi`, su tamaño, el SHA-256 y un *build id* corto; el preparador
  lo enlaza en `booteable_core`.
- El target `bootx64_efi` es verificable por separado (`file`, `objdump -p`).
- La ruta de arranque no se personaliza desde la TUI: cambiar el mensaje implica
  recompilar. Así se evita inyectar datos en un binario de arranque.

### 4.4 Límites del programa de arranque

- Solo modo texto UEFI; no hay drivers, teclado propio, red ni acceso directo al
  hardware.
- No se implementa un gestor de arranque ni se encadena otro binario.
- No se usa `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL`: el programa no necesita leer su
  propio archivo.
- Secure Boot: el PE no está firmado. Si el firmware tiene Secure Boot activo, no
  se ejecuta. La solución documentada es usar un equipo de laboratorio con
  autorización; la firma con clave propia y `MOK` es trabajo futuro.

## 5. Modelo de dominio

La página **2 · Clases** del diagrama muestra estos tipos. Son datos puros, sin
herencia ni dependencias de plataforma.

```cpp
namespace booteable {

enum class FirmwareTarget { Uefi };
enum class PartitionScheme { Gpt };
enum class FileSystemKind { Fat32 };
enum class DeviceTransport { Usb, Sata, Nvme, Mmc, Virtual, Unknown };
enum class PrepStage {
    Idle, Enumerating, Validating, Confirming, Building,
    Unmounting, Opening, Writing, Syncing, Verifying,
    Done, Failed, Cancelled, Incomplete
};
enum class PrepOutcome {
    None, Verified, VerificationFailed, Cancelled, Incomplete,
    PermissionDenied, InvalidTarget, Disconnected,
    InsufficientSpace, BuildError, DeviceError, UnmountError
};
enum class SafetyVerdict { Allowed, AllowedWithWarning, Rejected };

struct DevicePartition {
    std::string name;          // sdb1
    std::string mount_point;   // vacío si no está montada
    std::uint64_t size_bytes = 0;
};

struct DeviceInfo {
    std::string path;          // /dev/sdb
    std::string by_id;         // /dev/disk/by-id/usb-...
    std::string model;
    std::string serial;
    std::uint64_t size_bytes = 0;
    std::uint32_t logical_sector = 512;
    bool removable = false;
    DeviceTransport transport = DeviceTransport::Unknown;
    bool read_only = false;
    std::vector<DevicePartition> partitions;
};

struct SystemContext {
    std::string root_device;       // sda
    std::string boot_device;       // puede ser igual a root_device
    std::vector<std::string> mounted_device_names;
};

struct SafetyReport {
    SafetyVerdict verdict = SafetyVerdict::Rejected;
    std::vector<std::string> reasons;
    std::vector<std::string> warnings;
};

struct BootImageSpec {
    std::uint64_t disk_bytes = 0;      // tamaño exacto del destino
    std::uint64_t esp_bytes = 0;       // 0 = automático (64 MiB o 260 MiB)
    std::string volume_label = "BOOTEABLE";
    std::uint32_t logical_sector = 512; // 512 o 4096
};

struct ImageLayout {
    std::uint32_t sector_size = 512;
    std::uint64_t total_sectors = 0;
    std::uint64_t esp_first_lba = 2048;
    std::uint64_t esp_sectors = 0;
    std::uint64_t backup_header_lba = 0;
    std::uint64_t backup_entries_lba = 0;
    std::uint64_t first_usable_lba = 0;
    std::uint64_t last_usable_lba = 0;
    std::uint32_t entry_sectors = 32;
    std::uint32_t fat_sectors = 0;
    std::uint32_t cluster_count = 0;
    std::string efi_path = "EFI/BOOT/BOOTX64.EFI";
};

struct ValidationIssue { std::string field; std::string message; };
struct ValidationReport {
    std::vector<ValidationIssue> issues;
    bool ok() const { return issues.empty(); }
};

struct ImageRegion {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::string sha256;         // hash del contenido de la región
};

struct ImageManifest {
    std::uint64_t disk_bytes = 0;
    std::vector<ImageRegion> regions;   // MBR, GPT, ESP, GPT de respaldo
    std::string manifest_sha256;        // hash de la lista canónica
};

struct PrepConfig {
    DeviceInfo device;
    std::string image_path;     // vacío = archivo temporal disperso
    std::uint64_t esp_bytes = 0;                    // 0 = automático
    std::string volume_label = "BOOTEABLE";         // configurable
    bool overwrite_confirmed = false;               // casilla + Enter
};

struct PrepProgress {
    PrepStage stage = PrepStage::Idle;
    std::uint64_t bytes_done = 0;
    std::uint64_t bytes_total = 0;
    int percent = 0;
    std::string detail;
};

struct PrepResult {
    PrepOutcome outcome = PrepOutcome::None;
    PrepStage failed_stage = PrepStage::Idle;
    std::string message;
    std::string image_sha256;   // hash del manifiesto de la imagen
    std::string device_sha256;  // hash del manifiesto releído del dispositivo
    std::uint64_t bytes_written = 0;
    bool synchronized = false;
    bool verified = false;
};

} // namespace booteable
```

Reglas de validación en `domain/Validation.*`:

- `disk_bytes > 0` y múltiplo del sector lógico; `logical_sector` es 512 o 4096.
- `esp_bytes` configurable: `0` significa automático (64 MiB con 512, 260 MiB
  con 4096). Un valor explícito debe quedar entre el mínimo de 48/260 MiB y el
  máximo de 2 GiB, y ser múltiplo del sector lógico.
- Si la ESP automática no entra, `Validation` la reduce al mayor tamaño que quepa
  antes de la GPT de respaldo, igual que `ImageLayout`, siempre por encima del
  mínimo FAT32. Una ESP pedida explícitamente que no entra se rechaza.
- `esp_first_lba + esp_sectors + entry_sectors + 1 <= disk_bytes / logical_sector`
  (ESP, GPT de respaldo y alineación).
- `volume_label` configurable: 1–11 caracteres ASCII imprimibles, sin minúsculas
  ni espacios iniciales; se completa con espacios en el BPB. Por defecto
  `BOOTEABLE`.
- `overwrite_confirmed == true`; la UI solo lo activa con la casilla marcada y
  `Enter`.
- El tamaño del binario EFI incrustado debe ser menor que la ESP y mayor que 0.

## 6. Interfaces y adaptadores

```cpp
namespace booteable {

class IBlockDeviceSource {
public:
    virtual ~IBlockDeviceSource() = default;
    virtual std::vector<DeviceInfo> list() = 0;
    virtual std::optional<DeviceInfo> refresh(const std::string& path) = 0;
    virtual SystemContext system_context() = 0;
};

class IDeviceWriter {
public:
    virtual ~IDeviceWriter() = default;
    virtual bool open(const DeviceInfo& device, std::string& error) = 0;
    virtual bool write(const std::byte* data, std::size_t bytes,
                       std::string& error) = 0;
    virtual bool sync(std::string& error) = 0;
    virtual bool read_back(std::byte* data, std::size_t bytes,
                           std::string& error) = 0;
    virtual void close() = 0;

    // Extensiones compatibles de WP3b.
    virtual bool read_at(std::uint64_t offset, std::byte* data,
                         std::size_t bytes, std::string& error) {
        (void)offset; (void)data; (void)bytes;
        error = "El escritor no soporta lectura por offset.";
        return false;
    }
    virtual int last_errno() const { return 0; }
    virtual std::uint64_t size() const { return 0; }
    virtual std::uint32_t sector_size() const { return 0; }
    virtual std::uint64_t bytes_written() const { return 0; }
};

class IImageVerifier {
public:
    virtual ~IImageVerifier() = default;
    virtual bool verify(const std::filesystem::path& image,
                        ImageManifest& out, std::string& error) const = 0;
};

class IPrepController {
public:
    virtual ~IPrepController() = default;
    virtual std::vector<DeviceInfo> devices() = 0;
    virtual SafetyReport safety(const DeviceInfo&) = 0;
    virtual ValidationReport validate(const PrepConfig&) = 0;
    virtual bool start(const PrepConfig&) = 0;
    virtual void cancel() = 0;
    virtual PrepProgress progress() const = 0;
    virtual PrepResult result() const = 0;
    virtual bool running() const = 0;

    // Extensiones compatibles de WP4b: la UI no conoce root, sysfs ni /dev.
    virtual bool can_write() const { return true; }
    virtual std::string write_block_reason() const { return {}; }
    virtual bool export_image(const PrepConfig&, std::string& error) {
        error = "El controlador no permite exportar imágenes.";
        return false;
    }
};

} // namespace booteable
```

Adaptadores:

| Interfaz | Implementación real | Implementación de prueba |
|---|---|---|
| `IBlockDeviceSource` | `SysfsBlockDeviceSource` | `FakeBlockDeviceSource` (JSON/fixtures) |
| `IDeviceWriter` | `PosixDeviceWriter` | `FileDeviceWriter` / `FaultyDeviceWriter` |
| `IImageVerifier` | `ImageVerifier` | — (las pruebas corrompen la imagen y releen) |
| `IPrepController` | `PrepController` | `FakePrepController` |

`PrepController` se construye con las fábricas reales; la UI recibe solo
`IPrepController`. `booteable-image` compone las mismas piezas sin UI.

### 6.1 Cambios de contrato posteriores a WP0

WP0 congeló `src/domain/` y las interfaces de `src/app/`. El cierre de WP5
auditó las diferencias y todas son **aditivas y compatibles**; no se rompió
ninguna implementación existente:

| WP | Archivo | Cambio | Motivo |
|---|---|---|---|
| WP3a | `domain/Types.hpp` | `DevicePartition.dev_id`; `DeviceInfo.dev_id` y `holders` | Relacionar montajes y detectar LVM/RAID |
| WP3b | `device/IDeviceWriter.hpp` | `read_at`, `last_errno`, `size`, `sector_size`, `bytes_written` | Releer regiones, traducir errores y validar geometría |
| WP4b | `app/IPrepController.hpp` | `can_write`, `write_block_reason`, `export_image` | La UI no conoce `root`, `sysfs` ni `/dev` |

Los métodos nuevos tienen implementación por defecto, salvo los campos de
`DeviceInfo`/`DevicePartition`, que se inicializan solos y no cambian el uso
existente.

## 7. Formato del medio

### 7.1 Geometría

Sea `D` el tamaño exacto del destino en bytes, `S` el sector lógico (512 o 4096)
y `T = D / S` el total de LBA. La ESP ocupa `esp_sectors = esp_bytes / S`,
alineada a 1 MiB: empieza en `esp_first_lba = 1 MiB / S` (2048 con 512, 256 con
4096).

| Elemento | LBA |
|---|---|
| MBR protector | 0 |
| Cabecera GPT primaria | 1 |
| Entradas GPT primarias (128 × 128 B) | 2 … 2 + `entry_sectors` − 1 |
| Inicio de la ESP | `esp_first_lba` |
| Fin de la ESP | `esp_first_lba` + `esp_sectors` − 1 |
| Entradas GPT de respaldo | `T` − 1 − `entry_sectors` … `T` − 2 |
| Cabecera GPT de respaldo | `T` − 1 |

`entry_sectors = ceil(128 × 128 / S)` es 32 con sector de 512 y 4 con 4096.
`first_usable_lba = 2 + entry_sectors` y
`last_usable_lba = T - 2 - entry_sectors`. Si el destino es una USB de
mayor capacidad que la imagen de prueba, la TUI construye la imagen con
`disk_bytes` igual a la capacidad real, de modo que la GPT de respaldo quede en
el último LBA físico. Para QEMU/CLI se usa `--size-mb` y ese es el tamaño del
archivo.

Cuando `esp_bytes = 0` y el valor por defecto no entra en un disco pequeño
(por ejemplo, una imagen de 64 MiB con ESP automática de 64 MiB), el tamaño se
reduce al mayor que quepa antes de la GPT de respaldo, siempre por encima del
mínimo FAT32. Si el usuario pide `--esp-mib` explícito y no entra, el build
falla: no se recorta una elección explícita.

El archivo se crea con `ftruncate(disk_bytes)` y solo se escriben las regiones
con contenido (MBR, GPT primaria, ESP y GPT de respaldo). En un sistema de
archivos con soporte de *sparse files* una imagen de 64 GB ocupa unos pocos MiB;
el resto se lee como ceros. Nunca se reserva el tamaño completo en RAM. Para
QEMU, el archivo raw disperso se comporta como un disco del tamaño declarado.

### 7.2 MBR protector

- Firma `0x55AA` en el byte 510.
- Una entrada tipo `0xEE`, estado `0x00`, CHS de inicio `0x000200`, CHS de fin
  `0xFFFFFF`, `start_lba = 1`, `sectors = min(T - 1, 0xFFFFFFFF)`.
- Las otras tres entradas quedan en cero.
- No hay MBR híbrido ni código de arranque BIOS.

### 7.3 GPT

Cabecera (92 bytes útiles, resto del sector en cero):

```text
signature          "EFI PART"
revision           0x00010000
header_size        92
header_crc32       CRC32 de los primeros 92 bytes con este campo en cero
reserved           0
current_lba        1 (primaria) · T-1 (respaldo)
backup_lba         T-1 (primaria) · 1 (respaldo)
first_usable_lba   2 + entry_sectors  (34 con 512, 6 con 4096)
last_usable_lba    T - 2 - entry_sectors
disk_guid          GUID aleatorio por imagen (v4)
entries_lba        2 (primaria) · T-1-entry_sectors (respaldo)
num_entries        128
entry_size         128
entry_sectors      ceil(128 × 128 / S)  (32 con 512, 4 con 4096)
entries_crc32      CRC32 del arreglo completo de 16 KiB
```

Entrada 0 de la ESP:

```text
type_guid      C12A7328-F81F-11D2-BA4B-00A0C93EC93B
unique_guid    GUID aleatorio
first_lba      esp_first_lba (2048 con 512, 256 con 4096)
last_lba       esp_first_lba + esp_sectors - 1
attributes     0
name           "BOOTEABLE" en UTF-16LE
```

Las demás entradas quedan en cero. El CRC32 es el de IEEE 802.3 (polinomio
`0xEDB88320` reflejado), el mismo que usa UEFI; se implementa en
`core/Crc32.*` y se prueba con vectores conocidos.

### 7.4 ESP FAT32

Parámetros:

| Campo | Valor |
|---|---|
| `bytes_per_sector` | `S` (512 o 4096) |
| `sectors_per_cluster` | 1 |
| `reserved_sectors` | 32 |
| `num_fats` | 2 |
| `root_entries` | 0 (FAT32) |
| `media` | `0xF8` |
| `total_sectors_32` | `esp_sectors` |
| `hidden_sectors` | `esp_first_lba` |
| Etiqueta | `volume_label` (por defecto `BOOTEABLE`) |

Cálculo de la FAT, iterando hasta estabilizar:

```text
fat_sectors = ceil((cluster_count + 2) * 4 / S)
data_sectors = total_sectors - reserved_sectors - num_fats * fat_sectors
cluster_count = floor(data_sectors / sectors_per_cluster)
```

Se exige `cluster_count >= 65525` (mínimo FAT32) y `fat_sectors < 65536`. Con
sector de 512 el mínimo práctico es **48 MiB**; con 4096, **260 MiB**. El tamaño
por defecto es 64 MiB en 512 y 260 MiB en 4Kn; la CLI permite elegir otro
siempre que cumpla el mínimo. La TUI usa el valor automático.

Contenido:

- Sector 0: boot sector FAT32 (salto `EB 3C 90`, OEM `BOOTEABL` de 8 bytes, BPB completo,
  firma `0x55AA`).
- Sector 1: `FSInfo` con firmas `0x41615252` / `0x61417272` / `0xAA550000`, cantidad
  de clústeres libres y próximo libre.
- Sector 6: copia del boot sector. Sector 7: copia del `FSInfo`.
- FAT[0] = `0x0FFFFFF8`, FAT[1] = `0x0FFFFFFF`; ambas tablas se escriben
  idénticas.
- Directorio raíz en el clúster 2.

Árbol y cadena de clústeres:

```text
clúster 2  /            → entrada "EFI"       (attr 0x10)
clúster 3  /EFI         → entrada "BOOT"      (attr 0x10)
clúster 4  /EFI/BOOT    → entrada "BOOTX64 EFI" (attr 0x20, tamaño = sizeof(efi))
clúster 5+ /EFI/BOOT/BOOTX64.EFI  → contenido, contiguo, último clúster 0x0FFFFFFF
```

No se implementan nombres largos: `EFI`, `BOOT` y `BOOTX64.EFI` entran en 8.3.
Las fechas y horas de las entradas son una constante fija (`2026-01-01 00:00:00`)
para que dos compilaciones de la misma entrada produzcan la misma imagen. Solo
cambian los GUID aleatorios si se pide una imagen nueva; para pruebas
reproducibles se puede fijar una semilla.

### 7.5 Verificación de la imagen

`ImageVerifier` relee el archivo o dispositivo y comprueba, en este orden:

1. Firma del MBR, entrada `0xEE` y `0x55AA`.
2. Cabecera GPT primaria: firma, revisión, tamaño, CRC de cabecera y de
   entradas.
3. Cabecera de respaldo: mismos CRC y coherencia con la primaria.
4. Entrada de la ESP: tipo, límites dentro de `first/last_usable_lba`.
5. BPB FAT32: firma, parámetros esperados, `cluster_count` coherente.
6. Ambas FAT idénticas, cadena de `EFI/BOOT/BOOTX64.EFI` completa y sin ciclos.
7. Contenido del archivo: tamaño y SHA-256 iguales a `kBootEfiSha256`.
8. Cada región del `ImageManifest` (MBR, GPT primaria, ESP, GPT de respaldo)
   coincide con su SHA-256 y el hash canónico del manifiesto coincide con el
   calculado al construirla.

La verificación de la imagen **no** reemplaza la verificación del dispositivo:
tras escribir se relee el dispositivo y se comparan las mismas regiones contra
el manifiesto. No se recorre el disco entero: solo lo que la imagen define, de
modo que una USB de 64 GB no multiplica el tiempo de verificación por su tamaño
libre. Las herramientas externas (`fsck.vfat -n`, `sgdisk -v`) pueden usarse en
las pruebas como control cruzado, nunca en tiempo de ejecución.

## 8. Flujo de preparación

La página **3 · Secuencia** muestra el camino feliz y la cancelación.

```text
UI            Controller        ImageBuilder       DeviceWriter      Device
│ seleccionar │                    │                   │                │
│────────────>│ enumerar/refrescar │                   │                │
│             │───────────────────────────────────────────────────────>│ sysfs
│<────────────│ DeviceInfo         │                   │                │
│ confirmar   │                    │                   │                │
│────────────>│ validar + política │                   │                │
│             │ build(spec)        │                   │                │
│             │───────────────────>│ GPT+FAT32+EFI     │                │
│             │<───────────────────│ imagen + manifest │                │
│             │ unmount            │                   │                │
│             │───────────────────────────────────────>│ umount2        │
│             │ open(device)       │                   │                │
│             │───────────────────────────────────────>│ O_RDWR|O_EXCL  │
│             │ write chunks       │                   │                │
│             │───────────────────────────────────────>│ write()        │
│             │ progress           │                   │                │
│             │ sync               │                   │                │
│             │───────────────────────────────────────>│ fsync          │
│             │ verify             │                   │                │
│             │───────────────────────────────────────>│ read()         │
│             │<───────────────────────────────────────│ sha256         │
│<────────────│ PrepResult         │                   │                │
```

Detalle por etapa:

| Etapa | Acción | Si falla |
|---|---|---|
| `Validating` | `Validation` + `SafetyPolicy` + relectura de identidad | `InvalidTarget`, con motivos |
| `Building` | Construir la imagen dispersa y su `ImageManifest` con hashes por región | `BuildError` |
| `Unmounting` | `umount2(MNT_DETACH)` de cada partición montada | `UnmountError`, sin escribir |
| `Opening` | `open` con `O_RDWR|O_CLOEXEC|O_EXCL` y `ioctl(BLKGETSIZE64)` | `PermissionDenied` / `DeviceError` |
| `Writing` | Bloques de 1 MiB; comprobar cancelación entre bloques | `DeviceError` / `Disconnected` |
| `Syncing` | `fsync` y `ioctl(BLKFLSBUF)` | `DeviceError`; no verificar |
| `Verifying` | Reabrir en solo lectura, releer las regiones y comparar el manifiesto | `VerificationFailed` |
| `Done` | Revalidar identidad y cerrar | Si cambió: `Disconnected` |

Antes de abrir se compara la identidad actual (`path`, `serial`, `by_id`,
`size_bytes`) con la foto tomada al seleccionar. Después de cerrar se vuelve a
comparar; un cambio produce `Disconnected` y el resultado no es `Verified`.

### 8.1 Cancelación

- Antes de `Writing`: se aborta sin escribir en el dispositivo (puede haberse desmontado).
- Durante `Writing`: se termina el bloque en curso, se intenta `fsync` y se
  cierra; el resultado es `Cancelled` con `Incomplete = true`.
- Durante `Verifying`: se puede cancelar; el resultado es `Cancelled` e
  `Incomplete`, aunque la escritura haya terminado.
- El token de cancelación es un `std::atomic_bool` que `PrepController` pasa a
  `PrepRunner`; `DeviceFlasher` lo consulta entre bloques de 1 MiB.

## 9. Máquina de estados

La máquina de estados es la siguiente; la página **4 · Estados** la representa
junto con los terminales de error y cancelación.

```text
Idle → Enumerating → Validating → Confirming → Building
     → Unmounting → Opening → Writing → Syncing → Verifying → Done
                         │        │         │
                         └────────┴─────────┴──→ Failed / Cancelled / Incomplete
```

- `Done` solo se alcanza con `PrepOutcome::Verified`.
- `Failed` agrupa errores con su etapa y mensaje.
- `Incomplete` es el estado terminal de una cancelación o interrupción; la UI lo
  muestra en ámbar y recomienda volver a preparar la USB.
- Volver a `Idle` exige una nueva enumeración y una nueva confirmación; no se
  reutiliza una confirmación anterior.

## 10. Capa de dispositivos Linux

### 10.1 Enumeración

`SysfsBlockDeviceSource` recorre `/sys/block` y, para cada entrada:

- Descarta `loop*`, `ram*`, `zram*`, `dm-*`, `md*`, `sr*`, `fd*`.
- Lee `removable`, `size` (en sectores de 512), `ro`, `device/model`,
  `device/serial`, `device/uevent` y la ruta real del enlace para detectar
  `/usb` en la cadena.
- Resuelve `by_id` buscando el enlace `/dev/disk/by-id/usb-*` cuyo destino sea el
  mismo nodo (comparando `realpath`).
- Lee las particiones desde `/sys/block/<dev>/<dev>*` y sus puntos de montaje
  desde `/proc/self/mountinfo` (campos 5 y 10: punto de montaje y `major:minor`).
- Comprueba `holders` para detectar uso por `dm`/RAID.

`SystemContext` obtiene el disco raíz y el de `/boot` desde
`/proc/self/mountinfo` y `stat("/")`/`stat("/boot")`, convirtiendo `major:minor`
al nombre `sdX`/`nvmeXnY`.

### 10.2 Política de seguridad

`SafetyPolicy::evaluate(device, context)` devuelve `Allowed`,
`AllowedWithWarning` o `Rejected` con motivos. Reglas de rechazo:

| Regla | Motivo |
|---|---|
| `transport != Usb` | No es un dispositivo USB. |
| `removable == false` | El firmware/el usuario pueden confundirlo con un disco fijo. |
| `path` es el disco raíz o contiene `/boot` | Es el disco del sistema. |
| `serial` vacío o `by_id` ausente | Identidad no comprobable. |
| `read_only` | El medio está bloqueado. |
| `logical_sector` distinto de 512 o 4096 | Geometría no soportada. |
| `size_bytes < mínimo` | No entra la ESP ni la GPT. |
| Tiene `holders` (`dm`, `md`, LVM) | Está en uso por otro subsistema. |
| Cualquier partición montada | Hay que desmontarla y el usuario debe confirmarlo. |
| `path` no existe o no es un dispositivo de bloque | Destino inválido. |

Advertencias (no bloquean): dispositivo USB con otras particiones, etiqueta
previa distinta de `BOOTEABLE`, o más de una USB conectada.

La política se ejecuta dos veces: al listar y al iniciar. La UI no la replica.

### 10.3 Desmontaje

`MountManager`:

1. Relee `/proc/self/mountinfo` y filtra las entradas cuyo `major:minor`
   pertenece al disco o a sus particiones.
2. Ordena de la ruta más profunda a la menos profunda.
3. Llama `umount2(mount_point, MNT_DETACH)`.
4. Relee `mountinfo` y confirma que no quedan montajes.
5. Si queda alguno, devuelve `UnmountError` y **no** abre el dispositivo.

No se usa `system("umount")`. Si el usuario no quiere desmontar, cancela.

### 10.4 Escritura y relectura

`PosixDeviceWriter`:

```text
fd = open(dev, O_RDWR | O_CLOEXEC | O_EXCL)
ioctl(fd, BLKGETSIZE64, &bytes)     // tamaño real
ioctl(fd, BLKSSZGET, &sector)       // 512 o 4096; se ajusta la imagen
write(fd, chunk, 1 MiB)             // bucle hasta completar; EINTR reintenta
fsync(fd)
ioctl(fd, BLKFLSBUF)                // descarta caché del dispositivo
close(fd)

fd = open(dev, O_RDONLY | O_CLOEXEC)
for region in manifest.regions:
    pread(region.offset, 1 MiB) + SHA-256
close(fd)
```

- `O_EXCL` falla con `EBUSY` si el dispositivo está montado o en uso; es una
  segunda barrera además del desmontaje.
- Un `write` corto no es error: se continúa desde el offset pendiente.
- `ENOSPC`, `EIO`, `ENODEV`, `ENXIO` y `EPERM` se traducen a resultados de
  dominio, no a excepciones que lleguen a la UI.
- La relectura recorre solo las regiones del manifiesto, no la capacidad libre
  de la USB. `pread` permite leer en el offset exacto sin recorrer huecos.
- El `fsync` y el `BLKFLSBUF` garantizan que la comparación no lea de caché del
  host.

`booteable-image flash` usa el mismo par escritor/verificador y acepta
`--device` y `--yes`; sin `--yes` pide confirmación explícita antes de abrir.
Antes de escribir comprueba que el `logical_sector` y el tamaño de la imagen
coinciden con el dispositivo; si no, rechaza y sugiere `--sector-size`/`--size-mb`.

## 11. Seguridad y confirmación

- **Privilegios.** La TUI arranca como usuario normal. Si `geteuid() != 0`,
  enumera y permite `build`/`verify`, pero el botón de escritura queda
  deshabilitado con el mensaje: *"Se requieren privilegios para escribir
  (/dev/sdX). Ejecutá `sudo ./build/booteable`."* No se usa `setuid` ni se
  invocan ayudantes externos en el MVP.
- **Confirmación.** El diálogo muestra ruta, modelo, serie, capacidad, imagen,
  etiqueta y SHA-256, y exige marcar una casilla de advertencia y pulsar
  `Enter`. Sin la casilla, el botón permanece deshabilitado.
- **Doble comprobación.** La identidad se revalida antes de `open` y después de
  `close`. Un cambio de serie o tamaño cancela la operación.
- **Sin discos del sistema.** Se rechaza el disco que contiene `/` y `/boot`,
  aunque el usuario insista; no hay opción de forzar.
- **Sin escritura parcial silenciosa.** Cualquier error deja `Incomplete` o
  `Failed` con la etapa exacta. La UI nunca muestra "verificado" si el SHA-256
  del dispositivo difiere.
- **Secure Boot.** Se detecta si `/sys/firmware/efi` existe y se muestra una nota
  en el resultado. La herramienta no lee ni modifica variables de Secure Boot;
  solo advierte que un binario sin firma no arrancará con esa opción activa.

## 12. Interfaz TUI

### 12.1 Escenas

El formato `.gscene` admite un solo nodo raíz por archivo y no tiene `include`
ni composición declarativa. Por eso la estructura se reparte en seis escenas:

| Archivo | Root | Se injerta en |
|---|---|---|
| `booteable.gscene` | `content` (shell) | raíz de `AdaptiveScreen` |
| `booteable_rail.gscene` | `rail` | `app_frame/rail_slot` |
| `booteable_main.gscene` | `main` | `app_frame/shell/stage_slot` |
| `booteable_steps.gscene` | `steps` | `app_frame/shell/stage_slot` |
| `booteable_confirm.gscene` | `confirm` | `app_frame/shell/stage_slot` |
| `booteable_result.gscene` | `result` | `app_frame/shell/stage_slot` |

`tools/embed-ui.py` incrusta todas las escenas y un único
`assets/ui/booteable.gtheme`. `AppScene` construye el tema una vez, construye
cada root con ese tema y los injerta bajo los slots. Los textos dinámicos, las
señales, las transiciones y el refresco con `IPrepController` siguen en C++.

El wizard tiene seis pasos: **Detectar → Particionar → Formatear → Escribir →
Sincronizar → Verificar**. El paso 1 es la selección; los pasos 2–6 se recorren
con `[ Continuar ]`, que solo se habilita cuando la etapa real terminó.

Hay una única pausa real: después de construir y verificar la imagen, antes de
abrir la USB. Ahí el worker espera a que el usuario pulse
`[ Continuar y escribir ]`; la USB sigue intacta y cancelar no deja nada a
medias. La escritura, el `fsync` y la verificación no se pausan: son una sola
unidad de durabilidad. La UI los muestra en vivo y, si terminan rápido, el
usuario puede leerlos igual con `[ Continuar ]`. Un fallo o una cancelación salta
directo al resultado.

`MainScene` (paso 1, rail plegado):

```text
┌─┬ booteable | Preparador UEFI ─────────────────────────────────────────────┐
│E│ USB detectadas: 2                    │ /dev/sdb · USB Flash Disk          │
│A│ ▸ /dev/sdb  16.0 GiB  [USB]          │ Serie: 0001                        │
│S│   /dev/sdc   2.0 GiB  [USB]          │ Montajes: /media/usb (sdb1)        │
│ │                                      │ Seguridad: APTO                    │
│ │                                      │ Imagen: GPT · ESP 64 MiB           │
│ │                                      │ [ Preparar /dev/sdb ]              │
├─┴──────────────────────────────────────┴────────────────────────────────────┤
│ E exportar · A actualizar · S salir · F9 desplegar                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

El rail es la única acción global: se pliega a 5 columnas (`[E]`, `[A]`, `[S]`
con una celda libre a cada lado) y se despliega a 18 (`[ Exportar ]`,
`[ Actualizar ]`, `[ Salir ]` alineados a la izquierda). El botón `[>]` / `[<]`
o `F9` lo pliega; `F8` abre la página de exportación. El contenido ocupa el resto
del ancho y `AdaptiveScreen` decide el layout con `ancho_terminal - ancho_rail`.

Página de exportación del rail:

```text
┌──────────────────┬ booteable | Preparador UEFI ────────────────────────────┐
│ HERRAMIENTAS     │ USB detectadas: 2        │ /dev/sdb · USB Flash Disk      │
│ [ Exportar     ] │ ▸ /dev/sdb  16.0 GiB     │ Serie: 0001                    │
│   [ 64 MiB ]     │   /dev/sdc   2.0 GiB     │ Seguridad: APTO                │
│   [ /dev/sdb ]   │                          │ Imagen: GPT · ESP 64 MiB       │
│ [ Actualizar   ] │                          │ [ Preparar /dev/sdb ]          │
│ [ Salir        ] │                          │                                │
└──────────────────┴──────────────────────────┴────────────────────────────────┘
```

`[ 64 MiB ]` exporta una imagen estándar de 64 MiB/512 independiente de la USB.
`[ /dev/sdb ]` replica tamaño, sector y etiqueta del dispositivo elegido; queda
deshabilitado si no hay selección. La exportación no modifica la USB.

`ConfirmScene` es una pantalla completa, no un overlay: resumen del destino,
plan de imagen, advertencia en rojo, `CheckBox`
*"Entiendo que se perderá todo el contenido de /dev/sdX"* y botones
`[ Volver ]` / `[ Sobrescribir y preparar ]`. Trabaja sobre una instantánea del
dispositivo, que se revalida antes de escribir.

`StepScene` usa dos ranuras (`step_a`, `step_b`) para que el cambio de paso no
tenga un hueco:

```text
┌─┬ PASO 3 DE 6 ─────────────────────────────────────────────────────────────┐
│X│ Formatear                                                              │
│S│ FASE 1 · CONSTRUIR IMAGEN · la USB todavía no se modifica              │
│ │ Crear la ESP FAT32 y copiar EFI/BOOT/BOOTX64.EFI al árbol 8.3.         │
│ │ [|] Generando FAT32…                                                   │
│ │ ████████████████░░░░░░░░  42 %                                         │
│ │ [##>---]                                                               │
│ │ Validando destino e imagen                                             │
│ │ Construyendo la imagen…                                                │
│ │ [ Continuar ]   [ Esc · Cancelar ]                                     │
└─┴────────────────────────────────────────────────────────────────────────┘
```

Cuando la imagen está lista y el worker espera, la tarjeta de Formatear muestra
`Imagen lista; esperando Confirmar` y el botón cambia a
`[ Continuar y escribir ]`. Ese es el único punto donde la UI detiene el
pipeline. Al final, la tarjeta de Verificar cambia a `[ Ver resultado ]`.

Los pasos 2 (Particionar) y 3 (Formatear) no tienen señal propia en el
controlador: `Building` cubre ambos. La UI los separa con un temporizador de
~600 ms, pero cualquier etapa real (`Writing`, `Syncing`, `Verifying`) pisa el
temporizador de inmediato y un paso nunca se marca como hecho antes de que
`Building` termine. Si el build es más rápido que el umbral, Formatear se salta
solo como temporizador; el usuario lo lee igual porque la tarjeta no avanza
sola.

`ResultScene`: resultado (`VERIFICADO` en verde, `INCOMPLETO` en ámbar,
`FALLÓ` en rojo), paso y etapa, mensaje, SHA-256 de imagen y dispositivo, y las
instrucciones de prueba. Si falla `Building`, el resultado informa
“Preparar imagen (Particionar / Formatear)”, sin inventar cuál de los dos falló.

### 12.2 Reglas de UI

- La UI solo incluye `app/IPrepController.hpp` y `src/domain/Types.hpp`.
- La estructura y el tema son declarativos (`assets/ui/*.gscene` y
  `booteable.gtheme`); el comportamiento y los bindings de señales quedan en
  `src/ui/`. La composición de escenas se hace en C++, no en el formato.
- `MainScene` refresca con `F5` y al recibir el evento de cambio de dispositivo;
  no usa sondeo continuo en el panel de selección.
- `AppScript` llama a `AppScene::animate(delta)` en cada frame para la
  transición y el temporizador de pasos; consulta `progress()` cada 100 ms. El
  hilo de escritura solo actualiza un `std::atomic` y un mutex corto.
- La transición es un deslizamiento de ~180 ms sobre `anchor_left` /
  `anchor_right`; al terminar queda un solo panel visible en el escenario.
- La confirmación es una pantalla propia y usa la instantánea del dispositivo,
  revalidada antes de escribir.
- `Esc` vuelve de la confirmación y del resultado; en los pasos equivale a
  `Cancelar`; en el paso 1 pliega el rail y después sale. `F10` sale salvo
  durante la preparación; si la preparación ya terminó y el resultado está
  listo, sale.
- `Continuar` solo se habilita cuando la etapa real terminó. Las tarjetas no
  avanzan solas; el pipeline sigue en segundo plano y la UI se pone al día.
- El único corte real es después de construir/verificar la imagen y antes de
  escribir. `PrepController` expone `waiting_for_continue()` y
  `continue_preparation()`; cancelar despierta la espera y no abre la USB.
- Los pasos 2–3 son estimados por tiempo con guardas: la etapa real manda y no
  se muestra “verificado” ni un paso completo antes de tiempo.
- Un fallo o una cancelación muestran el resultado de inmediato, sin obligar a
  recorrer las tarjetas restantes.
- El rail guarda su estado plegado/desplegado entre pantallas. En los pasos 2–6
  solo ofrece Cancelar mientras la preparación corre; Salir queda deshabilitado
  hasta que termina.
- A 80×24 los paneles se apilan; a 120×40 se usa el layout de dos columnas. El
  breakpoint se calcula sobre el ancho del escenario, no sobre el terminal.
- Los mensajes de error muestran la etapa y el `errno` traducido, nunca un
  "éxito" parcial.
- No hay lógica de negocio en los nodos: los `LineEdit`/`Button` emiten señales
  y `PrepController` decide.

## 13. Errores y mensajes

| Situación | `PrepOutcome` | Mensaje de la UI |
|---|---|---|
| `open` devuelve `EACCES`/`EPERM` | `PermissionDenied` | "Sin permisos para escribir /dev/sdX. Ejecutá con sudo." |
| Destino no USB o sin serie | `InvalidTarget` | Motivo concreto de `SafetyReport.reasons` |
| `ENOSPC` o tamaño menor que la imagen | `InsufficientSpace` | Capacidad necesaria y disponible |
| `ENODEV`/`ENXIO` durante escritura | `Disconnected` | "La USB se desconectó durante la escritura." |
| `EIO` o `write` corto irrecuperable | `DeviceError` | "Falló la escritura en la etapa X." |
| Montaje que no se puede desmontar | `UnmountError` | Punto de montaje y proceso que lo usa, si se conoce |
| SHA-256 distinto | `VerificationFailed` | "La relectura no coincide. El medio quedó incompleto." |
| Cancelación | `Cancelled` | "Preparación cancelada; la USB puede quedar incompleta." |
| Fallo al construir GPT/FAT | `BuildError` | Detalle de validación o tamaño |

El mensaje siempre incluye: dispositivo, etapa y resultado. Los detalles técnicos
(`errno`) se muestran en una línea secundaria, no se ocultan.

## 14. Estrategia de pruebas

No se agrega un framework externo: `tests/TestHarness.hpp` con `CHECK`/`CHECK_EQ`
y objetivos de CTest, igual que los proyectos de referencia.

| Prueba CTest | Tipo | Qué verifica |
|---|---|---|
| `test_core` | unitaria | Vectores conocidos de CRC32 y SHA-256 |
| `test_gpt` | unitaria | MBR protector, GPT primaria/respaldo, CRC, GUID y límites con 512 y 4096 |
| `test_fat32` | unitaria | BPB, FSInfo, FAT duplicada, cadena y archivo 8.3, con sector 512 y 4096 |
| `test_image_roundtrip` | unitaria | Construir → verificar → corromper → fallar |
| `test_layout` | unitaria | Cálculos de FAT, clústeres, alineación y mínimos de 48/260 MiB |
| `test_validation` | unitaria | Etiqueta, casilla, ESP mínima por sector, reducción automática y tamaño del EFI |
| `test_safety_policy` | unitaria | Raíz, disco interno, sin serie, montado, 4Kn aceptado, sector raro, LVM, capacidad |
| `test_sysfs_source` | unitaria | Enumeración sysfs con fixtures |
| `test_mount_manager` | unitaria | Montajes, desmontaje y relectura de `mountinfo` |
| `test_device_writer` | integración | Escritura en archivo regular y dispositivo falso; relectura y SHA |
| `test_flash` | integración | Escritura, desconexión, cancelación (`Incomplete`), tamaño/sector y desmontaje |
| `test_prep_controller` | integración | Hilos, privilegios, exportación, cancelación y ausencia de descriptores colgados |
| `test_ui_flow` | unitaria | Escenas, confirmación y layouts a 80×24 y 120×40 |
| `test_fake_controller` | unitaria | Flujo completo con el controlador falso |
| `efi_binary` | integración | Cabecera PE32+, subsistema EFI y ausencia de imports de Windows |
| `image_cli` | integración | `build`/`verify` en 512 y 4096, corrupción y `fsck.vfat` opcional |
| `flash_cli` | integración | `flash` sobre archivo regular, rechazos y confirmación |
| `qemu_boot` / `qemu_boot_image` / `qemu_boot_4kn` | integración opcional | QEMU + OVMF y `BOOTEABLE-BOOT-OK` en serie; se saltan si faltan |
| `qemu_secure_boot` | integración opcional | El PE sin firma no arranca con Secure Boot |
| Aceptación manual | manual | P1–P4, P9 y P10 de `Requerimientos.md` |

Criterios transversales:

- `ctest -j4 --output-on-failure` pasa sin red, sin root y sin USB real en menos
  de 2 minutos en un equipo de 4 núcleos. `tests/main.cpp` acepta una suite por
  argumento y CTest registra cada una por separado.
- La prueba QEMU detecta la ausencia de OVMF (`OVMF_CODE_4M.fd`,
  `OVMF_CODE.fd` o `OVMF_CODE`/`OVMF_VARS`) y se salta con un mensaje, no falla.
- Las cuatro pruebas QEMU comparten las variables de OVMF y el log serie; se
  serializan con `RESOURCE_LOCK qemu_ovmf` aunque CTest use `-j4`.
- Ningún test deja archivos grandes en el repositorio; usan `build/` o
  `std::filesystem::temp_directory_path()`.
- La imagen de prueba por defecto es de 64 MiB + overhead; el test de tamaño
  máximo usa un archivo disperso, no RAM real.

## 15. Mapeo de requisitos

| Requerimiento | Componente | Evidencia |
|---|---|---|
| RF-01/RF-02 código propio sin SO | `boot/` | `efi_main`, binario PE32+, sin libc |
| RF-03 resultado visible | `boot/main.cpp` | Mensaje + `BOOTEABLE-BOOT-OK` en consola y serie |
| RF-04 no destructivo | `boot/` | Sin protocolos de disco ni escrituras |
| RF-05 enumeración | `SysfsBlockDeviceSource` | Lista con modelo, serie, tamaño, `by-id`, montajes |
| RF-06 destino seguro | `SafetyPolicy` | P2 y `test_safety_policy` |
| RF-07 confirmación | `ConfirmScene` | P3 y captura de la UI |
| RF-08 construcción propia | `image/` | `booteable-image build` sin herramientas externas |
| RF-09 formato coherente | `GptWriter`, `Fat32Writer` | `test_gpt`, `test_fat32` |
| RF-10 desmontaje | `MountManager` | `test_device_writer` con montaje falso |
| RF-11 progreso | `PrepController`, `StepScene` | Etapas y porcentaje en vivo |
| RF-12 cancelación | `std::atomic_bool` + `PrepController` | `test_flash` y `test_prep_controller` |
| RF-13 verificación | `DeviceWriter` + `ImageVerifier` | SHA-256 releído y CRC/FAT |
| RF-14 errores | `PrepOutcome` + UI | Tabla §13 y P10 |
| RF-15 prueba virtual | `tools/run-qemu.sh` | `test_qemu_boot` |
| RF-16 arranque físico | USB preparada | P9 |

## 16. Riesgos y no objetivos

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Secure Boot activo | El binario no arranca | Documentar; probar en equipo de laboratorio con autorización; firma como trabajo futuro |
| Firmware estricto con FAT32 | La ESP no se monta | Respetar BPB, FSInfo, dos FAT y copia de boot; validar con `fsck.vfat -n` en tests |
| USB 4Kn con ESP menor a 260 MiB | FAT32 inválida | Validar el mínimo por sector y ajustar el tamaño por defecto |
| USB desconectada a mitad | Medio corrupto | `Disconnected`, `Incomplete`, no verificar |
| Caché del host | Falsa verificación | `fsync` + `BLKFLSBUF` + relectura |
| Identidad cambiada (`/dev/sdb` reutilizado) | Escribir el disco equivocado | Comparar serie y `by_id` antes y después |
| Montaje automático durante la escritura | `EBUSY` o datos inconsistentes | Desmontar y abrir con `O_EXCL`; revalidar |
| Imagen del tamaño de la USB | Consumo excesivo de RAM o disco | Archivo disperso con `ftruncate`; solo se escriben y verifican las regiones del manifiesto |
| QEMU/OVMF ausente | Sin prueba virtual | Test opcional que se salta; documento con comandos |
| Equipo de prueba sin UEFI | No se puede demostrar | Confirmar el firmware antes de la entrega; BIOS queda fuera de alcance |
| Cambios de alcance | Recortes | Prioridad: RF-01..RF-14; la firma Secure Boot queda como trabajo futuro |

No objetivos del MVP:

- BIOS/CSM, ARM, RISC-V, 32 bits.
- Copiar ISOs, multiboot, persistencia, instalación de SO.
- Firma Secure Boot, MOK, `sbsign`.
- Imágenes NTFS/exFAT/ext4 o particionado MBR clásico.
- GUI gráfica, red, telemetría o actualizaciones.
- Escribir en discos internos o “forzar” un destino rechazado.

## 17. Verificación del diagrama

El diagrama se guarda como XML sin comprimir para que sea revisable en el
repositorio. Se regenera con `python3 tools/generate_diagram.py`; las vistas
previas se exportan con el CLI de draw.io. Páginas:

| # | Página | Contenido | Vista previa |
|---|---|---|---|
| 1 | Componentes | Módulos por mundo, medio USB, cadena de arranque y contrato de formato | [PNG](diagramas/01-componentes.png) |
| 2 | Clases | Enumeraciones, structs, contratos e implementaciones de WP3/WP4, incluidos `PrepRunner` y `DeviceFlasher` | [PNG](diagramas/02-clases.png) |
| 3 | Secuencia | Preparación a través de `PrepRunner`/`DeviceFlasher` y cancelación | [PNG](diagramas/03-secuencia.png) |
| 4 | Estados | `PrepStage` de la preparación y terminales | [PNG](diagramas/04-estados.png) |
| 5 | Leyenda | Relaciones UML, visibilidad y estereotipos | [PNG](diagramas/05-leyenda.png) |

Convenciones:

- Diagrama monocromo; los contenedores separan mundos (preparador, firmware) y
  el medio, no son decoración.
- Componentes con `shape=component` y el contrato de formato con
  `shape=package`; las notas usan `shape=note`.
- Flecha abierta: uso o dependencia; discontinua: relación puntual o contrato.
- Clases con `swimlane` y compartimentos nativos: nombre / datos / métodos.
- Estereotipos `«enumeration»`, `«struct»`, `«class»` e `«interface»`.
- Flecha con triángulo hueco y línea discontinua: realización.
- Flecha abierta: asociación; flecha abierta discontinua: dependencia.
- Rombo relleno en el origen: composición; rombo hueco: agregación.
- En secuencia, `shape=umlLifeline` y mensajes con flecha rellena; los retornos
  son discontinuos.
- En estados, rectángulos redondeados para etapas y gris claro para terminales.

Regeneración de las vistas previas (el snap de draw.io no puede escribir en
`/tmp`; usar una ruta del proyecto):

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

## 18. Estructura de archivos y targets

```text
boot/
  main.cpp        efi_main, mensaje y espera de tecla
  efi.h           tipos y protocolos UEFI mínimos
  efi.cpp         memcpy/memset, helpers de consola y serie
src/
  domain/         Types.hpp · Validation.hpp/.cpp
  core/           Sha256 · Crc32 · SizeFormat · Bytes
  image/          GptWriter · Fat32Writer · BootImageBuilder
                  ImageVerifier · ImageLayout
  device/         IBlockDeviceSource · SysfsBlockDeviceSource
                  SafetyPolicy · MountManager · IDeviceWriter
                  PosixDeviceWriter · FileDeviceWriter · DeviceFlasher
  app/            IPrepController · PrepController · IPrepRunner · PrepRunner
                  FakePrepController
  ui/             AppScene · RailScene · MainScene · StepScene
                  ConfirmScene · ResultScene · UiFormat · UiAssets
  cli/            main.cpp (build/verify/flash)
tests/            TestHarness.hpp · test_*.cpp (una suite por argumento)
tools/            build-efi.sh · run-qemu.sh · embed-efi.py · check-efi.py
                  generate_diagram.py · test-image-cli.sh · test-cli-flash.sh
docs/             Requerimientos · Arquitectura · Glosario · Plan · Evidencias
                  Prueba-fisica · Guion-demo · diagrama
build/generated/  EmbeddedBootImage.cpp/.hpp (generado por CMake)
```

Targets CMake:

| Target | Enlaza | Notas |
|---|---|---|
| `bootx64_efi` | — | Cross-compilado con MinGW-w64, `-nostdlib` |
| `booteable_embedded` | `bootx64_efi` | Genera el arreglo y el SHA-256 |
| `booteable_core` | `booteable_embedded` | Sin Guayaba ni `ui/` |
| `booteable_ui` | `booteable_core` + Guayaba | Solo `app/` y `domain/` |
| `booteable` | `booteable_ui` + `booteable_core` | TUI |
| `booteable-image` | `booteable_core` | CLI headless |
| `booteable_tests` | `booteable_core` + `booteable_ui` | Suites por argumento |
| `check` | — | `ctest -j4 --output-on-failure` |

Reglas de dependencia:

- `domain/` no incluye `<linux/*>`, `<sys/*>`, `<guayaba/*>` ni `boot/efi.h`.
- `image/` no incluye `<guayaba/*>`; solo `domain/`, `core/` y biblioteca estándar.
- `device/` puede incluir Linux; no incluye `ui/`.
- `ui/` no incluye `device/`, `image/` ni `core/`; solo `app/` y `domain/`.
- `boot/` no incluye nada de `src/`; replica las constantes de formato que
  necesita (ninguna en el MVP).
