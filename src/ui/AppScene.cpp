#include "ui/AppScene.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "ui/UiAssets.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::AnchorPreset;
using guayaba::Button;
using guayaba::Control;
using guayaba::InputEvent;
using guayaba::Key;
using guayaba::Label;
using guayaba::Node;
using guayaba::NodeRef;
using guayaba::Rect;
using guayaba::Script;
using guayaba::SplitContainer;
using guayaba::SplitOrientation;
using guayaba::Vec2;

constexpr int kRailCollapsed = 5;
constexpr int kRailExpanded = 18;
constexpr double kTransitionSeconds = 0.18;
constexpr double kPartitionScreenSeconds = 0.6;

double ease_out_cubic(double value) {
    const double inverse = 1.0 - value;
    return 1.0 - inverse * inverse * inverse;
}

bool same(NodeRef<Control> left, NodeRef<Control> right) {
    return left.id() == right.id();
}

// Pantalla completa de la TUI. El ancho disponible para el split es el del
// terminal menos el rail, porque el rail es un slot fijo del shell.
class AdaptiveScreen : public Control {
public:
    explicit AdaptiveScreen(const std::string& node_name)
        : Control(node_name) {}

    void set_split(NodeRef<SplitContainer> split) { split_ = split; }
    void set_rail_slot(NodeRef<Control> rail_slot) { rail_slot_ = rail_slot; }

    void on_update_layout(const Rect& parent_rect) override {
        int rail_width = 0;
        if (const Control* slot = rail_slot_.get()) {
            rail_width = slot->get_custom_minimum_size().x;
        }
        const int available = parent_rect.size.x - rail_width;

        if (auto* split = split_.get()) {
            const bool wide = available >= 100;
            if (!initialized_ || wide != wide_) {
                initialized_ = true;
                wide_ = wide;
                split->set_orientation(wide ? SplitOrientation::VerticalDivider
                                            : SplitOrientation::HorizontalDivider);
                split->set_split_ratio(wide ? 0.38f : 0.42f);
            }
            split->set_min_first(wide ? 30 : 0);
            split->set_min_second(wide ? 34 : 0);
            split->set_custom_minimum_size(wide ? Vec2{40, 11} : Vec2{40, 20});
        }
        Control::on_update_layout(parent_rect);
    }

private:
    NodeRef<SplitContainer> split_;
    NodeRef<Control> rail_slot_;
    bool initialized_ = false;
    bool wide_ = false;
};

class AppScript : public Script<Node> {
public:
    explicit AppScript(AppScene* app) : app_(app) {}

    void on_enter_tree() override {
        if (app_ != nullptr) {
            app_->tick();
        }
    }

    void on_process(double delta_time) override {
        if (app_ == nullptr) {
            return;
        }
        // La transición y el temporizador de los pasos necesitan resolución de
        // frame; el sondeo del controlador sigue a 100 ms.
        app_->animate(delta_time);

        elapsed_ += delta_time;
        if (elapsed_ < 0.1) {
            return;
        }
        elapsed_ -= 0.1;
        app_->tick();
    }

    void on_unhandled_input(const InputEvent& event) override {
        if (app_ == nullptr || !event.is_key() || !event.pressed) {
            return;
        }
        app_->handle_key(event.key);
    }

private:
    AppScene* app_;
    double elapsed_ = 0.0;
};

DeviceInfo fallback_device() {
    DeviceInfo device;
    device.path = "(sin dispositivo)";
    return device;
}

} // namespace

AppScene::AppScene(IPrepController& controller) : controller_(controller) {}

