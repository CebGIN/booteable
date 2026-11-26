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
#include "ui/ConfirmScene.hpp"
#include "ui/MainScene.hpp"
#include "ui/RailScene.hpp"
#include "ui/ResultScene.hpp"
#include "ui/StepScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {

enum class UiScreen {
    Main,
    Confirm,
    Progress,
    Result,
};

// Estado observable de la TUI. Las escenas no guardan lógica de negocio: leen
// este estado y llaman a AppScene, que a su vez solo habla con IPrepController.
struct UiState {
    UiScreen screen = UiScreen::Main;
    bool rail_expanded = false;
    bool export_page = false;
    std::size_t selected = 0;
    bool confirmed = false;
    bool can_write = true;
    std::string write_reason;
    std::string status;
    StatusLevel status_level = StatusLevel::Hint;
    SafetyReport safety;
    PrepProgress progress;
    PrepResult result;
    WizardStep wizard_step = WizardStep::Detect;
};

/*
    Coordinador del wizard. Compone las escenas declarativas, traduce la entrada
    global a acciones y refresca la vista con el estado del controlador.

    La UI no incluye device/, image/ ni core/: todo pasa por IPrepController.
*/
class AppScene {
public:
    explicit AppScene(IPrepController& controller);

    AppScene(const AppScene&) = delete;
    AppScene& operator=(const AppScene&) = delete;

    // Construye la escena completa. El llamador entrega la raíz a SceneManager.
    std::unique_ptr<guayaba::Node> build();

    // Entrada, ciclo de vida y animación. `handle_key` se usa desde el script
    // global y también desde las pruebas headless.
    void handle_key(guayaba::Key key);
    void animate(double delta_time);
    void settle_transition();
    void tick();
    void refresh();

    // Acciones disparadas por la UI o por las pruebas.
    void request_prepare();
    void advance_step();
    void request_export();
    void request_export_standard();
    void request_export_selected();
    void select(int index);
    void set_confirmed(bool confirmed);
    void accept_confirmation();
    void dismiss_confirmation();
    void cancel_preparation();
    void leave_result();
    void quit();
    void toggle_rail();
    void open_export_page();
    void close_export_page();
    void rail_back();
    void request_refresh();

    const UiState& state() const { return state_; }
    IPrepController& controller() { return controller_; }
    bool result_ready() const { return result_ready_; }

    // Accesos de solo lectura para las pruebas headless.
    guayaba::NodeRef<guayaba::ListView> device_list() const;
    guayaba::NodeRef<guayaba::Button> prepare_button() const;
    guayaba::NodeRef<guayaba::Button> export_button() const;
    guayaba::NodeRef<guayaba::Button> export_standard_button() const;
    guayaba::NodeRef<guayaba::Button> export_selected_button() const;
    guayaba::NodeRef<guayaba::PanelContainer> rail() const;
    guayaba::NodeRef<guayaba::Control> rail_slot() const;
    guayaba::NodeRef<guayaba::Control> stage() const;
    guayaba::NodeRef<guayaba::Label> status_label() const;
    guayaba::NodeRef<guayaba::PanelContainer> step_panel() const;
    guayaba::NodeRef<guayaba::Label> step_kicker() const;
    guayaba::NodeRef<guayaba::Label> step_title() const;
    guayaba::NodeRef<guayaba::Label> step_status() const;
    guayaba::NodeRef<guayaba::Label> step_log() const;
    guayaba::NodeRef<guayaba::Button> continue_button() const;
    guayaba::NodeRef<guayaba::Button> cancel_button() const;
    guayaba::NodeRef<guayaba::CheckBox> warning_checkbox() const;
    guayaba::NodeRef<guayaba::ProgressBar> progress_bar() const;
    guayaba::NodeRef<guayaba::SplitContainer> split() const;
    guayaba::NodeRef<guayaba::PanelContainer> main_panel() const;
    guayaba::NodeRef<guayaba::PanelContainer> devices_panel() const;
    guayaba::NodeRef<guayaba::PanelContainer> details_panel() const;
    guayaba::NodeRef<guayaba::PanelContainer> confirm_panel() const;
    guayaba::NodeRef<guayaba::PanelContainer> result_panel() const;
    guayaba::NodeRef<guayaba::Label> result_outcome() const;
    guayaba::NodeRef<guayaba::Label> result_kicker() const;
    guayaba::NodeRef<guayaba::Label> confirm_kicker() const;

private:
    struct Transition {
        bool active = false;
        bool screen_change = false;
        bool forward = true;
        double elapsed = 0.0;
        double duration = 0.18;
        guayaba::NodeRef<guayaba::Control> from;
        guayaba::NodeRef<guayaba::Control> to;
    };

    void show(UiScreen screen, bool animated = true, bool forward = true);
    void show_step_page(WizardStep step, bool animated = true);
    void refresh_screen(UiScreen screen);
    void set_status(std::string text, StatusLevel level);
    void refresh_main();
    void refresh_confirm();
    void refresh_progress();
    void refresh_result();
    void refresh_rail();
    void refresh_status();
    void focus_current_screen();
    void update_wizard_from_progress();
    void push_log(const std::string& line);
    void start_transition(guayaba::NodeRef<guayaba::Control> from,
                          guayaba::NodeRef<guayaba::Control> to,
                          bool screen_change, bool forward);
    void finish_transition();
    void sync_visibility();
    void focus_export_step(int delta);
    bool image_ready() const;
    bool is_step_done(WizardStep step) const;
    void advance_furthest(WizardStep step);
    void refresh_step_widget(StepSceneWidgets& widgets, WizardStep step);
    guayaba::NodeRef<guayaba::Control> panel_for(UiScreen screen) const;
    StepSceneWidgets& active_step_widgets();
    const StepSceneWidgets& active_step_widgets() const;
    RailMode rail_mode() const;
    std::string hint_text() const;
    const DeviceInfo* selected_device() const;
    PrepConfig config_for(const DeviceInfo& device, bool confirmed) const;

    IPrepController& controller_;
    UiState state_;
    std::string confirm_status_;
    std::vector<DeviceInfo> devices_;

    // Dispositivo confirmado al abrir la pantalla de confirmación. El flujo de
    // escritura usa esta instantánea y no la selección actual.
    std::optional<DeviceInfo> target_;
    std::string progress_notice_;
    int progress_notice_ticks_ = 0;

    std::unique_ptr<guayaba::Theme> theme_;

    RailSceneWidgets rail_;
    MainSceneWidgets main_;
    ConfirmSceneWidgets confirm_;
    StepSceneWidgets step_a_;
    StepSceneWidgets step_b_;
    ResultSceneWidgets result_;

    guayaba::NodeRef<guayaba::Label> status_;
    guayaba::NodeRef<guayaba::Control> rail_slot_;
    guayaba::NodeRef<guayaba::Control> stage_slot_;
    guayaba::NodeRef<guayaba::PanelContainer> main_root_;
    guayaba::NodeRef<guayaba::Control> steps_root_;
    guayaba::NodeRef<guayaba::PanelContainer> confirm_root_;
    guayaba::NodeRef<guayaba::PanelContainer> result_root_;

    int active_step_card_ = 0;
    WizardStep wizard_step_ = WizardStep::Detect;
    WizardStep furthest_step_ = WizardStep::Detect;
    bool result_ready_ = false;
    bool seen_building_ = false;
    double building_elapsed_ = 0.0;
    PrepStage last_stage_ = PrepStage::Idle;
    std::vector<std::string> step_log_;

    Transition transition_;
};

} // namespace booteable::ui

#endif // BOOTEABLE_UI_APPSCENE_HPP
