#include "image/BootImageBuilder.hpp"

#include <filesystem>
#include <system_error>

#include "domain/Validation.hpp"
#include "image/Fat32Writer.hpp"
#include "image/GptWriter.hpp"
#include "image/ImageFile.hpp"
#include "image/ImageLayout.hpp"
#include "image/ImageVerifier.hpp"

namespace booteable {
namespace {

void fail(ImageBuildResult& result, const std::string& message) {
    result.ok = false;
    result.error = message;
}

std::string first_validation_error(const ValidationReport& report) {
    if (report.ok()) {
        return {};
    }
    return report.issues.front().field + ": " + report.issues.front().message;
}

} // namespace

ImageBuildResult build_boot_image(const std::filesystem::path& output,
                                  const BootImageSpec& spec,
                                  const unsigned char* efi,
                                  std::size_t efi_size) {
    ImageBuildResult result;
    result.esp_bytes = layout_esp_bytes(spec);

    if (efi == nullptr || efi_size == 0) {
        fail(result, "El binario EFI incrustado está vacío.");
        return result;
    }

    // Se calcula primero la geometría: si la ESP automática no entra en un
    // disco pequeño, se ajusta antes de validar. Luego se reutilizan las reglas
    // de dominio para etiqueta, sector y capacidad.
    const LayoutResult layout_result = compute_layout(spec);
    if (!layout_result.ok) {
        fail(result, layout_result.error);
        return result;
    }
    result.layout = layout_result.layout;
    result.esp_bytes = result.layout.esp_sectors * result.layout.sector_size;

    PrepConfig config;
    config.device.path = output.string();
    config.device.size_bytes = spec.disk_bytes;
    config.device.logical_sector = spec.logical_sector;
    config.esp_bytes = result.esp_bytes;
    config.volume_label = spec.volume_label;
    config.overwrite_confirmed = true;
    const ValidationReport validation =
        validate(config, efi_size);
    if (!validation.ok()) {
        fail(result, first_validation_error(validation));
        return result;
    }

    const GptGuids guids = default_gpt_guids();
    const std::vector<std::uint8_t> mbr =
        make_protective_mbr(result.layout);
    const std::vector<std::uint8_t> primary_gpt =
        make_primary_gpt(result.layout, guids, spec.volume_label);
    const std::vector<std::uint8_t> backup_gpt =
        make_backup_gpt(result.layout, guids, spec.volume_label);
    if (mbr.size() != result.layout.sector_size ||
        primary_gpt.empty() || backup_gpt.empty()) {
        fail(result, "No se pudieron generar las estructuras GPT.");
        return result;
    }

    std::error_code ignored;
    if (std::filesystem::exists(output, ignored) &&
        !std::filesystem::is_regular_file(output, ignored)) {
        fail(result,
             "La salida de build debe ser un archivo regular, no un dispositivo.");
        return result;
    }
    std::filesystem::remove(output, ignored);

    ImageFile file;
    std::string error;
    bool created = file.create(output, spec.disk_bytes, error);
    bool written = false;

    const std::uint64_t esp_offset =
        result.layout.esp_first_lba * result.layout.sector_size;
    const EspSink sink = [&file, esp_offset](
                             std::uint64_t offset, const void* data,
                             std::size_t length, std::string& sink_error) {
        return file.write_at(esp_offset + offset, data, length, sink_error);
    };

    if (created) {
        written =
            file.write_at(0, mbr.data(), mbr.size(), error) &&
            file.write_at(result.layout.sector_size, primary_gpt.data(),
                          primary_gpt.size(), error) &&
            write_fat32(result.layout, spec.volume_label, efi, efi_size, sink,
                        error) &&
            file.write_at(result.layout.backup_entries_lba *
                              result.layout.sector_size,
                          backup_gpt.data(), backup_gpt.size(), error) &&
            file.sync(error);
    }

    std::string close_error;
    const bool closed = file.close(close_error);
    if (!closed && error.empty()) {
        error = close_error;
    }

    if (!created || !written || !closed) {
        std::filesystem::remove(output, ignored);
        fail(result, error.empty() ? "No se pudo escribir la imagen." : error);
        return result;
    }

    // El manifiesto sale de releer la imagen completa, no de las escrituras.
    ImageVerifier verifier;
    if (!verifier.verify(output, result.manifest, error)) {
        std::filesystem::remove(output, ignored);
        fail(result, "La imagen recién escrita no verifica: " + error);
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace booteable