std::unique_ptr<Node> AppScene::build() {
    auto root = std::make_unique<Node>("root");

    NodeRef<AdaptiveScreen> screen = root->add_child<AdaptiveScreen>("screen");
    screen->set_anchor_preset(AnchorPreset::FullRect);
    screen->set_clip_children(true);

    UiAssetBundle bundle = build_ui_assets();
    theme_ = std::move(bundle.theme);
    screen->add_child(take_root(bundle, "shell"));

    NodeRef<Control> content = screen->get_node<Control>("content");
    rail_slot_ = content->get_node<Control>("app_frame/rail_slot");
    stage_slot_ = content->get_node<Control>("app_frame/shell/stage_slot");
    status_ = content->get_node<Label>("app_frame/shell/status");

    rail_slot_->add_child(take_root(bundle, "rail"));
    stage_slot_->add_child(take_root(bundle, "main"));
    stage_slot_->add_child(take_root(bundle, "steps"));
    stage_slot_->add_child(take_root(bundle, "confirm"));
    stage_slot_->add_child(take_root(bundle, "result"));

    NodeRef<guayaba::PanelContainer> rail_root =
        rail_slot_->get_node<guayaba::PanelContainer>("rail");
    main_root_ = stage_slot_->get_node<guayaba::PanelContainer>("main");
    steps_root_ = stage_slot_->get_node<Control>("steps");
    confirm_root_ = stage_slot_->get_node<guayaba::PanelContainer>("confirm");
    result_root_ = stage_slot_->get_node<guayaba::PanelContainer>("result");

    bind_rail_scene(rail_root, rail_, *this);
    bind_main_scene(main_root_, main_, *this);
    bind_confirm_scene(confirm_root_, confirm_, *this);
    bind_step_scene(steps_root_, "step_a", step_a_, *this);
    bind_step_scene(steps_root_, "step_b", step_b_, *this);
    bind_result_scene(result_root_, result_, *this);

    screen->set_split(
        main_root_->get_node<SplitContainer>("main_layout/main_split"));
    screen->set_rail_slot(rail_slot_);

    root->attach_script<AppScript>(this);

    show(UiScreen::Main, false);
    refresh();
    return root;
}

void AppScene::handle_key(Key key) {
    switch (key) {
    case Key::F10:
        if (state_.screen == UiScreen::Progress) {
            progress_notice_ = "Hay una preparación en curso; cancelá con Esc.";
            progress_notice_ticks_ = 30;
            refresh_progress();
            refresh_status();
            return;
        }
        quit();
        return;

    case Key::ESCAPE:
        if (state_.screen == UiScreen::Confirm) {
            dismiss_confirmation();
        } else if (state_.screen == UiScreen::Progress) {
            cancel_preparation();
        } else if (state_.screen == UiScreen::Result) {
            leave_result();
        } else if (state_.export_page) {
            close_export_page();
        } else if (state_.rail_expanded) {
            toggle_rail();
        } else {
            quit();
        }
        return;

    case Key::ENTER:
        if (state_.screen == UiScreen::Main && !state_.export_page) {
            request_prepare();
        } else if (state_.screen == UiScreen::Confirm) {
            accept_confirmation();
        } else if (state_.screen == UiScreen::Result) {
            leave_result();
        }
        return;

    case Key::F5:
        if (state_.screen == UiScreen::Main) {
            request_refresh();
        }
        return;

    case Key::F8:
        if (state_.screen == UiScreen::Main) {
            open_export_page();
        }
        return;

    case Key::F9:
        toggle_rail();
        return;

    case Key::UP:
        if (state_.screen == UiScreen::Main) {
            if (state_.export_page) {
                focus_export_step(-1);
            } else if (!devices_.empty()) {
                select(static_cast<int>(state_.selected) - 1);
            }
        }
        return;

    case Key::DOWN:
        if (state_.screen == UiScreen::Main) {
            if (state_.export_page) {
                focus_export_step(1);
            } else if (!devices_.empty()) {
                select(static_cast<int>(state_.selected) + 1);
            }
        }
        return;

    case Key::SPACE:
        if (state_.screen == UiScreen::Confirm) {
            set_confirmed(!state_.confirmed);
        }
        return;

    default:
        return;
    }
}

void AppScene::animate(double delta_time) {
    // El layout ya corrió antes de on_process. Reaplicar las etiquetas del rail
    // hace que Button reajuste el wrap al ancho real del slot tras plegar o
    // desplegar; sin esto el texto queda partido en varias líneas.
    refresh_rail();

    if (transition_.active) {
        transition_.elapsed += std::max(0.0, delta_time);
        const double ratio =
            std::min(1.0, transition_.elapsed / transition_.duration);
        const double eased = ease_out_cubic(ratio);

        if (Control* to = transition_.to.get()) {
            if (transition_.forward) {
                to->set_anchor_left(static_cast<float>(1.0 - eased));
                to->set_anchor_right(static_cast<float>(2.0 - eased));
            } else {
                to->set_anchor_left(static_cast<float>(-1.0 + eased));
                to->set_anchor_right(static_cast<float>(eased));
            }
        }
        if (Control* from = transition_.from.get()) {
            if (transition_.forward) {
                from->set_anchor_left(static_cast<float>(-eased));
                from->set_anchor_right(static_cast<float>(1.0 - eased));
            } else {
                from->set_anchor_left(static_cast<float>(eased));
                from->set_anchor_right(static_cast<float>(1.0 + eased));
            }
        }

        if (ratio >= 1.0) {
            finish_transition();
        }
    }

    // Los pasos 2 y 3 no tienen señal propia en el controlador: Building cubre
    // ambos. La UI los separa por tiempo, pero cualquier etapa real los pisa y
    // nunca se marca un paso como hecho antes de que Building termine.
    if (state_.screen == UiScreen::Progress &&
        state_.progress.stage == PrepStage::Building) {
        building_elapsed_ += std::max(0.0, delta_time);
        if (wizard_step_ == WizardStep::Partition &&
            building_elapsed_ >= kPartitionScreenSeconds) {
            show_step_page(WizardStep::Format, true);
        }
    }
}

