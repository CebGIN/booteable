#include "ui/MainScene.hpp"

#include <string>
#include <utility>
#include <vector>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::ListView;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::SplitContainer;
using guayaba::VBox;

} // namespace

void bind_main_scene(NodeRef<Control> content, MainSceneWidgets& widgets,
                     AppScene& app) {
    widgets.panel = content->get_node<PanelContainer>("main");

    NodeRef<VBox> layout = widgets.panel->get_node<VBox>("main_layout");
    widgets.split = layout->get_node<SplitContainer>("main_split");

    widgets.devices_panel = widgets.split->get_node<PanelContainer>("devices_panel");
    NodeRef<VBox> devices_layout =
        widgets.devices_panel->get_node<VBox>("devices_layout");
    widgets.count = devices_layout->get_node<Label>("devices_count");
    widgets.device_list = devices_layout->get_node<ListView>("device_list");

    widgets.details_panel = widgets.split->get_node<PanelContainer>("details_panel");
    NodeRef<VBox> details = widgets.details_panel->get_node<VBox>("details_layout");
    widgets.detail_title = details->get_node<Label>("detail_title");
    widgets.detail_model = details->get_node<Label>("detail_model");
    widgets.detail_capacity = details->get_node<Label>("detail_capacity");
    widgets.detail_serial = details->get_node<Label>("detail_serial");
    widgets.detail_by_id = details->get_node<Label>("detail_by_id");
    widgets.detail_mounts = details->get_node<Label>("detail_mounts");
    widgets.detail_safety = details->get_node<Label>("detail_safety");
    widgets.detail_image = details->get_node<Label>("detail_image");

    NodeRef<HBox> buttons = details->get_node<HBox>("detail_buttons");
    widgets.prepare = buttons->get_node<Button>("prepare");
    widgets.export_image = buttons->get_node<Button>("export");

    widgets.status = layout->get_node<Label>("status");

    widgets.device_list->connect_signal<int>(
        "selection_changed", [&app](int index) { app.select(index); });
    widgets.device_list->connect_signal<int>(
        "item_activated", [&app](int) { app.request_prepare(); });
    widgets.prepare->connect_signal("clicked",
                                    [&app] { app.request_prepare(); });
    widgets.export_image->connect_signal(
        "clicked", [&app] { app.request_export(); });
}

void refresh_main_scene(MainSceneWidgets& widgets,
                        const std::vector<DeviceInfo>& devices,
                        const UiState& state) {
    std::vector<std::string> rows;
    rows.reserve(devices.size());
    for (const DeviceInfo& device : devices) {
        rows.push_back(device_row(device));
    }
    widgets.device_list->set_items(std::move(rows));
    if (!devices.empty() &&
        state.selected < static_cast<std::size_t>(devices.size())) {
        widgets.device_list->set_selected(static_cast<int>(state.selected));
    } else {
        widgets.device_list->set_selected(-1);
    }

    widgets.count->set_text("USB detectadas: " + std::to_string(devices.size()));
    widgets.prepare->set_disabled(!state.can_write);
    widgets.export_image->set_disabled(devices.empty());

    std::string status = state.status;
    StatusLevel level = state.status_level;
    if (!state.can_write && !state.write_reason.empty() &&
        (status.empty() || level == StatusLevel::Hint)) {
        status = state.write_reason;
        level = StatusLevel::Warning;
    }
    if (status.empty()) {
        status = "F5 actualizar | Enter preparar | F8 exportar | F10 salir";
        level = StatusLevel::Hint;
    }
    widgets.status->set_text(status);
    widgets.status->set_text_color(status_color(level));

    if (devices.empty() ||
        state.selected >= static_cast<std::size_t>(devices.size())) {
        widgets.detail_title->set_text("Sin dispositivo seleccionado");
        widgets.detail_model->set_text("Modelo: -");
        widgets.detail_capacity->set_text("Capacidad: -");
        widgets.detail_serial->set_text("Serie: -");
        widgets.detail_by_id->set_text("by-id: -");
        widgets.detail_mounts->set_text("Montajes: -");
        widgets.detail_safety->set_text("Seguridad: -");
        widgets.detail_safety->set_text_color("BRIGHT_BLACK");
        widgets.detail_image->set_text("Imagen: -");
        return;
    }

    const DeviceInfo& device = devices[state.selected];
    widgets.detail_title->set_text(device.path + " | " +
                                   (device.model.empty() ? "USB" : device.model));
    widgets.detail_model->set_text("Modelo: " +
                                   (device.model.empty() ? "-" : device.model));
    widgets.detail_capacity->set_text(
        "Capacidad: " + format_bytes(device.size_bytes) + " (" +
        transport_label(device.transport) + ", sector " +
        std::to_string(device.logical_sector) + " B)");
    widgets.detail_serial->set_text(
        "Serie: " + (device.serial.empty() ? "(sin serie)" : device.serial));
    widgets.detail_by_id->set_text(
        "by-id: " + (device.by_id.empty() ? "(no resuelto)" : device.by_id));
    widgets.detail_mounts->set_text("Montajes: " + describe_mounts(device));
    widgets.detail_safety->set_text("Seguridad: " +
                                    verdict_label(state.safety.verdict));
    widgets.detail_safety->set_text_color(verdict_color(state.safety.verdict));
    widgets.detail_image->set_text(
        "Imagen: GPT | ESP FAT32 " +
        format_bytes(device.logical_sector == 4096 ? kDefaultEspBytes4Kn
                                                   : kDefaultEspBytes512) +
        " | etiqueta BOOTEABLE");
}

} // namespace booteable::ui
