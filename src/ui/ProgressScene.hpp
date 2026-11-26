#ifndef BOOTEABLE_UI_PROGRESSSCENE_HPP
#define BOOTEABLE_UI_PROGRESSSCENE_HPP

#include <array>
#include <string>

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"

namespace booteable::ui {

class AppScene;

struct ProgressSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> title;
    guayaba::NodeRef<guayaba::Label> detail;
    guayaba::NodeRef<guayaba::ProgressBar> bar;
    std::array<guayaba::NodeRef<guayaba::Label>, 6> steps;
    guayaba::NodeRef<guayaba::Button> cancel;
    guayaba::NodeRef<guayaba::Label> status;
};

void bind_progress_scene(guayaba::NodeRef<guayaba::Control> content,
                         ProgressSceneWidgets& widgets, AppScene& app);

void refresh_progress_scene(ProgressSceneWidgets& widgets,
                            const DeviceInfo& device,
                            const PrepProgress& progress,
                            const std::string& notice = {});

} // namespace booteable::ui

#endif // BOOTEABLE_UI_PROGRESSSCENE_HPP
