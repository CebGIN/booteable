#ifndef BOOTEABLE_APP_FAKEPREPCONTROLLER_HPP
#define BOOTEABLE_APP_FAKEPREPCONTROLLER_HPP

#include <vector>

#include "app/IPrepController.hpp"

namespace booteable {

// Controlador de prueba: no toca hardware. Sirve para que la UI y los tests
// headless trabajen contra el contrato real desde WP0.
class FakePrepController : public IPrepController {
public:
    explicit FakePrepController(std::vector<DeviceInfo> devices = default_devices());

    static std::vector<DeviceInfo> default_devices();

    std::vector<DeviceInfo> devices() override;
    SafetyReport safety(const DeviceInfo& device) override;
    ValidationReport validate(const PrepConfig& config) override;
    bool start(const PrepConfig& config) override;
    void cancel() override;
    PrepProgress progress() const override;
    PrepResult result() const override;
    bool running() const override;

    // Ayuda para las pruebas: simula un error en la etapa indicada.
    void fail_next(PrepOutcome outcome, PrepStage stage, std::string message);

private:
    std::vector<DeviceInfo> devices_;
    PrepProgress progress_;
    PrepResult result_;
    bool running_ = false;
    bool fail_armed_ = false;
    PrepOutcome fail_outcome_ = PrepOutcome::None;
    PrepStage fail_stage_ = PrepStage::Idle;
    std::string fail_message_;
};

} // namespace booteable

#endif // BOOTEABLE_APP_FAKEPREPCONTROLLER_HPP
