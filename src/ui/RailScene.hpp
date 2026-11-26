#ifndef BOOTEABLE_UI_RAILSCENE_HPP
#define BOOTEABLE_UI_RAILSCENE_HPP

#include <string>

#include <guayaba/Guayaba.hpp>

namespace booteable::ui {

class AppScene;

struct RailSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> title;
    guayaba::NodeRef<guayaba::Button> export_button;
    guayaba::NodeRef<guayaba::Button> refresh;
    guayaba::NodeRef<guayaba::Button> exit;
    guayaba::NodeRef<guayaba::Button> back;
    guayaba::NodeRef<guayaba::Button> cancel;
    guayaba::NodeRef<guayaba::Button> export_64;
    guayaba::NodeRef<guayaba::Button> export_selected;
    guayaba::NodeRef<guayaba::Button> export_back;
};

// Qué acciones ofrece el rail en la pantalla actual. Las herramientas solo
// existen en el paso 1; durante la preparación queda Cancelar.
enum class RailMode {
    MainTools,
    ExportPage,
    Confirm,
    Progress,
    Result,
};

struct RailSceneState {
    RailMode mode = RailMode::MainTools;
    bool expanded = false;
    bool has_device = false;
    std::string device_path;
};

void bind_rail_scene(guayaba::NodeRef<guayaba::PanelContainer> root,
                     RailSceneWidgets& widgets, AppScene& app);

void refresh_rail_scene(RailSceneWidgets& widgets, const RailSceneState& state);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_RAILSCENE_HPP
