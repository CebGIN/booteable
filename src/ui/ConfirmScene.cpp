#include "ui/ConfirmScene.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::CheckBox;
using guayaba::HBox;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::VBox;

} // namespace

void bind_confirm_scene(NodeRef<PanelContainer> root,
                        ConfirmSceneWidgets& widgets, AppScene& app) {
    widgets.panel = root;
    NodeRef<VBox> layout = root->get_node<VBox>("confirm_layout");

    widgets.kicker = layout->get_node<Label>("confirm_kicker");
    widgets.summary = layout->get_node<Label>("confirm_summary");
    widgets.image = layout->get_node<Label>("confirm_image");
    widgets.warning = layout->get_node<Label>("confirm_warning");
    widgets.checkbox = layout->get_node<CheckBox>("confirm_checkbox");

    NodeRef<HBox> buttons = layout->get_node<HBox>("confirm_buttons");
    widgets.back = buttons->get_node<Button>("confirm_back");
    widgets.overwrite = buttons->get_node<Button>("confirm_overwrite");
    widgets.status = layout->get_node<Label>("confirm_status");

    widgets.checkbox->connect_signal<bool>(
        "toggled", [&app](bool checked) { app.set_confirmed(checked); });
    widgets.back->connect_signal("clicked",
                                 [&app] { app.dismiss_confirmation(); });
    widgets.overwrite->connect_signal(
        "clicked", [&app] { app.accept_confirmation(); });
}

void refresh_confirm_scene(ConfirmSceneWidgets& widgets,
                           const DeviceInfo& device, bool confirmed,
                           const std::string& status) {
    widgets.summary->set_text(
        "Destino: " + device.path + " | " +
        (device.model.empty() ? "USB" : device.model) + " | " +
        format_bytes(device.size_bytes) + " | sector " +
        std::to_string(device.logical_sector) + " B");

    widgets.image->set_text(
        "Imagen: GPT | ESP FAT32 " +
        format_bytes(device.logical_sector == 4096 ? kDefaultEspBytes4Kn
                                                   : kDefaultEspBytes512) +
        " | etiqueta BOOTEABLE | EFI/BOOT/BOOTX64.EFI");

    widgets.warning->set_text(
        "Se perderá todo el contenido de " + device.path +
        ". Esta acción no se puede deshacer.");
    widgets.checkbox->set_text("Entiendo que se perderá todo el contenido de " +
                               device.path);
    widgets.checkbox->set_checked(confirmed);
    widgets.status->set_text(status);
}

} // namespace booteable::ui
