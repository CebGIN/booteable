#include "ui/MainScene.hpp"

#include <string>
#include <utility>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "app/IPrepController.hpp"

namespace booteable::ui {
namespace {

using guayaba::Control;
using guayaba::InputEvent;
using guayaba::Key;
using guayaba::Label;
using guayaba::Node;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::SceneManager;
using guayaba::Script;
using guayaba::VBox;

class MainSceneScript : public Script<Node> {
public:
    MainSceneScript(IPrepController* controller, NodeRef<Label> summary,
                    NodeRef<Label> devices, NodeRef<Label> status)
        : controller_(controller),
          summary_(summary),
          devices_(devices),
          status_(status) {}

    void on_enter_tree() override {
        refresh();
    }

    void on_unhandled_input(const InputEvent& event) override {
        if (!event.is_key() || !event.pressed) {
            return;
        }
        switch (event.key) {
        case Key::F10:
        case Key::ESCAPE:
        case Key::Q:
            SceneManager::get_instance().stop_running();
            break;
        default:
            break;
        }
    }

private:
    void refresh() {
        if (controller_ == nullptr) {
            status_->set_text("Sin controlador.");
            return;
        }

        const std::vector<DeviceInfo> devices = controller_->devices();
        summary_->set_text("WP0 · esqueleto de la TUI sobre GuayabaTUIEngine");

        std::string list = "Dispositivos simulados: " + std::to_string(devices.size());
        for (const DeviceInfo& device : devices) {
            list += "\n  " + device.path + "  " + device.model;
        }
        devices_->set_text(list);

        status_->set_text("Contratos congelados en WP0 · F10 o Esc para salir");
    }

    IPrepController* controller_;
    NodeRef<Label> summary_;
    NodeRef<Label> devices_;
    NodeRef<Label> status_;
};

NodeRef<Label> add_label(const NodeRef<Control>& parent, const std::string& name,
                         const std::string& text) {
    NodeRef<Label> label = parent->add_child<Label>(name);
    label->set_text(text);
    label->set_fit_content(true);
    label->set_anchor_right(1.0f);
    label->set_grow_x(guayaba::Grow::Both);
    label->set_clip_children(true);
    return label;
}

} // namespace

std::unique_ptr<guayaba::Node> build_main_scene(IPrepController& controller) {
    auto root = std::make_unique<Node>("root");

    NodeRef<Control> screen = root->add_child<Control>("screen", guayaba::Vec2{0, 0});
    screen->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    screen->set_clip_children(true);

    NodeRef<PanelContainer> background =
        screen->add_child<PanelContainer>("background");
    background->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    background->get_stylebox().set_background_color("BLACK");

    NodeRef<VBox> layout = screen->add_child<VBox>("layout");
    layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    layout->set_separation(1);

    NodeRef<Label> title = add_label(layout, "title", "booteable");
    title->set_text_color("BRIGHT_WHITE");

    NodeRef<Label> rule = add_label(layout, "rule", std::string(64, '-'));
    rule->set_text_color("BRIGHT_BLACK");

    NodeRef<Label> summary = add_label(layout, "summary", "WP0");
    NodeRef<Label> devices = add_label(layout, "devices", "Dispositivos simulados: ...");
    NodeRef<Label> status = add_label(layout, "status", "");
    status->set_text_color("BRIGHT_BLACK");

    root->attach_script<MainSceneScript>(&controller, summary, devices, status);

    return root;
}

} // namespace booteable::ui
