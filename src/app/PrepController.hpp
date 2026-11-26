#ifndef BOOTEABLE_APP_PREPCONTROLLER_HPP
#define BOOTEABLE_APP_PREPCONTROLLER_HPP

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/IPrepController.hpp"
#include "app/PrepRunner.hpp"
#include "device/IBlockDeviceSource.hpp"
#include "device/SafetyPolicy.hpp"
#include "domain/Types.hpp"

namespace booteable {

/*
    Controlador real de WP4b. Enumeración y política se consultan en el hilo de
    la UI; la preparación corre en un hilo propio y publica PrepProgress y
    PrepResult protegidos por un mutex corto.

    La cancelación es un std::atomic_bool que el runner consulta entre bloques.
    El destructor cancela y espera al hilo, de modo que no quedan descriptores ni
    montajes abiertos al cerrar la aplicación.
*/
class PrepController : public IPrepController {
public:
    using PrivilegeCheck = std::function<bool()>;

    PrepController(IBlockDeviceSource& source, IPrepRunner& runner,
                   SafetyPolicy policy = {},
                   PrivilegeCheck can_write =
                       &PrepController::default_can_write);

    ~PrepController() override;

    PrepController(const PrepController&) = delete;
    PrepController& operator=(const PrepController&) = delete;

    std::vector<DeviceInfo> devices() override;
    SafetyReport safety(const DeviceInfo& device) override;
    ValidationReport validate(const PrepConfig& config) override;
    bool start(const PrepConfig& config) override;
    void cancel() override;
    PrepProgress progress() const override;
    PrepResult result() const override;
    bool running() const override;

    bool can_write() const override;
    std::string write_block_reason() const override;
    bool export_image(const PrepConfig& config, std::string& error) override;
    bool export_default_image(PrepConfig& config, std::string& image_path,
                              std::string& error) override;

    static bool default_can_write();

private:
    void publish_failure(PrepOutcome outcome, PrepStage stage,
                         const std::string& message);

    IBlockDeviceSource& source_;
    IPrepRunner& runner_;
    SafetyPolicy policy_;
    PrivilegeCheck can_write_;

    std::atomic_bool running_{false};
    std::atomic_bool cancel_{false};

    mutable std::mutex mutex_;
    PrepProgress progress_;
    PrepResult result_;

    std::thread worker_;
};

} // namespace booteable

#endif // BOOTEABLE_APP_PREPCONTROLLER_HPP
