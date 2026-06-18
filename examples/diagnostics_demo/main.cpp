#include <fiui/fiui.h>

#include <stdexcept>

namespace {

void throwing_callback(void*)
{
    throw std::runtime_error("intentional diagnostics demo failure");
}

} // namespace

int main()
{
    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-diagnostics/diagnostics_demo";
    fiui::configure_diagnostics(diagnostics);

    fiui::Window window("Diagnostics Demo");
    window.debug_id("diagnostics_window").size(700, 420);

    fiui::Column root;
    root.debug_id("root").padding(fiui::space::page).gap(fiui::space::md);

    fiui::Button button("Throw");
    button.debug_id("throw_button");
    button.on_click(throwing_callback);

    root.add(button);
    root.add(button);
    button.click();
    window.content(root);

    fiui::render_frame(window);
    fiui::flush_diagnostics();
    return 0;
}
