#ifndef BOOTEABLE_UI_UIASSETS_HPP
#define BOOTEABLE_UI_UIASSETS_HPP

#include <string_view>

#include <guayaba/Guayaba.hpp>

namespace booteable::ui {

// Texto de la escena y del tema incrustados en el binario. Se exponen para
// poder validarlos en las pruebas sin depender del sistema de archivos.
std::string_view scene_text();
std::string_view theme_text();

// Parsea y construye la escena con su tema. Lanza guayaba::SceneError con los
// diagnósticos si el asset incrustado no es válido.
guayaba::SceneAsset build_ui_asset();

} // namespace booteable::ui

#endif // BOOTEABLE_UI_UIASSETS_HPP
