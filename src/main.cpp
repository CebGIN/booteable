#include <memory>

#include <guayaba/Guayaba.hpp>

#include "app/FakePrepController.hpp"
#include "ui/AppScene.hpp"

int main() {
    booteable::FakePrepController controller;
    booteable::ui::AppScene app(controller);
    auto scene = app.build();

    guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
    manager.change_scene(std::move(scene));
    manager.set_target_fps(30);
    manager.start_running();
    return 0;
}