void AppScene::settle_transition() {
    if (!transition_.active) {
        return;
    }
    if (Control* to = transition_.to.get()) {
        to->set_anchor_left(0.0f);
        to->set_anchor_right(1.0f);
        to->set_anchor_top(0.0f);
        to->set_anchor_bottom(1.0f);
    }
    if (Control* from = transition_.from.get()) {
        from->set_anchor_left(0.0f);
        from->set_anchor_right(1.0f);
        from->set_anchor_top(0.0f);
        from->set_anchor_bottom(1.0f);
    }
    transition_.active = false;
    transition_.elapsed = transition_.duration;
    transition_.from = {};
    transition_.to = {};
    sync_visibility();
}

void AppScene::tick() {
    if (state_.screen != UiScreen::Progress) {
        return;
    }

    if (progress_notice_ticks_ > 0) {
        --progress_notice_ticks_;
    }

    if (controller_.running()) {
        state_.progress = controller_.progress();
        update_wizard_from_progress();
        refresh_progress();
        refresh_rail();
        refresh_status();
        return;
    }

    // El hilo terminó: se releen progreso y resultado juntos para no mostrar un
    // porcentaje anterior a la publicación final.
    state_.result = controller_.result();
    state_.progress = controller_.progress();
    if (state_.result.outcome == PrepOutcome::None) {
        state_.result.outcome = PrepOutcome::Cancelled;
        state_.result.message =
            "La preparación terminó sin un resultado explícito.";
    }
    progress_notice_.clear();
    progress_notice_ticks_ = 0;
    refresh_progress();
    show(UiScreen::Result);
}

void AppScene::refresh() {
    devices_ = controller_.devices();
    state_.can_write = controller_.can_write();
    state_.write_reason = controller_.write_block_reason();

    if (devices_.empty()) {
        state_.selected = 0;
        state_.safety = SafetyReport{};
        state_.safety.verdict = SafetyVerdict::Rejected;
        state_.safety.reasons.push_back("No se detectaron dispositivos USB.");
        set_status("No se detectaron dispositivos USB.", StatusLevel::Error);
    } else {
        if (state_.selected >= devices_.size()) {
            state_.selected = devices_.size() - 1;
        }
        state_.safety = controller_.safety(devices_[state_.selected]);
        if (!state_.can_write) {
            set_status(state_.write_reason.empty()
                           ? "Se requieren privilegios para escribir la USB."
                           : state_.write_reason,
                       StatusLevel::Warning);
        }
    }

    refresh_main();
    refresh_rail();
    refresh_status();
}

void AppScene::select(int index) {
    if (state_.screen != UiScreen::Main || state_.export_page) {
        return;
    }
    if (devices_.empty()) {
        return;
    }
    const int last = static_cast<int>(devices_.size()) - 1;
    const int clamped = std::max(0, std::min(index, last));
    state_.selected = static_cast<std::size_t>(clamped);
    state_.safety = controller_.safety(devices_[state_.selected]);
    state_.status.clear();
    refresh_main();
    refresh_rail();
    refresh_status();
}

void AppScene::request_prepare() {
    if (state_.screen != UiScreen::Main || state_.export_page) {
        return;
    }

    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        set_status("No hay dispositivos USB para preparar.", StatusLevel::Error);
        refresh_status();
        return;
    }

    state_.safety = controller_.safety(*device);
    if (state_.safety.verdict == SafetyVerdict::Rejected) {
        set_status(state_.safety.reasons.empty()
                       ? "El destino fue rechazado por la política de seguridad."
                       : state_.safety.reasons.front(),
                   StatusLevel::Error);
        refresh_main();
        refresh_status();
        return;
    }
    if (!state_.can_write) {
        set_status(state_.write_reason.empty()
                       ? "Se requieren privilegios para escribir la USB."
                       : state_.write_reason,
                   StatusLevel::Warning);
        refresh_main();
        refresh_status();
        return;
    }

    target_ = *device;
    state_.confirmed = false;
    confirm_status_.clear();
    set_status("", StatusLevel::Hint);
    refresh_confirm();
    show(UiScreen::Confirm);
}

