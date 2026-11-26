#include <exception>
#include <iostream>
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

    try {
        booteable::ui::AppScene app(controller);
        auto scene = app.build();

        guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
        manager.change_scene(std::move(scene));
        manager.set_target_fps(30);
        manager.start_running();
    } catch (const guayaba::SceneError& error) {
        std::cerr << "Error al cargar la escena de la UI:\n" << error.what()
                  << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error al iniciar la UI: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
