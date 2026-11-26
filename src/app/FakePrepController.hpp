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
    bool can_write() const override;
    std::string write_block_reason() const override;

    // Ayuda para las pruebas: simula un error en la etapa indicada.
    void fail_next(PrepOutcome outcome, PrepStage stage, std::string message);

    // Modo por pasos para las pruebas de la UI: start() deja el flujo en
    // Building y cada advance() avanza una etapa; así se observan progreso y
    // cancelación sin depender del tiempo real.
    void set_manual_steps(bool enabled);
    void advance();

    // Permite probar la UI sin root.
    void set_can_write(bool allowed, std::string reason = {});

private:
    std::vector<DeviceInfo> devices_;
    PrepProgress progress_;
    PrepResult result_;
    bool running_ = false;
    bool fail_armed_ = false;
    PrepOutcome fail_outcome_ = PrepOutcome::None;
    PrepStage fail_stage_ = PrepStage::Idle;
    std::string fail_message_;
    bool manual_steps_ = false;
    bool can_write_ = true;
    std::string write_block_reason_; 
};

} // namespace booteable

#endif // BOOTEABLE_APP_FAKEPREPCONTROLLER_HPP