void AppScene::request_export() { request_export_selected(); }

void AppScene::request_export_standard() {
    PrepConfig config;
    config.volume_label = "BOOTEABLE";
    config.esp_bytes = 0;
    config.overwrite_confirmed = true;

    std::string image_path;
    std::string error;
    if (controller_.export_default_image(config, image_path, error,
                                         ExportGeometry::Standard)) {
        set_status("Imagen exportada: " + image_path +
                       " (64 MiB, sector 512)",
                   StatusLevel::Success);
    } else {
        set_status("No se pudo exportar la imagen: " + error,
                   StatusLevel::Error);
    }
    state_.export_page = false;
    refresh_rail();
    refresh_status();
    if (main_.device_list) {
        main_.device_list->grab_focus();
    }
}

void AppScene::request_export_selected() {
    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        set_status("Elegí una USB para exportar con su geometría.",
                   StatusLevel::Warning);
        refresh_status();
        return;
    }

    PrepConfig config = config_for(*device, true);
    std::string image_path;
    std::string error;
    if (controller_.export_default_image(config, image_path, error,
                                         ExportGeometry::Selected)) {
        set_status("Imagen exportada: " + image_path + " (" +
                       format_bytes(device->size_bytes) + ", sector " +
                       std::to_string(device->logical_sector) + " B)",
                   StatusLevel::Success);
    } else {
        set_status("No se pudo exportar la imagen: " + error,
                   StatusLevel::Error);
    }
    state_.export_page = false;
    refresh_rail();
    refresh_status();
    if (main_.device_list) {
        main_.device_list->grab_focus();
    }
}

void AppScene::set_confirmed(bool confirmed) {
    if (state_.screen != UiScreen::Confirm) {
        return;
    }
    state_.confirmed = confirmed;
    if (confirmed) {
        confirm_status_.clear();
    }
    refresh_confirm();
}

void AppScene::accept_confirmation() {
    if (state_.screen != UiScreen::Confirm) {
        return;
    }
    if (!state_.confirmed) {
        confirm_status_ =
            "Marcá la casilla de advertencia antes de sobrescribir la USB.";
        refresh_confirm();
        return;
    }
    if (!target_) {
        confirm_status_ = "El dispositivo desapareció.";
        refresh_confirm();
        return;
    }

    // La selección no debe cambiar tras abrir la pantalla, pero el dispositivo
    // puede haberse desconectado: se revalida la instantánea antes de escribir.
    const auto found = std::find_if(
        devices_.begin(), devices_.end(),
        [this](const DeviceInfo& device) { return device.path == target_->path; });
    if (found == devices_.end()) {
        confirm_status_ = "El dispositivo desapareció.";
        refresh_confirm();
        return;
    }
    target_ = *found;
    state_.safety = controller_.safety(*target_);
    if (state_.safety.verdict == SafetyVerdict::Rejected) {
        confirm_status_ = state_.safety.reasons.empty()
                              ? "El destino fue rechazado por la política de seguridad."
                              : state_.safety.reasons.front();
        refresh_confirm();
        return;
    }

    PrepConfig config = config_for(*target_, true);
    const ValidationReport report = controller_.validate(config);
    if (!report.ok()) {
        confirm_status_ = report.issues.front().message;
        refresh_confirm();
        return;
    }

    state_.progress = PrepProgress{};
    state_.result = PrepResult{};
    progress_notice_.clear();
    progress_notice_ticks_ = 0;
    if (!controller_.start(config)) {
        state_.progress = controller_.progress();
        state_.result = controller_.result();
        show(UiScreen::Result);
        return;
    }

    state_.progress = controller_.progress();
    wizard_step_ = WizardStep::Partition;
    state_.wizard_step = wizard_step_;
    active_step_card_ = 0;
    seen_building_ = false;
    building_elapsed_ = 0.0;
    last_stage_ = PrepStage::Idle;
    step_log_.clear();
    push_log("Preparando " + target_->path);
    refresh_progress();
    show(UiScreen::Progress);
}

void AppScene::dismiss_confirmation() {
    state_.confirmed = false;
    confirm_status_.clear();
    target_.reset();
    set_status("", StatusLevel::Hint);
    show(UiScreen::Main, true, false);
}

