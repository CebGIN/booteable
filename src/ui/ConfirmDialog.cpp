#include "ui/ConfirmDialog.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::CheckBox;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::VBox;

} // namespace

void bind_confirm_dialog(NodeRef<Control> content, ConfirmDialogWidgets& widgets,
                         AppScene& app) {
    widgets.panel = content->get_node<PanelContainer>("confirm");
    NodeRef<VBox> layout = widgets.panel->get_node<VBox>("confirm_layout");

    widgets.title = layout->get_node<Label>("confirm_title");
    widgets.summary = layout->get_node<Label>("confirm_summary");
    widgets.warning = layout->get_node<Label>("confirm_warning");
    widgets.checkbox = layout->get_node<CheckBox>("confirm_checkbox");

    NodeRef<HBox> buttons = layout->get_node<HBox>("confirm_buttons");
    widgets.cancel = buttons->get_node<Button>("confirm_cancel");
    widgets.overwrite = buttons->get_node<Button>("confirm_overwrite");
    widgets.status = layout->get_node<Label>("confirm_status");

    widgets.checkbox->connect_signal<bool>(
        "toggled", [&app](bool checked) { app.set_confirmed(checked); });
    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.dismiss_confirmation(); });
    widgets.overwrite->connect_signal(
        "clicked", [&app] { app.accept_confirmation(); });
}

void refresh_confirm_dialog(ConfirmDialogWidgets& widgets,
                            const DeviceInfo& device, bool confirmed,
                            const std::string& status) {
    widgets.summary->set_text(
        "Destino: " + device.path + " | " +
        (device.model.empty() ? "USB" : device.model) + " | " +
        format_bytes(device.size_bytes));
    widgets.warning->set_text(
        "Se perderá todo el contenido de " + device.path +
        ". Esta acción no se puede deshacer.");
    widgets.checkbox->set_text("Entiendo que se perderá todo el contenido de " +
                               device.path);
    widgets.checkbox->set_checked(confirmed);
    widgets.status->set_text(status);
}

} // namespace booteable::ui
