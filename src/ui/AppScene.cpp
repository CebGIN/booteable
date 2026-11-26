#include "ui/AppScene.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Control;
using guayaba::InputEvent;
using guayaba::Key;
using guayaba::Node;
using guayaba::Script;
using guayaba::SplitContainer;
using guayaba::SplitOrientation;
using guayaba::Vec2;

// Pantalla completa de la TUI. Antes de repartir el rect entre los hijos decide
// si el panel principal va apilado (80x24) o en dos columnas (120x40).
class AdaptiveScreen : public Control {
public:
    explicit AdaptiveScreen(const std::string& node_name)
        : Control(node_name) {}

    void set_split(SplitContainer* split) { split_ = split; }

    void on_update_layout(const guayaba::Rect& parent_rect) override {
        if (split_ != nullptr) {
            const bool wide = parent_rect.size.x >= 100;
            split_->set_orientation(wide ? SplitOrientation::VerticalDivider
                                         : SplitOrientation::HorizontalDivider);
            split_->set_split_ratio(wide ? 0.38f : 0.42f);
            split_->set_min_first(wide ? 30 : 0);
            split_->set_min_second(wide ? 34 : 0);
            split_->set_custom_minimum_size(wide ? Vec2{40, 11} : Vec2{40, 20});
        }
        Control::on_update_layout(parent_rect);
    }

private:
    SplitContainer* split_ = nullptr;
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
        elapsed_ = 0.0;
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
    root_ = root.get();

    guayaba::NodeRef<AdaptiveScreen> screen =
        root->add_child<AdaptiveScreen>("screen");
    screen->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    screen->set_clip_children(true);

    main_ = build_main_scene(screen, *this);
    confirm_ = build_confirm_dialog(screen, *this);
    progress_ = build_progress_scene(screen, *this);
    result_ = build_result_scene(screen, *this);
    screen->set_split(main_.split.get());

    root->attach_script<AppScript>(this);

    refresh();
    show(UiScreen::Main);
    return root;
}

void AppScene::handle_key(Key key) {
    switch (key) {
    case Key::F10:
        if (state_.screen == UiScreen::Progress) {
            state_.status = "Hay una preparación en curso; cancelá con Esc.";
            refresh_main();
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
            state_.status = "Lista de dispositivos actualizada.";
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

    state_.progress = controller_.progress();
    refresh_progress();

    if (controller_.running()) {
        return;
    }

    state_.result = controller_.result();
    if (state_.result.outcome == PrepOutcome::None) {
        state_.result.outcome = PrepOutcome::Cancelled;
        state_.result.message = "La preparación terminó sin un resultado explícito.";
    }
    show(UiScreen::Result);
    refresh_result();
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
    } else {
        if (state_.selected >= devices_.size()) {
            state_.selected = devices_.size() - 1;
        }
        state_.safety = controller_.safety(devices_[state_.selected]);
    }

    if (!state_.can_write) {
        state_.status = state_.write_reason;
    }

    refresh_main();
}

void AppScene::select(int index) {
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
    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        state_.status = "No hay dispositivos USB para preparar.";
        refresh_main();
        return;
    }

    state_.safety = controller_.safety(*device);
    if (state_.safety.verdict == SafetyVerdict::Rejected) {
        state_.status = state_.safety.reasons.empty()
                            ? "El destino fue rechazado por la política de seguridad."
                            : state_.safety.reasons.front();
        refresh_main();
        return;
    }
    if (!state_.can_write) {
        state_.status = state_.write_reason.empty()
                            ? "Se requieren privilegios para escribir la USB."
                            : state_.write_reason;
        refresh_main();
        return;
    }

    state_.confirmed = false;
    confirm_status_.clear();
    refresh_confirm();
    show(UiScreen::Confirm);
}

void AppScene::request_export() {
    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        state_.status = "No hay dispositivos para exportar.";
        refresh_main();
        return;
    }

    PrepConfig config = config_for_selected(true);
    std::error_code error_code;
    const std::filesystem::path directory = "build";
    std::filesystem::create_directories(directory, error_code);

    std::string name = "booteable-export";
    const std::string path = device->path;
    const std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < path.size()) {
        name += "-" + path.substr(slash + 1);
    }
    config.image_path = (directory / (name + ".img")).string();

    std::string error;
    if (controller_.export_image(config, error)) {
        state_.status = "Imagen exportada: " + config.image_path;
    } else {
        state_.status = "No se pudo exportar la imagen: " + error;
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

    const DeviceInfo* device = selected_device();
    if (device == nullptr) {
        confirm_status_ = "El dispositivo desapareció.";
        refresh_confirm();
        return;
    }

    PrepConfig config = config_for_selected(true);
    const ValidationReport report = controller_.validate(config);
    if (!report.ok()) {
        confirm_status_ = report.issues.front().message;
        refresh_confirm();
        return;
    }

    state_.progress = PrepProgress{};
    state_.result = PrepResult{};
    if (!controller_.start(config)) {
        state_.progress = controller_.progress();
        state_.result = controller_.result();
        show(UiScreen::Result);
        refresh_result();
        return;
    }

    state_.progress = controller_.progress();
    refresh_progress();
    show(UiScreen::Progress);
}

void AppScene::dismiss_confirmation() {
    state_.confirmed = false;
    confirm_status_.clear();
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
    state_.status.clear();
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

PrepConfig AppScene::config_for_selected(bool confirmed) const {
    PrepConfig config;
    if (const DeviceInfo* device = selected_device()) {
        config.device = *device;
    }
    config.volume_label = "BOOTEABLE";
    config.esp_bytes = 0;
    config.overwrite_confirmed = confirmed;
    return config;
}

void AppScene::show(UiScreen screen) {
    state_.screen = screen;

    if (main_.panel) {
        main_.panel->set_visible(screen == UiScreen::Main ||
                                 screen == UiScreen::Confirm);
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

    if (screen == UiScreen::Main) {
        state_.status = state_.status;
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
    const DeviceInfo* device = selected_device();
    refresh_confirm_dialog(confirm_, device != nullptr ? *device : fallback_device(),
                           state_.confirmed, confirm_status_);
}

void AppScene::refresh_progress() {
    const DeviceInfo* device = selected_device();
    refresh_progress_scene(progress_,
                           device != nullptr ? *device : fallback_device(),
                           state_.progress);
}

void AppScene::refresh_result() {
    const DeviceInfo* device = selected_device();
    refresh_result_scene(result_, device != nullptr ? *device : fallback_device(),
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
