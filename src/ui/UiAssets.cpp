#include "ui/UiAssets.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "EmbeddedUiAssets.hpp"

namespace booteable::ui {

std::vector<UiSceneSource> scene_sources() {
    std::vector<UiSceneSource> sources;
    sources.reserve(assets::kSceneCount);
    for (std::size_t index = 0; index < assets::kSceneCount; ++index) {
        const assets::EmbeddedScene& scene = assets::kScenes[index];
        sources.push_back(
            UiSceneSource{scene.name, scene.source, scene.text});
    }
    return sources;
}

std::string_view theme_text() {
    return std::string_view(assets::kThemeText);
}

UiAssetBundle build_ui_assets() {
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

    UiAssetBundle bundle;
    bundle.theme = std::move(built_theme.theme);

    for (const UiSceneSource& source : scene_sources()) {
        guayaba::SceneParseOptions parse_options;
        parse_options.source_name = std::string(source.source);

        guayaba::SceneParseResult parsed =
            guayaba::parse_scene(source.text, parse_options);
        if (!parsed.ok()) {
            throw guayaba::SceneError(std::move(parsed.diagnostics));
        }

        // El tema se pasa explícitamente y se comparte entre todas las escenas;
        // no se resuelve el header `theme` de cada archivo.
        guayaba::SceneLoadOptions load_options;
        load_options.source_name = parse_options.source_name;
        load_options.fallback_theme = bundle.theme.get();

        guayaba::SceneBuildResult built = guayaba::build_scene(
            parsed.document, bundle.theme.get(), load_options);
        if (!built.ok()) {
            throw guayaba::SceneError(std::move(built.diagnostics));
        }

        bundle.roots.push_back(
            UiSceneRoot{std::string(source.name), std::move(built.root)});
    }

    return bundle;
}

std::unique_ptr<guayaba::Node> take_root(UiAssetBundle& bundle,
                                         std::string_view name) {
    for (UiSceneRoot& entry : bundle.roots) {
        if (entry.name == name) {
            return std::move(entry.root);
        }
    }
    throw std::runtime_error("Falta la escena '" + std::string(name) +
                             "' en los assets de la UI.");
}

} // namespace booteable::ui
