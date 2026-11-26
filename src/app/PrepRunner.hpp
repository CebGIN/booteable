#ifndef BOOTEABLE_APP_PREPRUNNER_HPP
#define BOOTEABLE_APP_PREPRUNNER_HPP

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

#include "device/IBlockDeviceSource.hpp"
#include "domain/Types.hpp"
#include "image/BootImageBuilder.hpp"

namespace booteable {

/*
    Contrato del flujo completo de preparación. Se separa de la implementación
    real para que las pruebas del controlador puedan inyectar un runner sin
    tocar hardware ni depender de /sys.
*/
class IPrepRunner {
public:
    virtual ~IPrepRunner() = default;

    virtual PrepResult run(
        const PrepConfig& config, const std::atomic_bool& cancel,
        const std::function<void(const PrepProgress&)>& progress) = 0;

    virtual ImageBuildResult build_image(const PrepConfig& config,
                                         const std::filesystem::path& output) = 0;
};

/*
    Runner real de WP4b: construye la imagen GPT/FAT32, la escribe con WP3b y
    verifica la relectura. El orden lo impone flash_image_to_device:
    validar identidad -> desmontar -> abrir -> escribir -> sincronizar -> verificar.

    Si PrepConfig::image_path está vacío se usa un archivo temporal que se borra
    al terminar; si viene informado (exportación) se conserva.
*/
class PrepRunner : public IPrepRunner {
public:
    explicit PrepRunner(IBlockDeviceSource& source);

    PrepResult run(
        const PrepConfig& config, const std::atomic_bool& cancel,
        const std::function<void(const PrepProgress&)>& progress) override;

    ImageBuildResult build_image(const PrepConfig& config,
                                 const std::filesystem::path& output) override;

private:
    IBlockDeviceSource& source_;
};

} // namespace booteable

#endif // BOOTEABLE_APP_PREPRUNNER_HPP
