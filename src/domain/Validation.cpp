#include "domain/Validation.hpp"

#include <string>

namespace booteable {
namespace {

constexpr std::uint64_t kAlignmentBytes = kMib;      // ESP alineada a 1 MiB
constexpr std::uint64_t kGptEntriesBytes = 128ull * 128ull;

bool is_printable_ascii(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return value >= 0x20 && value <= 0x7E;
}

bool is_fat_forbidden(char character) {
    switch (character) {
    case '"':
    case '*':
    case '/':
    case ':':
    case '<':
    case '>':
    case '?':
    case '\\':
    case '|':
        return true;
    default:
        return false;
    }
}

void add_issue(ValidationReport& report, const char* field,
               const std::string& message) {
    report.issues.push_back(ValidationIssue{field, message});
}

} // namespace

bool is_supported_sector(std::uint32_t logical_sector) {
    return logical_sector == 512 || logical_sector == 4096;
}

std::uint64_t min_esp_bytes(std::uint32_t logical_sector) {
    return logical_sector == 4096 ? kMinEspBytes4Kn : kMinEspBytes512;
}

std::uint64_t default_esp_bytes(std::uint32_t logical_sector) {
    return logical_sector == 4096 ? kDefaultEspBytes4Kn : kDefaultEspBytes512;
}

std::uint64_t effective_esp_bytes(const PrepConfig& config) {
    if (config.esp_bytes != 0) {
        return config.esp_bytes;
    }
    return default_esp_bytes(config.device.logical_sector);
}

ValidationReport validate_config(const PrepConfig& config) {
    ValidationReport report;
    const DeviceInfo& device = config.device;

    if (!is_supported_sector(device.logical_sector)) {
        add_issue(report, "logical_sector",
                  "El sector lógico debe ser de 512 o 4096 bytes.");
    }

    if (device.size_bytes == 0) {
        add_issue(report, "device.size_bytes",
                  "El dispositivo no reporta capacidad.");
    } else if (is_supported_sector(device.logical_sector) &&
               device.size_bytes % device.logical_sector != 0) {
        add_issue(report, "device.size_bytes",
                  "La capacidad no es múltiplo del sector lógico.");
    }

    const std::uint64_t esp_bytes = effective_esp_bytes(config);
    const std::uint64_t minimum = min_esp_bytes(device.logical_sector);
    if (esp_bytes < minimum) {
        add_issue(report, "esp_bytes",
                  "La ESP debe tener al menos " + std::to_string(minimum / kMib) +
                      " MiB para ese sector lógico.");
    }
    if (esp_bytes > kMaxEspBytes) {
        add_issue(report, "esp_bytes",
                  "La ESP no puede superar los 2048 MiB.");
    }
    if (is_supported_sector(device.logical_sector) &&
        esp_bytes % device.logical_sector != 0) {
        add_issue(report, "esp_bytes",
                  "La ESP debe ser múltiplo del sector lógico.");
    }

    const std::string& label = config.volume_label;
    if (label.empty() || label.size() > kMaxVolumeLabelLength) {
        add_issue(report, "volume_label",
                  "La etiqueta debe tener entre 1 y 11 caracteres.");
    } else {
        if (label.front() == ' ') {
            add_issue(report, "volume_label",
                      "La etiqueta no puede empezar con un espacio.");
        }
        for (char character : label) {
            if (!is_printable_ascii(character)) {
                add_issue(report, "volume_label",
                          "La etiqueta solo admite ASCII imprimible.");
                break;
            }
            if (character >= 'a' && character <= 'z') {
                add_issue(report, "volume_label",
                          "La etiqueta debe usar mayúsculas.");
                break;
            }
            if (is_fat_forbidden(character)) {
                add_issue(report, "volume_label",
                          "La etiqueta contiene un carácter no válido en FAT.");
                break;
            }
        }
    }

    if (!config.overwrite_confirmed) {
        add_issue(report, "overwrite_confirmed",
                  "Falta la casilla de confirmación de sobrescritura.");
    }

    // Geometría: GPT primaria, ESP alineada y GPT de respaldo. Se replica el
    // cálculo de ImageLayout para rechazar configuraciones imposibles antes de
    // construir o abrir el dispositivo.
    if (is_supported_sector(device.logical_sector) && device.size_bytes != 0 &&
        device.size_bytes % device.logical_sector == 0) {
        const std::uint64_t sector = device.logical_sector;
        const std::uint64_t total_sectors = device.size_bytes / sector;
        const std::uint64_t entry_sectors =
            (kGptEntriesBytes + sector - 1) / sector;
        const std::uint64_t esp_first_lba = kAlignmentBytes / sector;
        const std::uint64_t esp_sectors = esp_bytes / sector;
        const std::uint64_t first_usable = 2 + entry_sectors;

        if (total_sectors <= 2 + 2 * entry_sectors) {
            add_issue(report, "device.size_bytes",
                      "El dispositivo es demasiado pequeño para la GPT.");
        } else {
            const std::uint64_t last_usable = total_sectors - 2 - entry_sectors;
            if (esp_first_lba < first_usable) {
                add_issue(report, "esp_first_lba",
                          "La ESP no puede empezar antes de la GPT primaria.");
            } else if (esp_first_lba + esp_sectors - 1 > last_usable) {
                add_issue(report, "esp_bytes",
                          "La ESP no entra antes de la GPT de respaldo.");
            }
        }
    }

    return report;
}

ValidationReport validate_efi_size(const PrepConfig& config,
                                   std::uint64_t efi_bytes) {
    ValidationReport report;
    if (efi_bytes == 0) {
        add_issue(report, "boot_efi",
                  "El binario EFI incrustado está vacío.");
        return report;
    }
    const std::uint64_t esp_bytes = effective_esp_bytes(config);
    if (efi_bytes >= esp_bytes) {
        add_issue(report, "boot_efi",
                  "El binario EFI no entra en la ESP.");
    }
    return report;
}

ValidationReport validate(const PrepConfig& config, std::uint64_t efi_bytes) {
    ValidationReport report = validate_config(config);
    const ValidationReport efi = validate_efi_size(config, efi_bytes);
    report.issues.insert(report.issues.end(), efi.issues.begin(), efi.issues.end());
    return report;
}

} // namespace booteable
