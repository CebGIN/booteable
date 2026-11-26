#include "ui/ProgressScene.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::Control;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::ProgressBar;
using guayaba::VBox;

constexpr PrepStage kSteps[6] = {
    PrepStage::Building, PrepStage::Unmounting, PrepStage::Opening,
    PrepStage::Writing,  PrepStage::Syncing,    PrepStage::Verifying,
};

} // namespace

void bind_progress_scene(NodeRef<Control> content, ProgressSceneWidgets& widgets,
                         AppScene& app) {
    widgets.panel = content->get_node<PanelContainer>("progress");
    NodeRef<VBox> layout = widgets.panel->get_node<VBox>("progress_layout");

    widgets.title = layout->get_node<Label>("progress_title");
    widgets.detail = layout->get_node<Label>("progress_detail");
    widgets.bar = layout->get_node<ProgressBar>("progress_bar");

    for (std::size_t index = 0; index < widgets.steps.size(); ++index) {
        widgets.steps[index] =
            layout->get_node<Label>("step_" + std::to_string(index));
    }

    widgets.cancel = layout->get_node<Button>("progress_cancel");
    widgets.status = layout->get_node<Label>("progress_status");

    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.cancel_preparation(); });
}

void refresh_progress_scene(ProgressSceneWidgets& widgets,
                            const DeviceInfo& device,
                            const PrepProgress& progress,
                            const std::string& notice) {
    widgets.title->set_text("Preparando " + device.path);
    widgets.detail->set_text(stage_label(progress.stage));
    widgets.bar->set_value(static_cast<double>(progress.percent));

    for (std::size_t index = 0; index < widgets.steps.size(); ++index) {
        const std::string text = progress_step(progress.stage, kSteps[index]);
        widgets.steps[index]->set_text(text);
        if (text.rfind("[x]", 0) == 0) {
            widgets.steps[index]->set_text_color("BRIGHT_GREEN");
        } else if (text.rfind("[>]", 0) == 0) {
            widgets.steps[index]->set_text_color("BRIGHT_YELLOW");
        } else {
            widgets.steps[index]->set_text_color("BRIGHT_BLACK");
        }
    }

    if (!notice.empty()) {
        widgets.status->set_text(notice);
        widgets.status->set_text_color("BRIGHT_YELLOW");
    } else {
        widgets.status->set_text(progress.detail);
        widgets.status->set_text_color("BRIGHT_BLACK");
    }
}

} // namespace booteable::ui
