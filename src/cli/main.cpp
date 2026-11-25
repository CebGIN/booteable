#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "EmbeddedBootImage.hpp"
#include "device/DeviceFlasher.hpp"
#include "device/MountManager.hpp"
#include "device/PosixDeviceWriter.hpp"
#include "device/SafetyPolicy.hpp"
#include "device/SysfsBlockDeviceSource.hpp"
#include "domain/Types.hpp"
#include "image/BootImageBuilder.hpp"
#include "image/ImageVerifier.hpp"

#ifndef BOOTEABLE_VERSION
#define BOOTEABLE_VERSION "0.3.0-wp3"
#endif

namespace {

using booteable::BootImageSpec;
using booteable::ImageBuildResult;
using booteable::ImageManifest;
using booteable::kMib;

void print_usage() {
    std::cout <<
        "booteable-image " BOOTEABLE_VERSION "\n"
        "\n"
        "Uso:\n"
        "  booteable-image build  --output RUTA [--size-mb N] [--esp-mib N]\n"
        "                         [--label ETIQUETA] [--sector-size 512|4096]\n"
        "  booteable-image verify --image RUTA\n"
        "  booteable-image flash  --device RUTA --image RUTA [--yes]\n"
        "  booteable-image --help | --version\n"
        "\n"
        "build:  construye MBR + GPT + ESP FAT32 con el BOOTX64.EFI incrustado y\n"
        "        relee la imagen para verificar. El archivo es disperso y tiene\n"
        "        exactamente el tamaño pedido.\n"
        "verify: relee estructura, CRC, FAT, árbol EFI/BOOT y SHA-256 del binario.\n"
        "flash:  valida el destino, desmonta, escribe, sincroniza y relee. Sin\n"
        "        --yes pide confirmación explícita. Un archivo regular se acepta\n"
        "        como destino de prueba, nunca un disco del sistema.\n";
}

bool parse_u64(const std::string& text, std::uint64_t& out) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    for (char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    out = value;
    return true;
}

void print_manifest(const ImageManifest& manifest) {
    static const char* const kNames[] = {
        "MBR", "GPT primaria", "ESP FAT32", "GPT de respaldo"};
    std::cout << "  Regiones verificadas:\n";
    for (std::size_t index = 0; index < manifest.regions.size(); ++index) {
        const booteable::ImageRegion& region = manifest.regions[index];
        std::cout << "    " << (index < 4 ? kNames[index] : "región")
                  << " · offset " << region.offset << " · " << region.length
                  << " bytes · sha256 " << region.sha256.substr(0, 16) << "…\n";
    }
    std::cout << "  Manifiesto sha256: " << manifest.manifest_sha256 << "\n";
}

int command_build(const std::vector<std::string>& args) {
    std::string output;
    std::string label = "BOOTEABLE";
    std::uint64_t size_mb = 0;
    std::uint64_t esp_mib = 0;
    bool has_size = false;
    bool has_esp = false;
    std::uint32_t sector_size = 512;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& option = args[index];
        const auto value = [&args, &index, &option]() -> std::string {
            if (index + 1 >= args.size()) {
                std::cerr << "Falta el valor de " << option << ".\n";
                return {};
            }
            return args[++index];
        };

        if (option == "--output") {
            output = value();
        } else if (option == "--size-mb") {
            const std::string text = value();
            if (!parse_u64(text, size_mb) || size_mb == 0) {
                std::cerr << "--size-mb requiere un entero positivo.\n";
                return 2;
            }
            has_size = true;
        } else if (option == "--esp-mib") {
            const std::string text = value();
            if (!parse_u64(text, esp_mib) || esp_mib == 0) {
                std::cerr << "--esp-mib requiere un entero positivo.\n";
                return 2;
            }
            has_esp = true;
        } else if (option == "--label") {
            label = value();
        } else if (option == "--sector-size") {
            std::uint64_t parsed = 0;
            if (!parse_u64(value(), parsed) ||
                (parsed != 512 && parsed != 4096)) {
                std::cerr << "--sector-size debe ser 512 o 4096.\n";
                return 2;
            }
            sector_size = static_cast<std::uint32_t>(parsed);
        } else {
            std::cerr << "Opción desconocida: " << option << "\n";
            return 2;
        }
    }

