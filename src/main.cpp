#include <memory>

#include <guayaba/Guayaba.hpp>

#include "app/FakePrepController.hpp"
#include "ui/MainScene.hpp"

int main() {
    booteable::FakePrepController controller;
    auto scene = booteable::ui::build_main_scene(controller);

    guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
    manager.change_scene(std::move(scene));
    manager.set_target_fps(30);
    manager.start_running();
    return 0;
}
