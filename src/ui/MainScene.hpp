#ifndef BOOTEABLE_UI_MAINSCENE_HPP
#define BOOTEABLE_UI_MAINSCENE_HPP

#include <memory>

namespace guayaba {
class Node;
}

namespace booteable {

class IPrepController;

namespace ui {

// Escena mínima de WP0: valida que Guayaba abre, dibuja y cierra. WP4a la
// reemplaza por la pantalla completa de preparación.
std::unique_ptr<guayaba::Node> build_main_scene(IPrepController& controller);

} // namespace ui
} // namespace booteable

#endif // BOOTEABLE_UI_MAINSCENE_HPP
