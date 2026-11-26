#include "ui/StepScene.hpp"

#include <string>
#include <vector>

#include "ui/AppScene.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::ProgressBar;
using guayaba::Spinner;
using guayaba::VBox;

std::string join_log(const std::vector<std::string>& log) {
    std::string text;
    for (const std::string& line : log) {
        if (!text.empty()) {
            text += '\n';
        }
        text += line;
    }
    return text;
}

} // namespace

void bind_step_scene(NodeRef<Control> steps_root, const std::string& panel_name,
                     StepSceneWidgets& widgets, AppScene& app) {
    const std::string prefix = panel_name + "_";
    widgets.panel = steps_root->get_node<PanelContainer>(panel_name);
    NodeRef<VBox> layout =
        widgets.panel->get_node<VBox>(prefix + "layout");

    widgets.kicker = layout->get_node<Label>(prefix + "kicker");
    widgets.title = layout->get_node<Label>(prefix + "title");
    widgets.phase = layout->get_node<Label>(prefix + "phase");
    widgets.description = layout->get_node<Label>(prefix + "description");

    NodeRef<HBox> status_row =
        layout->get_node<HBox>(prefix + "status_row");
    widgets.spinner = status_row->get_node<Spinner>(prefix + "spinner");
    widgets.status = status_row->get_node<Label>(prefix + "status");

    widgets.bar = layout->get_node<ProgressBar>(prefix + "bar");
    widgets.dots = layout->get_node<Label>(prefix + "dots");
    widgets.log = layout->get_node<Label>(prefix + "log");

    NodeRef<HBox> buttons = layout->get_node<HBox>(prefix + "buttons");
    widgets.cancel = buttons->get_node<Button>(prefix + "cancel");

    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.cancel_preparation(); });
}

void refresh_step_scene(StepSceneWidgets& widgets, WizardStep step,
                        const PrepProgress& progress,
                        const std::vector<std::string>& log, bool running) {
    widgets.kicker->set_text(wizard_kicker(step));
    widgets.title->set_text(wizard_step_label(step));
    widgets.phase->set_text(wizard_phase_label(step));
    widgets.description->set_text(wizard_step_description(step));

    widgets.status->set_text(progress.detail.empty() ? "Esperando..."
                                                     : progress.detail);
    widgets.status->set_text_color("BRIGHT_WHITE");
    widgets.bar->set_value(static_cast<double>(progress.percent));
    widgets.dots->set_text(wizard_dots(step));
    widgets.log->set_text(join_log(log));

    widgets.spinner->set_running(running);
    widgets.spinner->set_visible(running);
}

} // namespace booteable::ui
