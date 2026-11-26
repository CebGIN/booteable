// Validación headless de las escenas y el tema declarativos: cada .gscene se
// parsea y valida por separado, y build_ui_assets() compone los roots con un
// tema compartido.

#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "ui/UiAssets.hpp"
#include "TestHarness.hpp"

namespace {

using guayaba::Button;
using guayaba::CheckBox;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::ListView;
using guayaba::PanelContainer;
using guayaba::ProgressBar;

void test_scenes_parse_and_validate() {
    const std::vector<booteable::ui::UiSceneSource> sources =
        booteable::ui::scene_sources();
    CHECK_EQ(sources.size(), std::size_t{6});

    for (const booteable::ui::UiSceneSource& source : sources) {
        const guayaba::SceneParseOptions options{std::string(source.source)};
        const guayaba::SceneParseResult parsed =
            guayaba::parse_scene(source.text, options);
        CHECK(parsed.ok());
        CHECK(parsed.document.root != nullptr);

        const std::vector<guayaba::SceneDiagnostic> diagnostics =
            guayaba::validate_scene(parsed.document, true,
                                    std::string(source.source));
        CHECK(!guayaba::has_error(diagnostics));
    }
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
    CHECK(built.theme->has_style("rail"));
    CHECK(built.theme->has_style("step_panel"));
    CHECK(built.theme->has_style("spinner"));
    CHECK(built.theme->has_style("button.normal"));
    CHECK(built.theme->has_style("button.rail.normal"));
    CHECK(built.theme->has_style("button.primary.normal"));
    CHECK(built.theme->has_style("button.danger.normal"));
    CHECK(built.theme->has_style("list_view"));
    CHECK(built.theme->has_style("progress_bar"));
    CHECK(built.theme->has_style("split_container.divider"));
    CHECK(built.theme->has_color("text.primary"));
    CHECK(built.theme->has_color("selection.background"));
}

void test_bundle_builds_expected_widgets() {
    booteable::ui::UiAssetBundle bundle = booteable::ui::build_ui_assets();
    CHECK(bundle.theme != nullptr);
    CHECK_EQ(bundle.roots.size(), std::size_t{6});

    const auto shell = booteable::ui::take_root(bundle, "shell");
    CHECK(shell != nullptr);
    CHECK(shell->get_node<HBox>("app_frame").is_valid());
    CHECK(shell->get_node<Control>("app_frame/rail_slot").is_valid());
    CHECK(shell->get_node<Control>("app_frame/shell/stage_slot").is_valid());

    const auto rail = booteable::ui::take_root(bundle, "rail");
    CHECK(rail != nullptr);
    CHECK_EQ(rail->get_name(), std::string("rail"));
    CHECK(rail->get_node<Button>("rail_layout/rail_export").is_valid());
    CHECK(rail->get_node<Button>("rail_layout/rail_export_64").is_valid());
    CHECK(rail->get_node<Button>("rail_layout/rail_export_selected")
              .is_valid());

    const auto main = booteable::ui::take_root(bundle, "main");
    CHECK(main != nullptr);
    CHECK(main->get_node<ListView>(
                 "main_layout/main_split/devices_panel/devices_layout/"
                 "device_list")
              .is_valid());
    CHECK(main->get_node<Button>(
                 "main_layout/main_split/details_panel/details_layout/"
                 "detail_buttons/prepare")
              .is_valid());

    const auto steps = booteable::ui::take_root(bundle, "steps");
    CHECK(steps != nullptr);
    CHECK(steps->get_node<PanelContainer>("step_a").is_valid());
    CHECK(steps->get_node<PanelContainer>("step_b").is_valid());
    CHECK(steps->get_node<Label>("step_a/step_a_layout/step_a_title")
              .is_valid());
    CHECK(!steps->get_node<PanelContainer>("step_a")->is_visible());
    CHECK(!steps->get_node<PanelContainer>("step_b")->is_visible());

    const auto confirm = booteable::ui::take_root(bundle, "confirm");
    CHECK(confirm != nullptr);
    CHECK(confirm->get_node<CheckBox>("confirm_layout/confirm_checkbox")
              .is_valid());
    CHECK(!confirm->is_visible());

    const auto result = booteable::ui::take_root(bundle, "result");
    CHECK(result != nullptr);
    CHECK(result->get_node<Label>("result_layout/result_outcome").is_valid());
    CHECK(!result->is_visible());

    // Un root de cada escena mide lo que le da el slot; no se exige tamaño
    // propio porque la composición la hace AppScene.
    const guayaba::Rect box = guayaba::Rect::from_size(
        guayaba::Vec2{80, 24});
    shell->update_layout(box);
    auto* shell_control = dynamic_cast<guayaba::Control*>(shell.get());
    CHECK(shell_control != nullptr);
    CHECK(shell_control->get_rect().right() <= 80);
    CHECK(shell_control->get_rect().bottom() <= 24);
}

} // namespace

void run_ui_scene_tests() {
    test_scenes_parse_and_validate();
    test_theme_parses_and_builds();
    test_bundle_builds_expected_widgets();
}
