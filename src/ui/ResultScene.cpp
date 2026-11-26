#include "ui/ResultScene.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::VBox;

} // namespace

void bind_result_scene(NodeRef<Control> content, ResultSceneWidgets& widgets,
                       AppScene& app) {
    widgets.panel = content->get_node<PanelContainer>("result");
    NodeRef<VBox> layout = widgets.panel->get_node<VBox>("result_layout");

    widgets.outcome = layout->get_node<Label>("result_outcome");
    widgets.stage = layout->get_node<Label>("result_stage");
    widgets.message = layout->get_node<Label>("result_message");
    widgets.details = layout->get_node<Label>("result_details");
    widgets.hashes = layout->get_node<Label>("result_hashes");
    widgets.commands = layout->get_node<Label>("result_commands");

    NodeRef<HBox> buttons = layout->get_node<HBox>("result_buttons");
    widgets.back = buttons->get_node<Button>("result_back");
    widgets.quit = buttons->get_node<Button>("result_quit");

    widgets.back->connect_signal("clicked", [&app] { app.leave_result(); });
    widgets.quit->connect_signal("clicked", [&app] { app.quit(); });
}

void refresh_result_scene(ResultSceneWidgets& widgets, const DeviceInfo& device,
                          const PrepResult& result) {
    widgets.outcome->set_text(outcome_label(result.outcome));
    widgets.outcome->set_text_color(outcome_color(result.outcome));

    const PrepStage stage =
        result.outcome == PrepOutcome::Verified ? PrepStage::Done
                                                : result.failed_stage;
    widgets.stage->set_text("Destino: " + device.path + " | Etapa: " +
                            stage_label(stage));
    widgets.message->set_text(result.message.empty() ? "(sin mensaje)"
                                                     : result.message);

    widgets.details->set_text(
        "Bytes escritos: " + format_bytes(result.bytes_written) +
        " | Sincronizado: " + (result.synchronized ? "sí" : "no") +
        " | Verificado: " + (result.verified ? "sí" : "no"));

    widgets.hashes->set_text(
        "Imagen SHA-256: " + short_hash(result.image_sha256) +
        "\nDispositivo SHA-256: " + short_hash(result.device_sha256));
}

} // namespace booteable::ui
