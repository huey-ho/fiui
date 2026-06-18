#include <fiui/fiui.h>

int main()
{
    fiui::Window window("Package Smoke");
    window.debug_id("package_smoke_window").size(240.0f, 120.0f);

    fiui::Column root;
    root.debug_id("package_smoke_root");

    fiui::Text label("Package smoke");
    label.debug_id("package_smoke_label");

    root.add(label);
    window.content(root);

    const fiui::FrameReport report = fiui::render_frame(window);
    return report.frame_id > 0 ? 0 : 1;
}
