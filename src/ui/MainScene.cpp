#include "ui/MainScene.hpp"

#include <string>
#include <utility>
#include <vector>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::ButtonState;
using guayaba::Control;
using guayaba::Grow;
using guayaba::HBox;
using guayaba::Label;
using guayaba::ListView;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::StyleBorder;
using guayaba::StyleBox;
using guayaba::VBox;
using guayaba::Vec2;

NodeRef<Label> add_row(const NodeRef<Control>& parent, const std::string& name,
                       const std::string& text, int height = 1) {
    NodeRef<Label> label = parent->add_child<Label>(name);
    label->set_text(text);
    label->set_custom_minimum_size(Vec2{0, height});
    label->set_anchor_right(1.0f);
    label->set_grow_x(Grow::Both);
    label->set_clip_children(true);
    return label;
}

void style_button(const NodeRef<Button>& button) {
    StyleBox& normal = button->get_stylebox(ButtonState::Normal);
    normal.set_border_style(StyleBorder::ASCII);
    normal.set_border_color("BRIGHT_BLACK");
    normal.set_border_width(1);
    normal.set_content_margin(1, 0, 1, 0);

    StyleBox& hover = button->get_stylebox(ButtonState::Hover);
    hover.set_border_style(StyleBorder::ASCII);
    hover.set_border_color("BRIGHT_CYAN");
    hover.set_border_width(1);
    hover.set_content_margin(1, 0, 1, 0);
    hover.set_background_color("BRIGHT_BLACK");
}

void style_panel(const NodeRef<PanelContainer>& panel) {
    StyleBox& box = panel->get_stylebox();
    box.set_background_color("BLACK");
    box.set_content_margin(1, 0, 1, 0);
}

} // namespace

MainSceneWidgets build_main_scene(guayaba::NodeRef<guayaba::Control> parent,
                                  AppScene& app) {
    MainSceneWidgets widgets;

    widgets.panel = parent->add_child<PanelContainer>("main");
    widgets.panel->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    style_panel(widgets.panel);

    NodeRef<VBox> layout = widgets.panel->add_child<VBox>("main_layout");
    layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    layout->set_separation(0);

    NodeRef<Label> title =
        add_row(layout, "app_title", "booteable | Preparador UEFI");
    title->set_text_color("BRIGHT_CYAN");

    NodeRef<Label> rule = add_row(
        layout, "app_rule",
        "----------------------------------------------------------------");
    rule->set_text_color("BRIGHT_BLACK");

    widgets.split = layout->add_child<guayaba::SplitContainer>("main_split");
    widgets.split->set_grow_y(Grow::Both);
    widgets.split->set_divider_width(1);
    widgets.split->get_divider_stylebox().set_background_color("BRIGHT_BLACK");

    widgets.devices_panel = widgets.split->add_child<PanelContainer>("devices_panel");
    style_panel(widgets.devices_panel);
    NodeRef<VBox> devices_layout =
        widgets.devices_panel->add_child<VBox>("devices_layout");
    devices_layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    devices_layout->set_separation(0);

    widgets.count = add_row(devices_layout, "devices_count", "USB detectadas: 0");
    widgets.count->set_text_color("BRIGHT_WHITE");

    widgets.device_list = devices_layout->add_child<ListView>("device_list");
    widgets.device_list->set_custom_minimum_size(Vec2{12, 5});
    widgets.device_list->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    widgets.device_list->set_selection_background_color("BRIGHT_BLUE");
    widgets.device_list->set_selection_text_color("BRIGHT_WHITE");
    widgets.device_list->set_text_color("WHITE");
    widgets.device_list->get_stylebox().set_border_style(StyleBorder::ASCII);
    widgets.device_list->get_stylebox().set_border_color("BRIGHT_BLACK");
    widgets.device_list->get_stylebox().set_border_width(1);
    widgets.device_list->get_stylebox().set_content_margin(1, 0, 1, 0);

    widgets.details_panel = widgets.split->add_child<PanelContainer>("details_panel");
    style_panel(widgets.details_panel);
    NodeRef<VBox> details =
        widgets.details_panel->add_child<VBox>("details_layout");
    details->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    details->set_separation(0);

    widgets.detail_title =
        add_row(details, "detail_title", "Sin dispositivo seleccionado");
    widgets.detail_title->set_text_color("BRIGHT_WHITE");
    widgets.detail_model = add_row(details, "detail_model", "Modelo: -");
    widgets.detail_capacity = add_row(details, "detail_capacity", "Capacidad: -");
    widgets.detail_serial = add_row(details, "detail_serial", "Serie: -");
    widgets.detail_by_id = add_row(details, "detail_by_id", "by-id: -");
    widgets.detail_by_id->set_text_color("BRIGHT_BLACK");
    widgets.detail_mounts = add_row(details, "detail_mounts", "Montajes: -");
    widgets.detail_safety = add_row(details, "detail_safety", "Seguridad: -");
    widgets.detail_image = add_row(
        details, "detail_image",
        "Imagen: GPT | ESP FAT32 | etiqueta BOOTEABLE");

    NodeRef<HBox> buttons = details->add_child<HBox>("detail_buttons");
    buttons->set_custom_minimum_size(Vec2{0, 3});
    buttons->set_separation(2);

    widgets.prepare = buttons->add_child<Button>("prepare");
    widgets.prepare->set_text("[ Preparar USB ]");
    widgets.prepare->set_custom_minimum_size(Vec2{18, 3});
    style_button(widgets.prepare);

    widgets.export_image = buttons->add_child<Button>("export");
    widgets.export_image->set_text("[ Exportar imagen ]");
    widgets.export_image->set_custom_minimum_size(Vec2{20, 3});
    style_button(widgets.export_image);

    widgets.status = add_row(layout, "status", "");
    widgets.status->set_text_color("BRIGHT_BLACK");

    widgets.device_list->connect_signal<int>(
        "selection_changed", [&app](int index) { app.select(index); });
    widgets.device_list->connect_signal<int>(
        "item_activated", [&app](int) { app.request_prepare(); });
    widgets.prepare->connect_signal("clicked",
                                    [&app] { app.request_prepare(); });
    widgets.export_image->connect_signal(
        "clicked", [&app] { app.request_export(); });

    return widgets;
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
    if (!state.can_write && !state.write_reason.empty()) {
        status = state.write_reason;
        widgets.status->set_text_color("BRIGHT_YELLOW");
    } else {
        widgets.status->set_text_color("BRIGHT_BLACK");
    }
    widgets.status->set_text(status.empty()
                                 ? "F5 actualizar | Enter preparar | F8 exportar | F10 salir"
                                 : status);

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
