#include <fiui/fiui.h>

int main()
{
    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-diagnostics/hello";
    fiui::configure_diagnostics(diagnostics);

    fiui::Window window("Hello");
    window.debug_id("hello_window").size(640, 360);

    fiui::Column root;
    root.debug_id("root").padding(fiui::space::page).gap(fiui::space::md);

    fiui::Text title("Hello fiui");
    title.debug_id("title").style(fiui::text::title);

    fiui::Button button("Render frame");
    button.debug_id("render_button");

    root.add(title);
    root.add(button);
    window.content(root);

    fiui::render_frame(window);
    fiui::flush_diagnostics();
    return 0;
}
