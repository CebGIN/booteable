#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "EmbeddedBootImage.hpp"
#include "domain/Types.hpp"
#include "image/BootImageBuilder.hpp"
#include "image/ImageVerifier.hpp"

#ifndef BOOTEABLE_VERSION
#define BOOTEABLE_VERSION "0.2.0-wp2"
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
        "flash:  llega en WP3b; por ahora no escribe dispositivos.\n";
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
        std::cerr << "El comando 'flash' llega en WP3b; todavía no escribe "
                     "dispositivos.\n";
        return 2;
    }

    std::cerr << "Comando desconocido: " << args[0] << "\n";
    print_usage();
    return 2;
}
