#include "ui/ProgressScene.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::ButtonState;
using guayaba::Control;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::ProgressBar;
using guayaba::StyleBorder;
using guayaba::StyleBox;
using guayaba::VBox;
using guayaba::Vec2;

constexpr PrepStage kSteps[6] = {
    PrepStage::Building, PrepStage::Unmounting, PrepStage::Opening,
    PrepStage::Writing,  PrepStage::Syncing,    PrepStage::Verifying,
};

NodeRef<Label> add_row(const NodeRef<Control>& parent, const std::string& name,
                       const std::string& text, int height = 1) {
    NodeRef<Label> label = parent->add_child<Label>(name);
    label->set_text(text);
    label->set_custom_minimum_size(Vec2{0, height});
    label->set_anchor_right(1.0f);
    label->set_grow_x(guayaba::Grow::Both);
    label->set_clip_children(true);
    return label;
}

} // namespace

ProgressSceneWidgets build_progress_scene(
    guayaba::NodeRef<guayaba::Control> parent, AppScene& app) {
    ProgressSceneWidgets widgets;

    widgets.panel = parent->add_child<PanelContainer>("progress");
    widgets.panel->set_anchor_left(0.05f);
    widgets.panel->set_anchor_right(0.95f);
    widgets.panel->set_anchor_top(0.06f);
    widgets.panel->set_anchor_bottom(0.94f);
    StyleBox& box = widgets.panel->get_stylebox();
    box.set_background_color("BLACK");
    box.set_border_style(StyleBorder::ROUNDED);
    box.set_border_color("BRIGHT_CYAN");
    box.set_border_width(1);
    box.set_content_margin(1, 0, 1, 0);

    NodeRef<VBox> layout = widgets.panel->add_child<VBox>("progress_layout");
    layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    layout->set_separation(0);

    widgets.title = add_row(layout, "progress_title", "Preparando USB");
    widgets.title->set_text_color("BRIGHT_CYAN");

    widgets.detail = add_row(layout, "progress_detail", "Esperando...");
    widgets.detail->set_text_color("BRIGHT_WHITE");

    widgets.bar = layout->add_child<ProgressBar>("progress_bar");
    widgets.bar->set_range(0, 100);
    widgets.bar->set_show_percentage(true);
    widgets.bar->set_custom_minimum_size(Vec2{0, 3});
    widgets.bar->set_anchor_right(1.0f);
    widgets.bar->set_grow_x(guayaba::Grow::Both);
    widgets.bar->get_stylebox().set_border_style(StyleBorder::ASCII);
    widgets.bar->get_stylebox().set_border_color("BRIGHT_BLACK");
    widgets.bar->get_stylebox().set_border_width(1);
    widgets.bar->get_stylebox().set_content_margin(1, 0, 1, 0);
    widgets.bar->set_fill_color(guayaba::Color::from_name("BRIGHT_CYAN"));

    for (std::size_t index = 0; index < widgets.steps.size(); ++index) {
        widgets.steps[index] =
            add_row(layout, "step_" + std::to_string(index), "");
    }

    widgets.cancel = layout->add_child<Button>("progress_cancel");
    widgets.cancel->set_text("[ Cancelar ]");
    widgets.cancel->set_custom_minimum_size(Vec2{14, 3});
    StyleBox& normal = widgets.cancel->get_stylebox(ButtonState::Normal);
    normal.set_border_style(StyleBorder::ASCII);
    normal.set_border_color("BRIGHT_YELLOW");
    normal.set_border_width(1);
    normal.set_content_margin(1, 0, 1, 0);
    StyleBox& hover = widgets.cancel->get_stylebox(ButtonState::Hover);
    hover.set_border_style(StyleBorder::ASCII);
    hover.set_border_color("BRIGHT_YELLOW");
    hover.set_border_width(1);
    hover.set_content_margin(1, 0, 1, 0);
    hover.set_background_color("BRIGHT_BLACK");

    widgets.status = add_row(layout, "progress_status", "");
    widgets.status->set_text_color("BRIGHT_BLACK");

    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.cancel_preparation(); });

    return widgets;
}

void refresh_progress_scene(ProgressSceneWidgets& widgets,
                            const DeviceInfo& device,
                            const PrepProgress& progress) {
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

    widgets.status->set_text(progress.detail);
    widgets.status->set_text_color("BRIGHT_BLACK");
}

} // namespace booteable::ui