void AppScene::cancel_preparation() {
    if (state_.screen != UiScreen::Progress) {
        return;
    }
    controller_.cancel();
    state_.progress = controller_.progress();
    refresh_progress();
    tick();
}

void AppScene::leave_result() {
    state_.result = PrepResult{};
    target_.reset();
    state_.confirmed = false;
    state_.status.clear();
    wizard_step_ = WizardStep::Detect;
    state_.wizard_step = wizard_step_;
    active_step_card_ = 0;
    seen_building_ = false;
    building_elapsed_ = 0.0;
    last_stage_ = PrepStage::Idle;
    step_log_.clear();
    show(UiScreen::Main, true, false);
    refresh();
}

void AppScene::quit() {
    guayaba::SceneManager::get_instance().stop_running();
}

void AppScene::toggle_rail() {
    state_.rail_expanded = !state_.rail_expanded;
    if (!state_.rail_expanded) {
        state_.export_page = false;
    }
    refresh_rail();
    refresh_status();
}

void AppScene::open_export_page() {
    if (state_.screen != UiScreen::Main) {
        return;
    }
    state_.export_page = true;
    state_.rail_expanded = true;
    state_.status.clear();
    refresh_rail();
    refresh_status();
    if (rail_.export_64) {
        rail_.export_64->grab_focus();
    }
}

void AppScene::close_export_page() {
    state_.export_page = false;
    refresh_rail();
    refresh_status();
    if (main_.device_list) {
        main_.device_list->grab_focus();
    }
}

void AppScene::rail_back() {
    if (state_.export_page) {
        close_export_page();
        return;
    }
    if (state_.screen == UiScreen::Confirm) {
        dismiss_confirmation();
        return;
    }
    if (state_.screen == UiScreen::Result) {
        leave_result();
        return;
    }
    if (state_.screen == UiScreen::Main && state_.rail_expanded) {
        toggle_rail();
    }
}

void AppScene::request_refresh() {
    if (state_.screen != UiScreen::Main) {
        return;
    }
    refresh();
    if (!devices_.empty() && state_.can_write) {
        set_status("Lista de dispositivos actualizada.", StatusLevel::Info);
        refresh_status();
    }
}

void AppScene::show(UiScreen next, bool animated, bool forward) {
    if (state_.screen == next) {
        refresh_screen(next);
        refresh_rail();
        refresh_status();
        sync_visibility();
        return;
    }

    if (transition_.active) {
        finish_transition();
    }

    NodeRef<Control> from = panel_for(state_.screen);
    state_.screen = next;
    if (next != UiScreen::Main) {
        state_.export_page = false;
    }
    state_.status.clear();
    state_.status_level = StatusLevel::Hint;

    if (next == UiScreen::Progress) {
        if (step_a_.panel) {
            step_a_.panel->set_visible(active_step_card_ == 0);
        }
        if (step_b_.panel) {
            step_b_.panel->set_visible(active_step_card_ == 1);
        }
    }

    refresh_screen(next);
    NodeRef<Control> to = panel_for(next);

    if (!animated || !from || !to || from.id() == to.id()) {
        sync_visibility();
    } else {
        main_root_->set_visible(same(from, main_root_) || same(to, main_root_));
        steps_root_->set_visible(same(from, steps_root_) || same(to, steps_root_));
        confirm_root_->set_visible(same(from, confirm_root_) ||
                                   same(to, confirm_root_));
        result_root_->set_visible(same(from, result_root_) ||
                                  same(to, result_root_));
        if (steps_root_->is_visible()) {
            if (step_a_.panel) {
                step_a_.panel->set_visible(active_step_card_ == 0);
            }
            if (step_b_.panel) {
                step_b_.panel->set_visible(active_step_card_ == 1);
            }
        }
        start_transition(from, to, true, forward);
    }

    refresh_rail();
    refresh_status();
    focus_current_screen();
}

