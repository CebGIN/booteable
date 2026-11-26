// Validación headless de los assets declarativos de la UI: el .gscene y el
// .gtheme incrustados se parsean, se validan y producen el árbol esperado.

#include <string>

#include <guayaba/Guayaba.hpp>

#include "ui/UiAssets.hpp"
#include "TestHarness.hpp"

namespace {

using guayaba::Button;
using guayaba::CheckBox;
using guayaba::Control;
using guayaba::Label;
using guayaba::ListView;
using guayaba::PanelContainer;
using guayaba::ProgressBar;
using guayaba::SplitContainer;

void test_scene_parses_and_validates() {
    const guayaba::SceneParseResult parsed = guayaba::parse_scene(
        booteable::ui::scene_text(),
        guayaba::SceneParseOptions{"booteable.gscene"});
    CHECK(parsed.ok());
    CHECK(parsed.document.root != nullptr);
    CHECK_EQ(parsed.document.root->type, std::string("Control"));
    CHECK_EQ(parsed.document.root->name, std::string("content"));

    const std::vector<guayaba::SceneDiagnostic> diagnostics =
        guayaba::validate_scene(parsed.document, true, "booteable.gscene");
    CHECK(!guayaba::has_error(diagnostics));
}

void test_theme_parses_and_builds() {
    const guayaba::ThemeParseResult parsed = guayaba::parse_theme(
        booteable::ui::theme_text(),
        guayaba::SceneParseOptions{"booteable.gtheme"});
    CHECK(parsed.ok());

    const guayaba::ThemeBuildResult built =
        guayaba::build_theme(parsed.document);
    CHECK(built.ok());
    CHECK(built.theme != nullptr);
    CHECK(built.theme->has_style("panel"));
    CHECK(built.theme->has_style("button.normal"));
    CHECK(built.theme->has_style("button.hover"));
    CHECK(built.theme->has_style("button.pressed"));
    CHECK(built.theme->has_style("button.disabled"));
    CHECK(built.theme->has_style("button.danger.normal"));
    CHECK(built.theme->has_style("list_view"));
    CHECK(built.theme->has_style("progress_bar"));
    CHECK(built.theme->has_style("split_container.divider"));
    CHECK(built.theme->has_color("text.primary"));
    CHECK(built.theme->has_color("selection.background"));
}

void test_scene_builds_expected_widgets() {
    guayaba::SceneAsset asset = booteable::ui::build_ui_asset();
    CHECK(asset.root != nullptr);
    CHECK(asset.theme != nullptr);

    guayaba::Node& content = *asset.root;
    CHECK(content.get_node<PanelContainer>("main").is_valid());
    CHECK(content.get_node<SplitContainer>("main/main_layout/main_split")
              .is_valid());
    CHECK(content
              .get_node<ListView>(
                  "main/main_layout/main_split/devices_panel/devices_layout/device_list")
              .is_valid());
    CHECK(content
              .get_node<Button>(
                  "main/main_layout/main_split/details_panel/details_layout/detail_buttons/prepare")
              .is_valid());
    CHECK(content.get_node<PanelContainer>("confirm").is_valid());
    CHECK(content.get_node<CheckBox>("confirm/confirm_layout/confirm_checkbox")
              .is_valid());
    CHECK(content.get_node<ProgressBar>("progress/progress_layout/progress_bar")
              .is_valid());
    CHECK(content.get_node<Label>("result/result_layout/result_outcome")
              .is_valid());

    // El panel de confirmación empieza oculto y el principal, visible.
    CHECK(!content.get_node<PanelContainer>("confirm")->is_visible());
    CHECK(content.get_node<PanelContainer>("main")->is_visible());

    content.update_layout(guayaba::Rect::from_size(guayaba::Vec2{80, 24}));
    CHECK(content.get_node<PanelContainer>("main")->get_rect().right() <= 80);
}

} // namespace

void run_ui_scene_tests() {
    test_scene_parses_and_validates();
    test_theme_parses_and_builds();
    test_scene_builds_expected_widgets();
}
