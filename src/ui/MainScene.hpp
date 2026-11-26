#ifndef BOOTEABLE_UI_MAINSCENE_HPP
#define BOOTEABLE_UI_MAINSCENE_HPP

#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"

namespace booteable::ui {

class AppScene;

struct UiState;

struct MainSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::SplitContainer> split;
    guayaba::NodeRef<guayaba::PanelContainer> devices_panel;
    guayaba::NodeRef<guayaba::PanelContainer> details_panel;
    guayaba::NodeRef<guayaba::ListView> device_list;
    guayaba::NodeRef<guayaba::Label> count;
    guayaba::NodeRef<guayaba::Label> detail_title;
    guayaba::NodeRef<guayaba::Label> detail_model;
    guayaba::NodeRef<guayaba::Label> detail_capacity;
    guayaba::NodeRef<guayaba::Label> detail_serial;
    guayaba::NodeRef<guayaba::Label> detail_by_id;
    guayaba::NodeRef<guayaba::Label> detail_mounts;
    guayaba::NodeRef<guayaba::Label> detail_safety;
    guayaba::NodeRef<guayaba::Label> detail_image;
    guayaba::NodeRef<guayaba::Button> prepare;
    guayaba::NodeRef<guayaba::Button> export_image;
    guayaba::NodeRef<guayaba::Label> status;
};

// Panel principal: lista de USB y detalles del destino elegido. La disposición
// en una o dos columnas la decide SplitContainer según el ancho de la pantalla.
MainSceneWidgets build_main_scene(guayaba::NodeRef<guayaba::Control> parent,
                                  AppScene& app);

void refresh_main_scene(MainSceneWidgets& widgets,
                        const std::vector<DeviceInfo>& devices,
                        const UiState& state);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_MAINSCENE_HPP
