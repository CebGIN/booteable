#ifndef BOOTEABLE_UI_CONFIRMSCENE_HPP
#define BOOTEABLE_UI_CONFIRMSCENE_HPP

#include <string>

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"

namespace booteable::ui {

class AppScene;

struct ConfirmSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> kicker;
    guayaba::NodeRef<guayaba::Label> summary;
    guayaba::NodeRef<guayaba::Label> image;
    guayaba::NodeRef<guayaba::Label> warning;
    guayaba::NodeRef<guayaba::CheckBox> checkbox;
    guayaba::NodeRef<guayaba::Button> back;
    guayaba::NodeRef<guayaba::Button> overwrite;
    guayaba::NodeRef<guayaba::Label> status;
};

void bind_confirm_scene(guayaba::NodeRef<guayaba::PanelContainer> root,
                        ConfirmSceneWidgets& widgets, AppScene& app);

void refresh_confirm_scene(ConfirmSceneWidgets& widgets,
                           const DeviceInfo& device, bool confirmed,
                           const std::string& status);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_CONFIRMSCENE_HPP
