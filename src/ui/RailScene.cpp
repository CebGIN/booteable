#include "ui/RailScene.hpp"

#include <string>

#include "ui/AppScene.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::TextAlign;
using guayaba::VBox;

struct Labels {
    const char* export_button;
    const char* refresh;
    const char* exit;
};

Labels labels_for(RailMode mode, bool expanded) {
    switch (mode) {
    case RailMode::MainTools:
        return expanded ? Labels{"[ Exportar ]", "[ Actualizar ]", "[ Salir ]"}
                        : Labels{"[E]", "[A]", "[S]"};
    case RailMode::ExportPage:
        return Labels{"[ Exportar ]", "[ Actualizar ]", "[ Salir ]"};
    case RailMode::Confirm:
        return expanded ? Labels{"", "", "[ Salir ]"}
                        : Labels{"", "", "[S]"};
    case RailMode::Progress:
        return expanded ? Labels{"", "", "[ Salir ]"}
                        : Labels{"", "", "[S]"};
    case RailMode::Result:
        return expanded ? Labels{"", "", "[ Salir ]"}
                        : Labels{"", "", "[S]"};
    }
    return Labels{"", "", ""};
}

} // namespace

void bind_rail_scene(NodeRef<PanelContainer> root, RailSceneWidgets& widgets,
                     AppScene& app) {
    widgets.panel = root;
    NodeRef<VBox> layout = root->get_node<VBox>("rail_layout");

    widgets.toggle = layout->get_node<Button>("rail_toggle");
    widgets.title = layout->get_node<Label>("rail_title");
    widgets.export_button = layout->get_node<Button>("rail_export");
    widgets.refresh = layout->get_node<Button>("rail_refresh");
    widgets.exit = layout->get_node<Button>("rail_exit");
    widgets.back = layout->get_node<Button>("rail_back");
    widgets.cancel = layout->get_node<Button>("rail_cancel");
    widgets.export_64 = layout->get_node<Button>("rail_export_64");
    widgets.export_selected = layout->get_node<Button>("rail_export_selected");
    widgets.export_back = layout->get_node<Button>("rail_export_back");

    widgets.toggle->connect_signal("clicked", [&app] { app.toggle_rail(); });
    widgets.export_button->connect_signal("clicked",
                                          [&app] { app.open_export_page(); });
    widgets.refresh->connect_signal("clicked",
                                    [&app] { app.request_refresh(); });
    widgets.exit->connect_signal("clicked", [&app] { app.quit(); });
    widgets.back->connect_signal("clicked", [&app] { app.rail_back(); });
    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.cancel_preparation(); });
    widgets.export_64->connect_signal(
        "clicked", [&app] { app.request_export_standard(); });
    widgets.export_selected->connect_signal(
        "clicked", [&app] { app.request_export_selected(); });
    widgets.export_back->connect_signal(
        "clicked", [&app] { app.close_export_page(); });
}

void refresh_rail_scene(RailSceneWidgets& widgets, const RailSceneState& state) {
    const bool expanded = state.expanded;
    widgets.toggle->set_text(expanded ? "[<]" : "[>]");
    const bool tools = state.mode == RailMode::MainTools ||
                       state.mode == RailMode::ExportPage;
    widgets.title->set_text(
        expanded ? (state.mode == RailMode::ExportPage
                        ? "EXPORTAR"
                        : (tools ? "HERRAMIENTAS" : "SESIÓN"))
                 : "");
    widgets.title->set_visible(expanded);

    // Desplegado, las etiquetas se alinean a la izquierda; plegado, se centran
    // en la celda de cinco columnas.
    const TextAlign align = expanded ? TextAlign::Left : TextAlign::Center;
    widgets.export_button->set_text_align(align);
    widgets.refresh->set_text_align(align);
    widgets.back->set_text_align(align);
    widgets.cancel->set_text_align(align);
    widgets.exit->set_text_align(align);
    widgets.export_64->set_text_align(align);
    widgets.export_selected->set_text_align(align);
    widgets.export_back->set_text_align(align);

    const Labels labels = labels_for(state.mode, expanded);
    widgets.export_button->set_text(labels.export_button);
    widgets.refresh->set_text(labels.refresh);
    widgets.exit->set_text(labels.exit);

    const bool main_tools = state.mode == RailMode::MainTools;
    const bool export_page = state.mode == RailMode::ExportPage;
    const bool confirm = state.mode == RailMode::Confirm;
    const bool progress = state.mode == RailMode::Progress;
    const bool result = state.mode == RailMode::Result;

    widgets.export_button->set_visible(main_tools);
    widgets.refresh->set_visible(main_tools);
    widgets.export_64->set_visible(export_page);
    widgets.export_selected->set_visible(export_page);
    widgets.export_back->set_visible(export_page);
    widgets.back->set_visible(confirm || result);
    widgets.cancel->set_visible(progress);
    widgets.exit->set_visible(!export_page);

    widgets.back->set_text(expanded ? "[ Volver ]" : "[V]");
    widgets.cancel->set_text(expanded ? "[ Cancelar ]" : "[X]");

    widgets.exit->set_disabled(progress);

    widgets.export_64->set_text("[ 64 MiB ]");
    if (state.has_device) {
        widgets.export_selected->set_text("[ " + state.device_path + " ]");
        widgets.export_selected->set_disabled(false);
    } else {
        widgets.export_selected->set_text("[ Sin USB ]");
        widgets.export_selected->set_disabled(true);
    }
}

} // namespace booteable::ui