void AppScene::show_step_page(WizardStep step, bool animated) {
    if (transition_.active) {
        finish_transition();
    }

    if (wizard_step_ == step && state_.screen == UiScreen::Progress) {
        refresh_progress();
        return;
    }

    wizard_step_ = step;
    state_.wizard_step = step;

    if (state_.screen != UiScreen::Progress) {
        refresh_progress();
        return;
    }

    const int target_index = active_step_card_ == 0 ? 1 : 0;
    StepSceneWidgets& target = target_index == 0 ? step_a_ : step_b_;
    StepSceneWidgets& current = active_step_card_ == 0 ? step_a_ : step_b_;

    refresh_step_scene(target, step, state_.progress, step_log_,
                       controller_.running());

    if (animated && current.panel && target.panel) {
        target.panel->set_visible(true);
        current.panel->set_visible(true);
        active_step_card_ = target_index;
        start_transition(current.panel, target.panel, false, true);
    } else {
        active_step_card_ = target_index;
        sync_visibility();
    }

    refresh_status();
    focus_current_screen();
}

void AppScene::refresh_screen(UiScreen screen) {
    switch (screen) {
    case UiScreen::Main:
        refresh_main();
        break;
    case UiScreen::Confirm:
        refresh_confirm();
        break;
    case UiScreen::Progress:
        refresh_progress();
        break;
    case UiScreen::Result:
        refresh_result();
        break;
    }
}

void AppScene::set_status(std::string text, StatusLevel level) {
    state_.status = std::move(text);
    state_.status_level = level;
}

void AppScene::refresh_main() { refresh_main_scene(main_, devices_, state_); }

void AppScene::refresh_confirm() {
    refresh_confirm_scene(confirm_, target_.value_or(fallback_device()),
                          state_.confirmed, confirm_status_);
}

void AppScene::refresh_progress() {
    StepSceneWidgets& widgets = active_step_widgets();
    refresh_step_scene(widgets, wizard_step_, state_.progress, step_log_,
                       controller_.running() &&
                           state_.screen == UiScreen::Progress);
    if (progress_notice_ticks_ > 0 && !progress_notice_.empty()) {
        widgets.status->set_text(progress_notice_);
        widgets.status->set_text_color("BRIGHT_YELLOW");
    }
}

void AppScene::refresh_result() {
    refresh_result_scene(result_, target_.value_or(fallback_device()),
                         state_.result);
}

void AppScene::refresh_rail() {
    if (!rail_slot_) {
        return;
    }
    rail_slot_->set_custom_minimum_size(
        Vec2{state_.rail_expanded ? kRailExpanded : kRailCollapsed, 0});

    RailSceneState rail_state;
    rail_state.mode = rail_mode();
    rail_state.expanded = state_.rail_expanded;
    if (const DeviceInfo* device = selected_device()) {
        rail_state.has_device = true;
        rail_state.device_path = device->path;
    }
    refresh_rail_scene(rail_, rail_state);
}

void AppScene::refresh_status() {
    if (!status_) {
        return;
    }

    std::string text = state_.status;
    StatusLevel level = state_.status_level;
    if (text.empty()) {
        if (state_.screen == UiScreen::Main && !state_.can_write &&
            !state_.write_reason.empty()) {
            text = state_.write_reason;
            level = StatusLevel::Warning;
        } else {
            text = hint_text();
            level = StatusLevel::Hint;
        }
    }
    status_->set_text(text);
    status_->set_text_color(status_color(level));
}

void AppScene::focus_current_screen() {
    switch (state_.screen) {
    case UiScreen::Main:
        if (state_.export_page) {
            if (rail_.export_64) {
                rail_.export_64->grab_focus();
            }
        } else if (main_.device_list) {
            main_.device_list->grab_focus();
        }
        break;
    case UiScreen::Confirm:
        if (confirm_.overwrite) {
            confirm_.overwrite->grab_focus();
        }
        break;
    case UiScreen::Progress: {
        StepSceneWidgets& widgets = active_step_widgets();
        if (widgets.cancel) {
            widgets.cancel->grab_focus();
        }
        break;
    }
    case UiScreen::Result:
        if (result_.back) {
            result_.back->grab_focus();
        }
        break;
    }
}

void AppScene::update_wizard_from_progress() {
    const PrepStage stage = state_.progress.stage;

    if (stage != last_stage_) {
        if (stage != PrepStage::Idle) {
            push_log(stage_label(stage));
        }
        last_stage_ = stage;
    }

    if (stage != PrepStage::Building) {
        building_elapsed_ = 0.0;
    }

    if (stage == PrepStage::Building) {
        if (!seen_building_) {
            seen_building_ = true;
        }
        if (wizard_step_ == WizardStep::Detect) {
            show_step_page(WizardStep::Partition, true);
        }
        return;
    }

    if (stage == PrepStage::Validating && !seen_building_) {
        if (wizard_step_ == WizardStep::Detect) {
            show_step_page(WizardStep::Partition, true);
        }
        return;
    }

    if (stage == PrepStage::Validating && seen_building_) {
        show_step_page(WizardStep::Write, true);
        return;
    }

    if (stage == PrepStage::Unmounting || stage == PrepStage::Opening ||
        stage == PrepStage::Writing) {
        show_step_page(WizardStep::Write, true);
        return;
    }

    if (stage == PrepStage::Syncing) {
        show_step_page(WizardStep::Sync, true);
        return;
    }

    if (stage == PrepStage::Verifying) {
        show_step_page(WizardStep::Verify, true);
    }
}

