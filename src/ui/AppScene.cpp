#include "ui/AppScene.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ui/UiAssets.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::AnchorPreset;
using guayaba::Control;
using guayaba::InputEvent;
using guayaba::Key;
using guayaba::Node;
using guayaba::NodeRef;
using guayaba::Rect;
using guayaba::Script;
using guayaba::SplitContainer;
using guayaba::SplitOrientation;
using guayaba::Vec2;

// Pantalla completa de la TUI. Es el único nodo propio que sobrevive a la
// escena declarativa: el formato no permite registrar tipos, así que el layout
// responsive sigue en C++ y el contenido se carga desde booteable.gscene.
//
// El ratio solo se aplica al entrar en un breakpoint; si se reescribiera en cada
// pasada, el arrastre del divisor nunca se vería.
class AdaptiveScreen : public Control {
public:
    explicit AdaptiveScreen(const std::string& node_name)
        : Control(node_name) {}

    void set_split(NodeRef<SplitContainer> split) { split_ = split; }

    void on_update_layout(const Rect& parent_rect) override {
        if (auto* split = split_.get()) {
            const bool wide = parent_rect.size.x >= 100;
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
        elapsed_ += delta_time;
        if (elapsed_ < 0.1) {
            return;
        }
        elapsed_ -= 0.1;
        if (app_ != nullptr) {
            app_->tick();
        }
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

    guayaba::SceneAsset asset = build_ui_asset();
    theme_ = std::move(asset.theme);
    screen->add_child(std::move(asset.root));

    NodeRef<Control> content = screen->get_node<Control>("content");
    bind_main_scene(content, main_, *this);
    bind_confirm_dialog(content, confirm_, *this);
    bind_progress_scene(content, progress_, *this);
    bind_result_scene(content, result_, *this);

    screen->set_split(content->get_node<SplitContainer>(
        "main/main_layout/main_split"));

    root->attach_script<AppScript>(this);

    refresh();
    show(UiScreen::Main);
    return root;
}

void AppScene::handle_key(Key key) {
    switch (key) {
    case Key::F10:
        if (state_.screen == UiScreen::Progress) {
            progress_notice_ = "Hay una preparación en curso; cancelá con Esc.";
            progress_notice_ticks_ = 30;
            refresh_progress();
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
        } else {
            quit();
        }
        return;
    case Key::ENTER:
        if (state_.screen == UiScreen::Main) {
            request_prepare();
        } else if (state_.screen == UiScreen::Confirm) {
            accept_confirmation();
        } else if (state_.screen == UiScreen::Result) {
            leave_result();
        }
        return;
    case Key::F5:
        if (state_.screen == UiScreen::Main) {
            refresh();
            if (!devices_.empty() && state_.can_write) {
                set_status("Lista de dispositivos actualizada.", StatusLevel::Info);
            }
            refresh_main();
        }
        return;
    case Key::F8:
        if (state_.screen == UiScreen::Main) {
            request_export();
        }
        return;
    case Key::UP:
        if (state_.screen == UiScreen::Main && !devices_.empty()) {
            select(static_cast<int>(state_.selected) - 1);
        }
        return;
    case Key::DOWN:
        if (state_.screen == UiScreen::Main && !devices_.empty()) {
            select(static_cast<int>(state_.selected) + 1);
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

void AppScene::tick() {
    if (state_.screen != UiScreen::Progress) {
        return;
    }

    if (progress_notice_ticks_ > 0) {
        --progress_notice_ticks_;
    }

    if (controller_.running()) {
        state_.progress = controller_.progress();
        refresh_progress();
        return;
    }

    // El hilo terminó: se releen progreso y resultado juntos para no mostrar un
    // porcentaje anterior a la publicación final.
    state_.result = controller_.result();
    state_.progress = controller_.progress();
    if (state_.result.outcome == PrepOutcome::None) {
        state_.result.outcome = PrepOutcome::Cancelled;
        state_.result.message = "La preparación terminó sin un resultado explícito.";
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
}

void AppScene::select(int index) {
    if (state_.screen != UiScreen::Main) {
        return;
    }
    if (devices_.empty()) {
        return;
    }
    const int last = static_cast<int>(devices_.size()) - 1;
    const int clamped = std::max(0, std::min(index, last));
    state_.selected = static_cast<std::size_t>(clamped);
    state_.safety = controller_.safety(devices_[state_.selected]);
    refresh_main();
}

void AppScene::request_prepare() {
    if (state_.screen != UiScreen::Main) {
        return;
    }

    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        set_status("No hay dispositivos USB para preparar.", StatusLevel::Error);
        refresh_main();
        return;
    }

    state_.safety = controller_.safety(*device);
    if (state_.safety.verdict == SafetyVerdict::Rejected) {
        set_status(state_.safety.reasons.empty()
                       ? "El destino fue rechazado por la política de seguridad."
                       : state_.safety.reasons.front(),
                   StatusLevel::Error);
        refresh_main();
        return;
    }
    if (!state_.can_write) {
        set_status(state_.write_reason.empty()
                       ? "Se requieren privilegios para escribir la USB."
                       : state_.write_reason,
                   StatusLevel::Warning);
        refresh_main();
        return;
    }

    target_ = *device;
    state_.confirmed = false;
    confirm_status_.clear();
    set_status("", StatusLevel::Hint);
    refresh_confirm();
    show(UiScreen::Confirm);
}

void AppScene::request_export() {
    if (state_.screen != UiScreen::Main) {
        return;
    }

    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        set_status("No hay dispositivos para exportar.", StatusLevel::Error);
        refresh_main();
        return;
    }

    PrepConfig config = config_for(*device, true);
    std::string image_path;
    std::string error;
    if (controller_.export_default_image(config, image_path, error)) {
        set_status("Imagen exportada: " + image_path, StatusLevel::Success);
    } else {
        set_status("No se pudo exportar la imagen: " + error, StatusLevel::Error);
    }
    refresh_main();
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

    // La selección no debe cambiar tras abrir el diálogo, pero el dispositivo
    // puede haberse desconectado: se revalida la instantánea contra la lista
    // actual antes de escribir.
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
    refresh_progress();
    show(UiScreen::Progress);
}

void AppScene::dismiss_confirmation() {
    state_.confirmed = false;
    confirm_status_.clear();
    target_.reset();
    set_status("", StatusLevel::Hint);
    show(UiScreen::Main);
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
    set_status("", StatusLevel::Hint);
    show(UiScreen::Main);
    refresh();
}

void AppScene::quit() {
    guayaba::SceneManager::get_instance().stop_running();
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

void AppScene::set_status(std::string text, StatusLevel level) {
    state_.status = std::move(text);
    state_.status_level = level;
}

void AppScene::show(UiScreen screen) {
    state_.screen = screen;

    if (main_.panel) {
        main_.panel->set_visible(screen == UiScreen::Main);
    }
    if (confirm_.panel) {
        confirm_.panel->set_visible(screen == UiScreen::Confirm);
    }
    if (progress_.panel) {
        progress_.panel->set_visible(screen == UiScreen::Progress);
    }
    if (result_.panel) {
        result_.panel->set_visible(screen == UiScreen::Result);
    }

    if (screen != UiScreen::Progress) {
        progress_notice_.clear();
        progress_notice_ticks_ = 0;
    }

    if (screen == UiScreen::Result) {
        refresh_result();
    }
    focus_current_screen();
}

void AppScene::focus_current_screen() {
    switch (state_.screen) {
    case UiScreen::Main:
        if (main_.device_list) {
            main_.device_list->grab_focus();
        }
        break;
    case UiScreen::Confirm:
        if (confirm_.overwrite) {
            confirm_.overwrite->grab_focus();
        }
        break;
    case UiScreen::Progress:
        if (progress_.cancel) {
            progress_.cancel->grab_focus();
        }
        break;
    case UiScreen::Result:
        if (result_.back) {
            result_.back->grab_focus();
        }
        break;
    }
}

void AppScene::refresh_main() {
    refresh_main_scene(main_, devices_, state_);
}

void AppScene::refresh_confirm() {
    refresh_confirm_dialog(confirm_, target_.value_or(fallback_device()),
                           state_.confirmed, confirm_status_);
}

void AppScene::refresh_progress() {
    const std::string notice =
        progress_notice_ticks_ > 0 ? progress_notice_ : std::string{};
    refresh_progress_scene(progress_,
                           target_.value_or(fallback_device()),
                           state_.progress, notice);
}

void AppScene::refresh_result() {
    refresh_result_scene(result_, target_.value_or(fallback_device()),
                         state_.result);
}

guayaba::NodeRef<guayaba::ListView> AppScene::device_list() const {
    return main_.device_list;
}

guayaba::NodeRef<guayaba::Button> AppScene::prepare_button() const {
    return main_.prepare;
}

guayaba::NodeRef<guayaba::Button> AppScene::export_button() const {
    return main_.export_image;
}

guayaba::NodeRef<guayaba::CheckBox> AppScene::warning_checkbox() const {
    return confirm_.checkbox;
}

guayaba::NodeRef<guayaba::ProgressBar> AppScene::progress_bar() const {
    return progress_.bar;
}

guayaba::NodeRef<guayaba::SplitContainer> AppScene::split() const {
    return main_.split;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::devices_panel() const {
    return main_.devices_panel;
}

guayaba::NodeRef<guayaba::PanelContainer> AppScene::details_panel() const {
    return main_.details_panel;
}

guayaba::NodeRef<guayaba::Label> AppScene::result_outcome() const {
    return result_.outcome;
}

} // namespace booteable::ui
