#ifndef BOOTEABLE_UI_RESULTSCENE_HPP
#define BOOTEABLE_UI_RESULTSCENE_HPP

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"

namespace booteable::ui {

class AppScene;

struct ResultSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> outcome;
    guayaba::NodeRef<guayaba::Label> stage;
    guayaba::NodeRef<guayaba::Label> message;
    guayaba::NodeRef<guayaba::Label> details;
    guayaba::NodeRef<guayaba::Label> hashes;
    guayaba::NodeRef<guayaba::Label> commands;
    guayaba::NodeRef<guayaba::Button> back;
    guayaba::NodeRef<guayaba::Button> quit;
};

void bind_result_scene(guayaba::NodeRef<guayaba::Control> content,
                       ResultSceneWidgets& widgets, AppScene& app);

void refresh_result_scene(ResultSceneWidgets& widgets, const DeviceInfo& device,
                          const PrepResult& result);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_RESULTSCENE_HPP