    if (output.empty()) {
        std::cerr << "build requiere --output RUTA.\n";
        return 2;
    }
    if (!has_size) {
        // 4Kn necesita una ESP mínima de 260 MiB; el valor por defecto deja
        // lugar para la GPT y una ESP cómoda. En 512 se parte de 64 MiB y, si
        // no entra, compute_layout ajusta la ESP automática.
        size_mb = sector_size == 4096 ? 512 : 64;
    }
    if (size_mb > UINT64_MAX / kMib) {
        std::cerr << "--size-mb es demasiado grande.\n";
        return 2;
    }
    if (has_esp && esp_mib > UINT64_MAX / kMib) {
        std::cerr << "--esp-mib es demasiado grande.\n";
        return 2;
    }

    BootImageSpec spec;
    spec.disk_bytes = size_mb * kMib;
    spec.esp_bytes = has_esp ? esp_mib * kMib : 0;
    spec.volume_label = label;
    spec.logical_sector = sector_size;

    const ImageBuildResult result = booteable::build_boot_image(
        output, spec, booteable::kBootEfi, booteable::kBootEfiSize);
    if (!result.ok) {
        std::cerr << "No se pudo construir la imagen: " << result.error << "\n";
        return 1;
    }

    std::cout << "Imagen construida y verificada: " << output << "\n"
              << "  Archivo: " << result.manifest.disk_bytes << " bytes ("
              << result.manifest.disk_bytes / kMib << " MiB)\n"
              << "  Sector lógico: " << result.layout.sector_size << " bytes\n"
              << "  GPT primaria en LBA 1 · entrada_sectores "
              << result.layout.entry_sectors << "\n"
              << "  ESP en LBA " << result.layout.esp_first_lba << " · "
              << result.esp_bytes / kMib << " MiB · FAT "
              << result.layout.fat_sectors << " sectores · "
              << result.layout.cluster_count << " clústeres\n"
              << "  GPT de respaldo en LBA " << result.layout.backup_header_lba
              << "\n"
              << "  EFI: " << booteable::kBootEfiSize << " bytes · sha256 "
              << booteable::kBootEfiSha256 << "\n";
    print_manifest(result.manifest);
    return 0;
}

int command_verify(const std::vector<std::string>& args) {
    std::string image;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "--image" && index + 1 < args.size()) {
            image = args[++index];
        } else {
            std::cerr << "Opción desconocida o sin valor: " << args[index]
                      << "\n";
            return 2;
        }
    }
    if (image.empty()) {
        std::cerr << "verify requiere --image RUTA.\n";
        return 2;
    }

    ImageManifest manifest;
    std::string error;
    const booteable::ImageVerifier verifier;
    if (!verifier.verify(image, manifest, error)) {
        std::cerr << "La imagen NO verifica: " << error << "\n";
        return 1;
    }

    std::cout << "Imagen verificada: " << image << "\n"
              << "  Tamaño: " << manifest.disk_bytes << " bytes\n";
    print_manifest(manifest);
    return 0;
}

std::string outcome_name(booteable::PrepOutcome outcome) {
    using booteable::PrepOutcome;
    switch (outcome) {
    case PrepOutcome::Verified:
        return "VERIFICADO";
    case PrepOutcome::VerificationFailed:
        return "VERIFICACIÓN FALLIDA";
    case PrepOutcome::Cancelled:
        return "CANCELADO";
    case PrepOutcome::Incomplete:
        return "INCOMPLETO";
    case PrepOutcome::PermissionDenied:
        return "SIN PERMISOS";
    case PrepOutcome::InvalidTarget:
        return "DESTINO INVÁLIDO";
    case PrepOutcome::Disconnected:
        return "DESCONECTADO";
    case PrepOutcome::InsufficientSpace:
        return "CAPACIDAD INSUFICIENTE";
    case PrepOutcome::BuildError:
        return "ERROR DE IMAGEN";
    case PrepOutcome::DeviceError:
        return "ERROR DE DISPOSITIVO";
    case PrepOutcome::UnmountError:
        return "ERROR AL DESMONTAR";
    case PrepOutcome::None:
    default:
        return "SIN RESULTADO";
    }
}