void AppScene::push_log(const std::string& line) {
    step_log_.push_back(line);
    if (step_log_.size() > 4) {
        step_log_.erase(step_log_.begin());
    }
}

void AppScene::start_transition(NodeRef<Control> from, NodeRef<Control> to,
                                bool screen_change, bool forward) {
    transition_.active = true;
    transition_.screen_change = screen_change;
    transition_.forward = forward;
    transition_.elapsed = 0.0;
    transition_.from = from;
    transition_.to = to;

    if (Control* from_control = from.get()) {
        from_control->set_anchor_top(0.0f);
        from_control->set_anchor_bottom(1.0f);
        from_control->set_anchor_left(0.0f);
        from_control->set_anchor_right(1.0f);
    }
    if (Control* to_control = to.get()) {
        to_control->set_anchor_top(0.0f);
        to_control->set_anchor_bottom(1.0f);
        if (forward) {
            to_control->set_anchor_left(1.0f);
            to_control->set_anchor_right(2.0f);
        } else {
            to_control->set_anchor_left(-1.0f);
            to_control->set_anchor_right(0.0f);
        }
    }
}

void AppScene::finish_transition() {
    if (!transition_.active) {
        return;
    }

    if (Control* to = transition_.to.get()) {
        to->set_anchor_left(0.0f);
        to->set_anchor_right(1.0f);
        to->set_anchor_top(0.0f);
        to->set_anchor_bottom(1.0f);
    }
    if (Control* from = transition_.from.get()) {
        from->set_anchor_left(0.0f);
        from->set_anchor_right(1.0f);
        from->set_anchor_top(0.0f);
        from->set_anchor_bottom(1.0f);
    }

    transition_.active = false;
    transition_.screen_change = false;
    transition_.elapsed = 0.0;
    transition_.from = {};
    transition_.to = {};

    sync_visibility();
}

void AppScene::sync_visibility() {
    if (!main_root_) {
        return;
    }

    main_root_->set_visible(state_.screen == UiScreen::Main);
    steps_root_->set_visible(state_.screen == UiScreen::Progress);
    confirm_root_->set_visible(state_.screen == UiScreen::Confirm);
    result_root_->set_visible(state_.screen == UiScreen::Result);

    const bool progress = state_.screen == UiScreen::Progress;
    if (step_a_.panel) {
        step_a_.panel->set_visible(progress && active_step_card_ == 0);
    }
    if (step_b_.panel) {
        step_b_.panel->set_visible(progress && active_step_card_ == 1);
    }

    const NodeRef<Control> panels[] = {main_root_, steps_root_, confirm_root_,
                                       result_root_, step_a_.panel,
                                       step_b_.panel};
    for (const NodeRef<Control>& panel : panels) {
        if (Control* control = panel.get()) {
            control->set_anchor_left(0.0f);
            control->set_anchor_right(1.0f);
            control->set_anchor_top(0.0f);
            control->set_anchor_bottom(1.0f);
        }
    }
}

NodeRef<Control> AppScene::panel_for(UiScreen screen) const {
    switch (screen) {
    case UiScreen::Main:
        return main_root_;
    case UiScreen::Confirm:
        return confirm_root_;
    case UiScreen::Progress:
        return steps_root_;
    case UiScreen::Result:
        return result_root_;
    }
    return {};
}

StepSceneWidgets& AppScene::active_step_widgets() {
    return active_step_card_ == 0 ? step_a_ : step_b_;
}

const StepSceneWidgets& AppScene::active_step_widgets() const {
    return active_step_card_ == 0 ? step_a_ : step_b_;
}

RailMode AppScene::rail_mode() const {
    if (state_.screen == UiScreen::Main) {
        return state_.export_page ? RailMode::ExportPage : RailMode::MainTools;
    }
    switch (state_.screen) {
    case UiScreen::Confirm:
        return RailMode::Confirm;
    case UiScreen::Progress:
        return RailMode::Progress;
    case UiScreen::Result:
        return RailMode::Result;
    default:
        return RailMode::MainTools;
    }
}

