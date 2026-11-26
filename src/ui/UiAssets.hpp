#ifndef BOOTEABLE_UI_UIASSETS_HPP
#define BOOTEABLE_UI_UIASSETS_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <guayaba/Guayaba.hpp>

namespace booteable::ui {

/*
    Cada .gscene incrustado es un documento independiente con un solo root. El
    formato no compone escenas, así que AppScene los injerta bajo los slots del
    shell y comparte un único tema entre todos.
*/
struct UiSceneSource {
    std::string_view name;
    std::string_view source;
    std::string_view text;
};

struct UiSceneRoot {
    std::string name;
    std::unique_ptr<guayaba::Node> root;
};

struct UiAssetBundle {
    std::unique_ptr<guayaba::Theme> theme;
    std::vector<UiSceneRoot> roots;
};

std::vector<UiSceneSource> scene_sources();
std::string_view theme_text();

// Parsea y construye el tema una vez y todas las escenas con ese tema. Lanza
// guayaba::SceneError con los diagnósticos del primer archivo que falle.
UiAssetBundle build_ui_assets();

// Extrae el root de una escena por nombre. Lanza std::runtime_error si falta.
std::unique_ptr<guayaba::Node> take_root(UiAssetBundle& bundle,
                                         std::string_view name);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_UIASSETS_HPP