std::string stage_name(booteable::PrepStage stage) {
    using booteable::PrepStage;
    switch (stage) {
    case PrepStage::Idle:
        return "inactiva";
    case PrepStage::Enumerating:
        return "enumerando";
    case PrepStage::Validating:
        return "validando";
    case PrepStage::Confirming:
        return "confirmando";
    case PrepStage::Building:
        return "construyendo";
    case PrepStage::Unmounting:
        return "desmontando";
    case PrepStage::Opening:
        return "abriendo";
    case PrepStage::Writing:
        return "escribiendo";
    case PrepStage::Syncing:
        return "sincronizando";
    case PrepStage::Verifying:
        return "verificando";
    case PrepStage::Done:
        return "completada";
    case PrepStage::Failed:
        return "falló";
    case PrepStage::Cancelled:
        return "cancelada";
    case PrepStage::Incomplete:
        return "incompleta";
    default:
        return "desconocida";
    }
}

void print_safety(const booteable::SafetyReport& report) {
    for (const std::string& reason : report.reasons) {
        std::cerr << "  Rechazo: " << reason << "\n";
    }
    for (const std::string& warning : report.warnings) {
        std::cout << "  Advertencia: " << warning << "\n";
    }
}

bool prompt_confirmation(const booteable::DeviceInfo& device,
                         const booteable::ImageManifest& manifest) {
    std::cout << "\nSe va a sobrescribir " << device.path << " ("
              << device.size_bytes / kMib << " MiB).\n"
              << "  Modelo: " << device.model << "\n"
              << "  Serie: " << device.serial << "\n"
              << "  Imagen: " << manifest.disk_bytes << " bytes · sha256 "
              << manifest.manifest_sha256.substr(0, 16) << "…\n"
              << "Se perderá todo el contenido anterior.\n"
              << "¿Continuar? [s/N]: ";
    std::cout.flush();

    std::string answer;
    if (!std::getline(std::cin, answer)) {
        return false;
    }
    for (char& character : answer) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return answer == "s" || answer == "si" || answer == "sí" || answer == "y" ||
           answer == "yes";
}

bool resolve_target(const std::string& path, std::uint32_t image_sector,
                    booteable::SysfsBlockDeviceSource& source,
                    booteable::DeviceInfo& device, bool& regular_file,
                    std::string& error) {
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) {
        error = "No se pudo consultar " + path + ": " + std::strerror(errno);
        return false;
    }

    if (S_ISBLK(info.st_mode)) {
        const std::optional<booteable::DeviceInfo> found = source.refresh(path);
        if (!found) {
            error = path + " no aparece en /sys/block como dispositivo de bloque.";
            return false;
        }
        device = *found;
        regular_file = false;
        return true;
    }

    if (S_ISREG(info.st_mode)) {
        // Destino de prueba: un archivo regular del tamaño exacto de la imagen.
        // No se aplica la política de USB porque no hay hardware que proteger.
        device = booteable::DeviceInfo{};
        device.path = path;
        device.size_bytes = static_cast<std::uint64_t>(info.st_size);
        device.logical_sector = image_sector;
        device.removable = true;
        device.transport = booteable::DeviceTransport::Virtual;
        device.serial = "regular-file";
        device.by_id = path;
        regular_file = true;
        return true;
    }

    error = path + " no es un dispositivo de bloque ni un archivo regular.";
    return false;
}

