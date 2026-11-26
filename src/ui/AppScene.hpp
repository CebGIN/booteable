#ifndef BOOTEABLE_UI_APPSCENE_HPP
#define BOOTEABLE_UI_APPSCENE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "app/IPrepController.hpp"
#include "domain/Types.hpp"
#include "ui/ConfirmDialog.hpp"
#include "ui/MainScene.hpp"
#include "ui/ProgressScene.hpp"
#include "ui/ResultScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {

enum class UiScreen {
    Main,
    Confirm,
    Progress,
    Result,
};

// Estado observable de la TUI. Las escenas no guardan lógica de negocio: leen
// este estado y llaman a AppScene, que a su vez solo habla con
// IPrepController.
struct UiState {
    UiScreen screen = UiScreen::Main;
    std::size_t selected = 0;
    bool confirmed = false;
    bool can_write = true;
    std::string write_reason;
    std::string status;
    StatusLevel status_level = StatusLevel::Hint;
    SafetyReport safety;
    PrepProgress progress;
    PrepResult result;
};

/*
    Coordinador de las cuatro escenas de WP4. Construye el árbol de Guayaba, traduce
    la entrada global a acciones y refresca la vista con el estado del controlador.

    La UI no incluye device/, image/ ni core/: todo pasa por IPrepController.
*/
class AppScene {
public:
    explicit AppScene(IPrepController& controller);

    AppScene(const AppScene&) = delete;
    AppScene& operator=(const AppScene&) = delete;

    // Construye la escena completa. El llamador entrega la raíz a SceneManager.
    std::unique_ptr<guayaba::Node> build();

    // Entrada y ciclo de vida. `handle_key` se usa desde el script global y
    // también desde las pruebas headless.
    void handle_key(guayaba::Key key);
    void tick();
    void refresh();

    // Acciones disparadas por la UI o por las pruebas.
    void request_prepare();
    void request_export();
    void select(int index);
    void set_confirmed(bool confirmed);
    void accept_confirmation();
    void dismiss_confirmation();
    void cancel_preparation();
    void leave_result();
    void quit();

    const UiState& state() const { return state_; }
    IPrepController& controller() { return controller_; }

    // Accesos de solo lectura para las pruebas headless.
    guayaba::NodeRef<guayaba::ListView> device_list() const;
    guayaba::NodeRef<guayaba::Button> prepare_button() const;
    guayaba::NodeRef<guayaba::Button> export_button() const;
    guayaba::NodeRef<guayaba::CheckBox> warning_checkbox() const;
    guayaba::NodeRef<guayaba::ProgressBar> progress_bar() const;
    guayaba::NodeRef<guayaba::SplitContainer> split() const;
    guayaba::NodeRef<guayaba::PanelContainer> devices_panel() const;
    guayaba::NodeRef<guayaba::PanelContainer> details_panel() const;
    guayaba::NodeRef<guayaba::Label> result_outcome() const;

private:
    void show(UiScreen screen);
    void set_status(std::string text, StatusLevel level);
    void refresh_main();
    void refresh_confirm();
    void refresh_progress();
    void refresh_result();
    void focus_current_screen();
    const DeviceInfo* selected_device() const;
    PrepConfig config_for(const DeviceInfo& device, bool confirmed) const;

    IPrepController& controller_;
    UiState state_;
    std::string confirm_status_;
    std::vector<DeviceInfo> devices_;

    // Dispositivo confirmado al abrir el diálogo. El flujo de escritura usa
    // esta instantánea y no la selección actual, que no debe cambiar mientras
    // el diálogo está abierto.
    std::optional<DeviceInfo> target_;
    std::string progress_notice_;
    int progress_notice_ticks_ = 0;

    std::unique_ptr<guayaba::Theme> theme_;

    MainSceneWidgets main_;
    ConfirmDialogWidgets confirm_;
    ProgressSceneWidgets progress_;
    ResultSceneWidgets result_;
};

} // namespace booteable::ui

#endif // BOOTEABLE_UI_APPSCENE_HPP
