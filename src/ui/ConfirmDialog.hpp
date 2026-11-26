#ifndef BOOTEABLE_UI_CONFIRMDIALOG_HPP
#define BOOTEABLE_UI_CONFIRMDIALOG_HPP

#include <string>

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"

namespace booteable::ui {

class AppScene;

struct ConfirmDialogWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> title;
    guayaba::NodeRef<guayaba::Label> summary;
    guayaba::NodeRef<guayaba::Label> warning;
    guayaba::NodeRef<guayaba::CheckBox> checkbox;
    guayaba::NodeRef<guayaba::Button> cancel;
    guayaba::NodeRef<guayaba::Button> overwrite;
    guayaba::NodeRef<guayaba::Label> status;
};

ConfirmDialogWidgets build_confirm_dialog(
    guayaba::NodeRef<guayaba::Control> parent, AppScene& app);

void refresh_confirm_dialog(ConfirmDialogWidgets& widgets,
                            const DeviceInfo& device, bool confirmed,
                            const std::string& status);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_CONFIRMDIALOG_HPP
