#include "ui/UiAssets.hpp"

#include <string_view>
#include <utility>

#include "EmbeddedUiAssets.hpp"

namespace booteable::ui {

std::string_view scene_text() {
    return std::string_view(booteable::ui::assets::kSceneText);
}

std::string_view theme_text() {
    return std::string_view(booteable::ui::assets::kThemeText);
}

guayaba::SceneAsset build_ui_asset() {
    guayaba::SceneParseOptions theme_parse_options;
    theme_parse_options.source_name = "booteable.gtheme";

    guayaba::ThemeParseResult parsed_theme =
        guayaba::parse_theme(theme_text(), theme_parse_options);
    if (!parsed_theme.ok()) {
        throw guayaba::SceneError(std::move(parsed_theme.diagnostics));
    }

    guayaba::ThemeBuildResult built_theme =
        guayaba::build_theme(parsed_theme.document);
    if (!built_theme.ok()) {
        throw guayaba::SceneError(std::move(built_theme.diagnostics));
    }

    guayaba::SceneParseOptions scene_parse_options;
    scene_parse_options.source_name = "booteable.gscene";

    guayaba::SceneParseResult parsed_scene =
        guayaba::parse_scene(scene_text(), scene_parse_options);
    if (!parsed_scene.ok()) {
        throw guayaba::SceneError(std::move(parsed_scene.diagnostics));
    }

    // El encabezado `theme` del .gscene sirve para SceneLoader::load_file(); en
    // el binario el tema viaja incrustado y se pasa explícitamente.
    guayaba::SceneLoadOptions options;
    options.source_name = "booteable.gscene";
    options.fallback_theme = built_theme.theme.get();

    guayaba::SceneBuildResult built =
        guayaba::build_scene(parsed_scene.document, built_theme.theme.get(),
                             options);
    if (!built.ok()) {
        throw guayaba::SceneError(std::move(built.diagnostics));
    }

    guayaba::SceneAsset asset;
    asset.root = std::move(built.root);
    asset.theme = std::move(built_theme.theme);
    return asset;
}

} // namespace booteable::ui
