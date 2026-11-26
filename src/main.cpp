#include <memory>

#include <guayaba/Guayaba.hpp>

#include "app/PrepController.hpp"
#include "app/PrepRunner.hpp"
#include "device/SysfsBlockDeviceSource.hpp"
#include "ui/AppScene.hpp"

int main() {
    booteable::SysfsBlockDeviceSource source;
    booteable::PrepRunner runner(source);
    booteable::PrepController controller(source, runner);

    booteable::ui::AppScene app(controller);
    auto scene = app.build();

    guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
    manager.change_scene(std::move(scene));
    manager.set_target_fps(30);
    manager.start_running();
    return 0;
}