int command_flash(const std::vector<std::string>& args) {
    std::string device_path;
    std::string image_path;
    bool assume_yes = false;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& option = args[index];
        if (option == "--yes") {
            assume_yes = true;
        } else if (option == "--device") {
            if (index + 1 >= args.size()) {
                std::cerr << "Falta el valor de --device.\n";
                return 2;
            }
            device_path = args[++index];
        } else if (option == "--image") {
            if (index + 1 >= args.size()) {
                std::cerr << "Falta el valor de --image.\n";
                return 2;
            }
            image_path = args[++index];
        } else {
            std::cerr << "Opción desconocida: " << option << "\n";
            return 2;
        }
    }

    if (device_path.empty() || image_path.empty()) {
        std::cerr << "flash requiere --device RUTA y --image RUTA.\n";
        return 2;
    }

    booteable::ImageManifest manifest;
    std::string error;
    const booteable::ImageVerifier verifier;
    if (!verifier.verify(image_path, manifest, error)) {
        std::cerr << "La imagen no verifica: " << error << "\n";
        return 1;
    }

    std::uint32_t image_sector = 0;
    if (!booteable::detect_image_sector_size(image_path, image_sector, error)) {
        std::cerr << "No se pudo detectar el sector de la imagen: " << error
                  << "\n";
        return 1;
    }

    booteable::SysfsBlockDeviceSource source;
    booteable::DeviceInfo device;
    bool regular_file = false;
    if (!resolve_target(device_path, image_sector, source, device,
                        regular_file, error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (device.size_bytes != manifest.disk_bytes) {
        std::cerr << "La imagen mide " << manifest.disk_bytes
                  << " bytes y el destino " << device.size_bytes
                  << " bytes; deben coincidir. Usá --size-mb al construir.\n";
        return 1;
    }
    if (!regular_file && device.logical_sector != image_sector) {
        std::cerr << "La imagen usa sectores de " << image_sector
                  << " bytes y el destino de " << device.logical_sector
                  << "; construí la imagen con --sector-size "
                  << device.logical_sector << ".\n";
        return 1;
    }

    if (regular_file) {
        std::cout << "Destino de prueba: archivo regular " << device.path
                  << " (" << device.size_bytes / kMib << " MiB).\n";
    } else {
        const booteable::SafetyPolicy policy;
        const booteable::SafetyReport report = policy.evaluate_for_write(
            device, source.system_context());
        print_safety(report);
        if (report.verdict == booteable::SafetyVerdict::Rejected) {
            std::cerr << "El destino fue rechazado; no se escribió nada.\n";
            return 1;
        }
        if (::geteuid() != 0) {
            std::cerr << "Se requieren privilegios para escribir "
                      << device.path << ". Ejecutá `sudo ./build/booteable-image"
                      << " flash ...`.\n";
            return 1;
        }
    }

    if (!assume_yes && !prompt_confirmation(device, manifest)) {
        std::cout << "Cancelado por el usuario; no se abrió el dispositivo.\n";
        return 1;
    }

    booteable::PosixDeviceWriter writer;
    booteable::MountManager mounts;
    booteable::FlashCallbacks callbacks;
    if (!regular_file) {
        callbacks.refresh = [&source](const std::string& path) {
            return source.refresh(path);
        };
    }
    callbacks.progress = [](const booteable::PrepProgress& progress) {
        std::cerr << "  [" << progress.percent << "%] " << progress.detail
                  << "\n";
    };

    const booteable::PrepResult result = booteable::flash_image_to_device(
        device, image_path, manifest, writer, mounts, callbacks);

    std::cout << "\nResultado: " << outcome_name(result.outcome) << "\n"
              << "  Etapa: " << stage_name(result.failed_stage) << "\n"
              << "  Mensaje: " << result.message << "\n"
              << "  Bytes escritos: " << result.bytes_written << "\n"
              << "  Sincronizado: " << (result.synchronized ? "sí" : "no")
              << "\n"
              << "  Verificado: " << (result.verified ? "sí" : "no") << "\n"
              << "  Imagen sha256: " << result.image_sha256 << "\n"
              << "  Dispositivo sha256: " << result.device_sha256 << "\n";

    return result.outcome == booteable::PrepOutcome::Verified ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return args.empty() ? 2 : 0;
    }

    if (args[0] == "--version") {
        std::cout << "booteable-image " BOOTEABLE_VERSION "\n";
        return 0;
    }

    const std::vector<std::string> rest(args.begin() + 1, args.end());
    if (args[0] == "build") {
        return command_build(rest);
    }
    if (args[0] == "verify") {
        return command_verify(rest);
    }
    if (args[0] == "flash") {
        return command_flash(rest);
    }

    std::cerr << "Comando desconocido: " << args[0] << "\n";
    print_usage();
    return 2;
}