std::string AppScene::hint_text() const {
    switch (state_.screen) {
    case UiScreen::Main:
        if (state_.export_page) {
            return "Arriba/Abajo elegir · Enter exportar · Esc volver";
        }
        return state_.rail_expanded
                   ? "F5 actualizar · F8 exportar · F9 plegar · F10 salir"
                   : "E exportar · A actualizar · S salir · F9 desplegar";
    case UiScreen::Confirm:
        return "Espacio marca · Enter sobrescribe · Esc vuelve";
    case UiScreen::Progress:
        return "Esc cancela · F10 bloqueado durante la preparación";
    case UiScreen::Result:
        return "Enter vuelve · F10 sale";
    }
    return "";
}

const DeviceInfo* AppScene::selected_device() const {
    if (devices_.empty() || state_.selected >= devices_.size()) {
        return nullptr;
    }
    return &devices_[state_.selected];
}

PrepConfig AppScene::config_for(const DeviceInfo& device, bool confirmed) const {
    PrepConfig config;
    config.device = device;
    config.volume_label = "BOOTEABLE";
    config.esp_bytes = 0;
    config.overwrite_confirmed = confirmed;
    return config;
}

void AppScene::focus_export_step(int delta) {
    const NodeRef<Button> buttons[] = {rail_.export_64, rail_.export_selected,
                                       rail_.export_back};
    int current = 0;
    for (int index = 0; index < 3; ++index) {
        if (buttons[index] && buttons[index]->is_focused()) {
            current = index;
        }
    }
    for (int attempt = 1; attempt <= 3; ++attempt) {
        const int next = ((current + delta * attempt) % 3 + 3) % 3;
        if (buttons[next] && !buttons[next]->is_disabled()) {
            buttons[next]->grab_focus();
            return;
        }
    }
}

guayaba::NodeRef<guayaba::ListView> AppScene::device_list() const {
    return main_.device_list;
}

guayaba::NodeRef<guayaba::Button> AppScene::prepare_button() const {
    return main_.prepare;
}

guayaba::NodeRef<guayaba::Button> AppScene::export_button() const {
    return rail_.export_button;
}

guayaba::NodeRef<guayaba::Button> AppScene::export_standard_button() const {
    return rail_.export_64;
}

guayaba::NodeRef<guayaba::Button> AppScene::export_selected_button() const {
    return rail_.export_selected;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::rail() const {
    return rail_.panel;
}

guayaba::NodeRef<guayaba::Control> AppScene::rail_slot() const {
    return rail_slot_;
}

guayaba::NodeRef<guayaba::Control> AppScene::stage() const {
    return stage_slot_;
}

guayaba::NodeRef<guayaba::Label> AppScene::status_label() const {
    return status_;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::step_panel() const {
    return active_step_widgets().panel;
}

guayaba::NodeRef<guayaba::Label> AppScene::step_kicker() const {
    return active_step_widgets().kicker;
}

guayaba::NodeRef<guayaba::Label> AppScene::step_title() const {
    return active_step_widgets().title;
}

guayaba::NodeRef<guayaba::Label> AppScene::step_status() const {
    return active_step_widgets().status;
}

guayaba::NodeRef<guayaba::Label> AppScene::step_log() const {
    return active_step_widgets().log;
}

guayaba::NodeRef<guayaba::Button> AppScene::cancel_button() const {
    return active_step_widgets().cancel;
}

guayaba::NodeRef<guayaba::CheckBox> AppScene::warning_checkbox() const {
    return confirm_.checkbox;
}

guayaba::NodeRef<guayaba::ProgressBar> AppScene::progress_bar() const {
    return active_step_widgets().bar;
}

guayaba::NodeRef<guayaba::SplitContainer> AppScene::split() const {
    return main_.split;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::main_panel() const {
    return main_root_;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::devices_panel() const {
    return main_.devices_panel;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::details_panel() const {
    return main_.details_panel;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::confirm_panel() const {
    return confirm_root_;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::result_panel() const {
    return result_root_;
}

guayaba::NodeRef<guayaba::Label> AppScene::result_outcome() const {
    return result_.outcome;
}

guayaba::NodeRef<guayaba::Label> AppScene::result_kicker() const {
    return result_.kicker;
}

guayaba::NodeRef<guayaba::Label> AppScene::confirm_kicker() const {
    return confirm_.kicker;
}

} // namespace booteable::ui
