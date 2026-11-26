#ifndef BOOTEABLE_UI_STEPSCENE_HPP
#define BOOTEABLE_UI_STEPSCENE_HPP

#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "domain/Types.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {

class AppScene;

struct StepSceneWidgets {
    guayaba::NodeRef<guayaba::PanelContainer> panel;
    guayaba::NodeRef<guayaba::Label> kicker;
    guayaba::NodeRef<guayaba::Label> title;
    guayaba::NodeRef<guayaba::Label> phase;
    guayaba::NodeRef<guayaba::Label> description;
    guayaba::NodeRef<guayaba::Spinner> spinner;
    guayaba::NodeRef<guayaba::Label> status;
    guayaba::NodeRef<guayaba::ProgressBar> bar;
    guayaba::NodeRef<guayaba::Label> dots;
    guayaba::NodeRef<guayaba::Label> log;
    guayaba::NodeRef<guayaba::Button> continue_button;
    guayaba::NodeRef<guayaba::Button> cancel;
};

// `panel_name` es "step_a" o "step_b": los hijos se llaman <panel>_<campo>.
void bind_step_scene(guayaba::NodeRef<guayaba::Control> steps_root,
                     const std::string& panel_name, StepSceneWidgets& widgets,
                     AppScene& app);

// `live` es el paso que el controlador está ejecutando; `done` indica que ya
// terminó y que el usuario puede avanzar con Continuar. `waiting_before_write`
// es la pausa real antes de abrir la USB y `result_ready` el último paso.
void refresh_step_scene(StepSceneWidgets& widgets, WizardStep step,
                        const PrepProgress& progress,
                        const std::vector<std::string>& log, bool live, bool done,
                        bool result_ready, bool waiting_before_write,
                        bool pipeline_running);

} // namespace booteable::ui

#endif // BOOTEABLE_UI_STEPSCENE_HPP
