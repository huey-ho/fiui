#include <fiui/fiui.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct DemoState {
    int apply_count = 0;
    int theme_switch_count = 0;
    int dialog_count = 0;
    int layout_mode = 0;
    bool dark_theme = true;
    bool ai_diagnostics_enabled = false;
    bool high_performance_mode = false;
    fiui::Application* app = nullptr;
    fiui::Window* window = nullptr;
    fiui::Text* status_text = nullptr;
    fiui::Text* diagnostics_text = nullptr;
    fiui::Text* dialog_title = nullptr;
    fiui::Text* dialog_body = nullptr;
    fiui::Text* dialog_status = nullptr;
    fiui::Dialog* modal_dialog = nullptr;
    fiui::Text* metrics_text = nullptr;
    fiui::Text* resource_text = nullptr;
    fiui::Input* project_name = nullptr;
    fiui::Progress* render_progress = nullptr;
    fiui::Slider* render_budget_slider = nullptr;
    fiui::RadioButton* mode_auto = nullptr;
    fiui::RadioButton* mode_manual = nullptr;
    fiui::RadioButton* mode_custom = nullptr;
    fiui::Select* diagnostics_level_select = nullptr;
    fiui::ListView* runtime_event_list = nullptr;
    fiui::TreeView* runtime_object_tree = nullptr;
    fiui::TableView* metrics_table = nullptr;
    fiui::TextArea* runtime_notes = nullptr;
    fiui::Image* preview = nullptr;
    fiui::Column* profile_section = nullptr;
    fiui::Column* dialog_section = nullptr;
    fiui::Column* visual_section = nullptr;
    fiui::Column* runtime_card = nullptr;
    fiui::Column* backend_card = nullptr;
    fiui::Row* status_row = nullptr;
    fiui::SplitView* workspace_split = nullptr;
    fiui::Text* layout_mode_text = nullptr;
    fiui::CheckBox* ai_diagnostics_toggle = nullptr;
    fiui::CheckBox* high_performance_toggle = nullptr;
    fiui::Switch* live_preview_switch = nullptr;
    bool overflow_visible = false;
};

bool has_arg(int argc, char** argv, const char* value)
{
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], value) == 0) {
            return true;
        }
    }
    return false;
}

void request_demo_repaint(DemoState& state, fiui::DirtyReason reason)
{
    if (state.window != nullptr) {
        state.window->mark_dirty(reason | fiui::DirtyReason::Paint);
    }
}

void refresh_metrics_table(DemoState& state, const char* reason)
{
    if (state.metrics_table == nullptr) {
        return;
    }
    const int budget =
        state.render_budget_slider == nullptr
            ? 42
            : static_cast<int>(state.render_budget_slider->value() * 100.0f);
    std::ostringstream budget_text;
    budget_text << budget << "%";
    std::ostringstream apply_text;
    apply_text << state.apply_count;
    std::ostringstream theme_text;
    theme_text << (state.dark_theme ? "dark" : "light");
    std::ostringstream diag_text;
    diag_text << (state.ai_diagnostics_enabled ? "verbose" : "basic");
    std::ostringstream perf_text;
    perf_text << (state.high_performance_mode ? "high" : "normal");

    state.metrics_table->clear_rows()
        .add_row("Frame time", state.high_performance_mode ? "0.9 ms" : "1.6 ms",
                 state.high_performance_mode ? "fast" : "stable")
        .add_row("Render budget", budget_text.str().c_str(),
                 budget >= 70 ? "wide" : "normal")
        .add_row("Apply count", apply_text.str().c_str(), "updated")
        .add_row("Diagnostics", diag_text.str().c_str(),
                 state.ai_diagnostics_enabled ? "ai" : "lean")
        .add_row("Theme tokens", theme_text.str().c_str(), "active")
        .add_row("Input route", "live", "active")
        .add_row("Dirty rects", state.apply_count > 0 ? "5" : "3", "partial")
        .add_row("Resources", "cached", "ready")
        .add_row("Display list", "42 cmds", "batched")
        .add_row("Object table", "128", "tracked");
    state.metrics_table->selected_row(0);
    if (reason != nullptr && state.resource_text != nullptr) {
        std::ostringstream text;
        text << "Metrics table refreshed from " << reason;
        state.resource_text->text(text.str().c_str());
    }
}

void update_diagnostics_panel(DemoState& state, const char* reason)
{
    if (state.diagnostics_text == nullptr) {
        return;
    }
    const fiui::RuntimeSnapshot snapshot = fiui::runtime_snapshot();
    std::ostringstream text;
    text << "Diagnostics: frame=" << snapshot.last_frame.frame_id
         << " dirty=" << snapshot.last_frame.original_dirty_rects
         << "/" << snapshot.last_frame.merged_dirty_rects
         << " repaint=" << (snapshot.last_frame.full_repaint ? "full" : "partial")
         << " event=" << snapshot.last_platform_event
         << " focus=" << snapshot.focus_target
         << " hover=" << snapshot.hover_target
         << " capture=" << snapshot.capture_target
         << " input=" << snapshot.input_event_count
         << " reason=" << (reason == nullptr ? "refresh" : reason);
    state.diagnostics_text->text(text.str().c_str());
}

void show_dialog(DemoState& state, const char* title, const char* body, const char* status)
{
    ++state.dialog_count;
    if (state.modal_dialog != nullptr) {
        state.modal_dialog->open();
    }
    if (state.dialog_title != nullptr) {
        state.dialog_title->text(title);
    }
    if (state.dialog_body != nullptr) {
        state.dialog_body->text(body);
    }
    if (state.dialog_status != nullptr) {
        state.dialog_status->text(status);
    }
    if (state.status_text != nullptr) {
        state.status_text->text(status);
    }
    update_diagnostics_panel(state, status);
    request_demo_repaint(state, fiui::DirtyReason::Input);
}

void close_modal_dialog(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr && state->modal_dialog != nullptr) {
        state->modal_dialog->close();
        if (state->status_text != nullptr) {
            state->status_text->text("Dialog closed");
        }
        update_diagnostics_panel(*state, "dialog_close");
        request_demo_repaint(*state, fiui::DirtyReason::Input);
    }
}

void apply_changes(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        ++state->apply_count;
        if (state->project_name != nullptr) {
            state->project_name->value(state->apply_count % 2 == 0 ? "fiui-runtime"
                                                                   : "fiui-control-center");
        }
        if (state->render_progress != nullptr) {
            state->render_progress->value(state->apply_count % 2 == 0 ? 0.72f : 0.42f);
        }
        if (state->status_text != nullptr) {
            state->status_text->text(state->apply_count % 2 == 0
                                         ? "Applied runtime profile"
                                         : "Applied control center profile");
        }
        if (state->dialog_status != nullptr) {
            state->dialog_status->text("Profile mutation dispatched through callback");
        }
        refresh_metrics_table(*state, "apply");
        update_diagnostics_panel(*state, "apply");
        request_demo_repaint(*state, fiui::DirtyReason::Input);
    }
}

void switch_theme(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        ++state->theme_switch_count;
        state->dark_theme = !state->dark_theme;
        if (state->app != nullptr) {
            state->app->theme(state->dark_theme ? "modern.dark" : "modern.light");
        }
        if (state->preview != nullptr) {
            state->preview->source(state->dark_theme ? "control-center-preview.png"
                                                     : "control-center-preview-light.png");
        }
        if (state->status_text != nullptr) {
            state->status_text->text(state->dark_theme ? "Theme switched to dark"
                                                       : "Theme switched to light");
        }
        if (state->dialog_title != nullptr) {
            state->dialog_title->text("Theme Dialog");
        }
        if (state->dialog_body != nullptr) {
            state->dialog_body->text(state->dark_theme
                                         ? "Modern dark tokens are active for every component"
                                         : "Modern light tokens are active for every component");
        }
        if (state->dialog_status != nullptr) {
            state->dialog_status->text("Theme tokens updated without raw app colors");
        }
        refresh_metrics_table(*state, "theme");
        update_diagnostics_panel(*state, "theme");
        request_demo_repaint(*state, fiui::DirtyReason::ThemeChanged);
    }
}

void show_overview_dialog(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        show_dialog(*state, "Overview Dialog",
                    "Runtime, layout, render, resources, and diagnostics are active",
                    "Overview panel selected");
    }
}

void show_layout_dialog(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        if (state->metrics_text != nullptr) {
            state->metrics_text->text("Layout: row flex, column flex, single-child fill");
        }
        show_dialog(*state, "Layout Inspector",
                    "Shell fills the window; content scroll and preview use explicit flex",
                    "Layout diagnostics selected");
    }
}

void apply_layout_mode(DemoState& state, int mode, const char* label)
{
    state.layout_mode = mode;
    if (state.profile_section == nullptr || state.dialog_section == nullptr ||
        state.visual_section == nullptr || state.runtime_card == nullptr ||
        state.backend_card == nullptr) {
        return;
    }

    if (mode == 1) {
        if (state.workspace_split != nullptr) {
            state.workspace_split->ratio(0.28f);
        }
        state.profile_section->flex(1.0f);
        state.dialog_section->flex(3.0f);
        state.visual_section->size(0.0f, 180.0f);
        state.runtime_card->flex(1.0f);
        state.backend_card->flex(2.0f);
    } else if (mode == 2) {
        if (state.workspace_split != nullptr) {
            state.workspace_split->ratio(0.42f);
        }
        state.profile_section->flex(1.0f);
        state.dialog_section->flex(1.0f);
        state.visual_section->size(0.0f, 420.0f);
        state.runtime_card->flex(1.0f);
        state.backend_card->flex(1.0f);
    } else if (mode == 3) {
        if (state.workspace_split != nullptr) {
            state.workspace_split->ratio(0.36f);
        }
        state.profile_section->size(420.0f, 0.0f);
        state.dialog_section->size(760.0f, 0.0f);
        state.visual_section->size(0.0f, 420.0f);
        state.runtime_card->size(520.0f, 0.0f);
        state.backend_card->size(520.0f, 0.0f);
    } else {
        if (state.workspace_split != nullptr) {
            state.workspace_split->ratio(0.34f);
        }
        state.profile_section->flex(1.0f);
        state.dialog_section->flex(2.0f);
        state.visual_section->size(0.0f, 200.0f);
        state.runtime_card->flex(1.0f);
        state.backend_card->flex(1.0f);
    }

    if (state.layout_mode_text != nullptr) {
        std::ostringstream text;
        text << "Layout mode: " << (label == nullptr ? "balanced" : label)
             << " | overflow policy: " << (state.overflow_visible ? "visible" : "clip")
             << " | inspect fiui-frame.json layout overflow fields";
        state.layout_mode_text->text(text.str().c_str());
    }
    if (state.metrics_text != nullptr) {
        std::ostringstream text;
        text << "Layout: mode=" << (label == nullptr ? "balanced" : label)
             << ", width/height modes recorded in frame JSON";
        state.metrics_text->text(text.str().c_str());
    }
    if (state.dialog_title != nullptr) {
        state.dialog_title->text("Layout Mode");
    }
    if (state.dialog_body != nullptr) {
        state.dialog_body->text("Buttons above mutate flex, fixed, and overflow layout states");
    }
    if (state.dialog_status != nullptr) {
        state.dialog_status->text(label == nullptr ? "Balanced layout applied" : label);
    }
    if (state.status_text != nullptr) {
        state.status_text->text(label == nullptr ? "Balanced layout applied" : label);
    }
    update_diagnostics_panel(state, label == nullptr ? "layout_mode" : label);
    request_demo_repaint(state, fiui::DirtyReason::Layout);
}

void apply_overflow_policy(DemoState& state, bool visible)
{
    state.overflow_visible = visible;
    if (state.status_row != nullptr) {
        state.status_row->overflow(visible ? fiui::OverflowPolicy::Visible
                                           : fiui::OverflowPolicy::Clip);
    }
    if (state.workspace_split != nullptr) {
        state.workspace_split->overflow(fiui::OverflowPolicy::Clip);
    }
    apply_layout_mode(state, state.layout_mode, visible ? "overflow visible" : "overflow clip");
}

void overflow_clip(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_overflow_policy(*state, false);
    }
}

void overflow_visible(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_overflow_policy(*state, true);
    }
}

void layout_balanced(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 0, "balanced");
    }
}

void layout_dialog_wide(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 1, "dialog wide");
    }
}

void layout_preview_tall(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 2, "preview tall");
    }
}

void layout_overflow_test(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 3, "overflow test");
    }
}

void show_resource_dialog(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        if (state->resource_text != nullptr) {
            state->resource_text->text("Resources: image texture, text metrics, theme tokens");
        }
        if (state->preview != nullptr) {
            state->preview->source("control-center-resource-preview.png");
        }
        show_dialog(*state, "Resource Browser",
                    "Image metadata and text layout resources are tracked in diagnostics",
                    "Resource dialog selected");
    }
}

void show_export_dialog(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        if (state->render_progress != nullptr) {
            state->render_progress->value(0.91f);
        }
        show_dialog(*state, "Export Report",
                    "Frame JSON, trace JSONL, leak report, and backend counters are available",
                    "Export dialog prepared");
    }
}

void reset_preview(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        if (state->preview != nullptr) {
            state->preview->source(state->dark_theme ? "control-center-preview.png"
                                                     : "control-center-preview-light.png");
        }
        if (state->render_progress != nullptr) {
            state->render_progress->value(0.42f);
        }
        show_dialog(*state, "Reset Preview",
                    "Preview image, progress, and dialog state returned to baseline",
                    "Preview reset complete");
    }
}

void update_runtime_options(DemoState& state, const char* reason)
{
    if (state.ai_diagnostics_toggle != nullptr) {
        state.ai_diagnostics_enabled = state.ai_diagnostics_toggle->checked();
    }
    if (state.high_performance_toggle != nullptr) {
        state.high_performance_mode = state.high_performance_toggle->checked();
    }
    if (state.live_preview_switch != nullptr) {
        state.high_performance_mode = state.high_performance_mode ||
                                      state.live_preview_switch->checked();
    }

    if (state.status_text != nullptr) {
        state.status_text->text(state.ai_diagnostics_enabled
                                    ? "AI diagnostics enabled"
                                    : "Runtime options updated");
    }
    if (state.metrics_text != nullptr) {
        std::ostringstream text;
        text << "Runtime options: diagnostics="
             << (state.ai_diagnostics_enabled ? "ai" : "basic")
             << ", performance=" << (state.high_performance_mode ? "high" : "balanced");
        state.metrics_text->text(text.str().c_str());
    }
    if (state.dialog_title != nullptr) {
        state.dialog_title->text("Runtime Options");
    }
    if (state.dialog_body != nullptr) {
        state.dialog_body->text(
            "CheckBox state is routed through focus, keyboard, dirty tracking, and render tree");
    }
    if (state.dialog_status != nullptr) {
        state.dialog_status->text(state.high_performance_mode
                                      ? "High performance mode selected"
                                      : "Balanced performance mode selected");
    }
    update_diagnostics_panel(state, reason == nullptr ? "runtime_options" : reason);
    request_demo_repaint(state, fiui::DirtyReason::Input);
}

void toggle_ai_diagnostics(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        update_runtime_options(*state, "ai_diagnostics_toggle");
        refresh_metrics_table(*state, "ai_diagnostics_toggle");
    }
}

void toggle_high_performance(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        update_runtime_options(*state, "performance_toggle");
        refresh_metrics_table(*state, "performance_toggle");
    }
}

void toggle_live_preview(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state != nullptr) {
        update_runtime_options(*state, "live_preview_switch");
    }
}

void change_render_budget(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr || state->render_budget_slider == nullptr) {
        return;
    }
    const float value = state->render_budget_slider->value();
    if (state->render_progress != nullptr) {
        state->render_progress->value(value);
    }
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Render budget slider: " << static_cast<int>(value * 100.0f) << "%";
        state->status_text->text(text.str().c_str());
    }
    if (state->resource_text != nullptr) {
        std::ostringstream text;
        text << "Render budget value=" << value << " routed through Slider on_change";
        state->resource_text->text(text.str().c_str());
    }
    refresh_metrics_table(*state, "render_budget_slider");
    update_diagnostics_panel(*state, "render_budget_slider");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void change_profile_mode(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr) {
        return;
    }
    const char* mode = "auto";
    if (state->mode_manual != nullptr && state->mode_manual->checked()) {
        mode = "manual";
    } else if (state->mode_custom != nullptr && state->mode_custom->checked()) {
        mode = "custom";
    }
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Profile mode: " << mode;
        state->status_text->text(text.str().c_str());
    }
    if (state->dialog_title != nullptr) {
        state->dialog_title->text("Profile Mode");
    }
    if (state->dialog_body != nullptr) {
        state->dialog_body->text("RadioButton group selection updated the profile mode");
    }
    if (state->dialog_status != nullptr) {
        state->dialog_status->text(mode);
    }
    update_diagnostics_panel(*state, "profile_mode_radio");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void change_diagnostics_level(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr || state->diagnostics_level_select == nullptr) {
        return;
    }
    const char* selected = state->diagnostics_level_select->selected_text();
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Diagnostics level: " << selected;
        state->status_text->text(text.str().c_str());
    }
    if (state->dialog_title != nullptr) {
        state->dialog_title->text("Diagnostics Level");
    }
    if (state->dialog_body != nullptr) {
        state->dialog_body->text("Select popup changed the diagnostics detail level");
    }
    if (state->dialog_status != nullptr) {
        state->dialog_status->text(selected);
    }
    update_diagnostics_panel(*state, "diagnostics_level_select");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void change_runtime_event(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr || state->runtime_event_list == nullptr) {
        return;
    }
    const char* selected = state->runtime_event_list->selected_text();
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Runtime event selected: " << selected;
        state->status_text->text(text.str().c_str());
    }
    if (state->dialog_title != nullptr) {
        state->dialog_title->text("Runtime Event");
    }
    if (state->dialog_body != nullptr) {
        std::ostringstream text;
        text << "ListView selected the " << selected << " diagnostic scenario";
        state->dialog_body->text(text.str().c_str());
    }
    if (state->dialog_status != nullptr) {
        state->dialog_status->text(selected);
    }
    update_diagnostics_panel(*state, "runtime_event_list");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void change_runtime_object(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr || state->runtime_object_tree == nullptr) {
        return;
    }
    const char* selected = state->runtime_object_tree->selected_text();
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Runtime object selected: " << selected;
        state->status_text->text(text.str().c_str());
    }
    if (state->dialog_title != nullptr) {
        state->dialog_title->text("Runtime Object Tree");
    }
    if (state->dialog_body != nullptr) {
        std::ostringstream text;
        text << "TreeView selected " << selected
             << "; expand and collapse groups to inspect hierarchy";
        state->dialog_body->text(text.str().c_str());
    }
    if (state->dialog_status != nullptr) {
        state->dialog_status->text(selected);
    }
    update_diagnostics_panel(*state, "runtime_object_tree");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void change_metrics_table(void* user_data)
{
    auto* state = static_cast<DemoState*>(user_data);
    if (state == nullptr || state->metrics_table == nullptr) {
        return;
    }
    const char* selected = state->metrics_table->selected_text();
    if (state->status_text != nullptr) {
        std::ostringstream text;
        text << "Metric row selected: " << selected;
        state->status_text->text(text.str().c_str());
    }
    if (state->metrics_text != nullptr) {
        std::ostringstream text;
        text << "Metrics: selected " << selected << " from TableView";
        state->metrics_text->text(text.str().c_str());
    }
    if (state->dialog_title != nullptr) {
        state->dialog_title->text("Metrics Table");
    }
    if (state->dialog_body != nullptr) {
        state->dialog_body->text("TableView supports header, rows, selection, and keyboard navigation");
    }
    if (state->dialog_status != nullptr) {
        state->dialog_status->text(selected);
    }
    update_diagnostics_panel(*state, "metrics_table");
    request_demo_repaint(*state, fiui::DirtyReason::Input);
}

void throw_diagnostics_error(void*)
{
    throw std::runtime_error("intentional control_center_demo callback failure");
}

void print_report(const char* label, const fiui::FrameReport& report)
{
    std::cout << label << " frame_id=" << report.frame_id
              << " dirty=" << report.original_dirty_rects << " merged=" << report.merged_dirty_rects
              << " full_repaint=" << (report.full_repaint ? "true" : "false")
              << " fallback=" << report.fallback_reason << " layout_ms=" << report.layout_ms
              << " display_ms=" << report.display_list_ms << '\n';
}

std::string read_text_file(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

void write_text_file(const char* path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    if (file) {
        file << text;
    }
}

} // namespace

int main(int argc, char** argv)
{
    const bool run_visible = has_arg(argc, argv, "--run") || has_arg(argc, argv, "--run-ai");
    const bool ai_diagnostics =
        has_arg(argc, argv, "--ai-diagnostics") || has_arg(argc, argv, "--run-ai");

    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode =
        (!run_visible || ai_diagnostics) ? fiui::DebugMode::AiFriendly : fiui::DebugMode::Basic;
    diagnostics.output_directory = "fiui-diagnostics/control_center_demo";
    fiui::configure_diagnostics(diagnostics);

    fiui::Application app;
    app.theme("modern.dark");
    DemoState state;
    state.app = &app;
    state.dark_theme = true;
    state.ai_diagnostics_enabled = ai_diagnostics;

    fiui::Window window("Control Center");
    window.debug_id("control_center_window").size(1180, 820);
    state.window = &window;

    fiui::Column app_root;
    app_root.debug_id("app_root").fill();

    fiui::Row shell;
    shell.debug_id("shell").padding(fiui::space::page).gap(fiui::space::lg).flex();

    fiui::Column nav;
    nav.debug_id("nav").padding(fiui::space::md).gap(fiui::space::sm).size(220, 0);

    fiui::Text nav_title("Control Center");
    nav_title.debug_id("nav_title").style(fiui::text::title);

    fiui::Button overview_tab("Overview");
    overview_tab.debug_id("overview_tab");
    overview_tab.style("subtle");
    overview_tab.on_click(show_overview_dialog, &state);

    fiui::Button network_tab("Network");
    network_tab.debug_id("network_tab");
    network_tab.style("subtle");
    network_tab.on_click(show_resource_dialog, &state);

    fiui::Button display_tab("Display");
    display_tab.debug_id("display_tab");
    display_tab.style("subtle");
    display_tab.on_click(switch_theme, &state);

    fiui::Button diagnostics_tab("Diagnostics");
    diagnostics_tab.debug_id("diagnostics_tab");
    diagnostics_tab.style("subtle");
    diagnostics_tab.on_click(show_layout_dialog, &state);

    nav.add(nav_title);
    nav.add(overview_tab);
    nav.add(network_tab);
    nav.add(display_tab);
    nav.add(diagnostics_tab);

    fiui::ScrollView content_scroll;
    content_scroll.debug_id("content_scroll").flex();

    fiui::Column content;
    content.debug_id("content").padding(fiui::space::page).gap(fiui::space::lg).fill();

    fiui::MenuBar menu_bar;
    menu_bar.debug_id("main_menu").fill_width();
    fiui::MenuItem file_menu("File");
    file_menu.debug_id("menu_file");
    fiui::MenuItem file_overview("Overview");
    file_overview.debug_id("menu_file_overview");
    file_overview.shortcut("Ctrl+O");
    file_overview.on_click(show_overview_dialog, &state);
    fiui::MenuItem file_apply("Apply changes");
    file_apply.debug_id("menu_file_apply");
    file_apply.shortcut("Ctrl+A");
    file_apply.on_click(apply_changes, &state);
    fiui::Separator file_separator;
    file_separator.debug_id("menu_file_separator");
    fiui::MenuItem file_reset("Reset preview");
    file_reset.debug_id("menu_file_reset");
    file_reset.on_click(reset_preview, &state);
    file_menu.add(file_overview);
    file_menu.add(file_apply);
    file_menu.add(file_separator);
    file_menu.add(file_reset);

    fiui::MenuItem view_menu("View");
    view_menu.debug_id("menu_view");
    fiui::MenuItem view_layout("Layout diagnostics >");
    view_layout.debug_id("menu_view_layout");
    fiui::MenuItem view_layout_tree("Layout tree");
    view_layout_tree.debug_id("menu_view_layout_tree");
    view_layout_tree.on_click(show_layout_dialog, &state);
    fiui::MenuItem view_dirty_rects("Dirty rectangles");
    view_dirty_rects.debug_id("menu_view_dirty_rects");
    view_dirty_rects.on_click(show_layout_dialog, &state);
    fiui::MenuItem view_runtime("Runtime overview");
    view_runtime.debug_id("menu_view_runtime");
    view_runtime.on_click(show_overview_dialog, &state);
    view_layout.add(view_layout_tree);
    view_layout.add(view_dirty_rects);
    view_menu.add(view_layout);
    view_menu.add(view_runtime);

    fiui::MenuItem theme_menu("Theme");
    theme_menu.debug_id("menu_theme");
    fiui::MenuItem theme_toggle("Toggle dark/light");
    theme_toggle.debug_id("menu_theme_toggle");
    theme_toggle.checked(true);
    theme_toggle.shortcut("Ctrl+T");
    theme_toggle.on_click(switch_theme, &state);
    fiui::MenuItem theme_future("Theme editor");
    theme_future.debug_id("menu_theme_future");
    theme_future.enabled(false);
    theme_menu.add(theme_toggle);
    theme_menu.add(theme_future);

    fiui::MenuItem resources_menu("Resources");
    resources_menu.debug_id("menu_resources");
    fiui::MenuItem resources_inspect("Inspect resources >");
    resources_inspect.debug_id("menu_resources_inspect");
    fiui::MenuItem resources_cache("Resource cache");
    resources_cache.debug_id("menu_resources_cache");
    resources_cache.on_click(show_resource_dialog, &state);
    fiui::MenuItem resources_textures("Texture handles");
    resources_textures.debug_id("menu_resources_textures");
    resources_textures.on_click(show_resource_dialog, &state);
    fiui::MenuItem resources_diagnostics("Trigger diagnostics");
    resources_diagnostics.debug_id("menu_resources_diagnostics");
    resources_diagnostics.style("danger");
    resources_diagnostics.on_click(throw_diagnostics_error);
    fiui::Separator resources_separator;
    resources_separator.debug_id("menu_resources_separator");
    resources_inspect.add(resources_cache);
    resources_inspect.add(resources_textures);
    resources_menu.add(resources_inspect);
    resources_menu.add(resources_separator);
    resources_menu.add(resources_diagnostics);

    fiui::MenuItem export_menu("Export");
    export_menu.debug_id("menu_export");
    fiui::MenuItem export_report("Export report");
    export_report.debug_id("menu_export_report");
    export_report.shortcut("Ctrl+E");
    export_report.on_click(show_export_dialog, &state);
    fiui::MenuItem export_apply("Apply and export");
    export_apply.debug_id("menu_export_apply");
    export_apply.on_click(apply_changes, &state);
    export_menu.add(export_report);
    export_menu.add(export_apply);

    menu_bar.add(file_menu);
    menu_bar.add(view_menu);
    menu_bar.add(theme_menu);
    menu_bar.add(resources_menu);
    menu_bar.add(export_menu);

    fiui::Separator menu_separator;
    menu_separator.debug_id("main_menu_separator");

    fiui::Text title("Runtime Control Center");
    title.debug_id("title").style(fiui::text::title);

    fiui::Text summary("Input, dirty tracking, resources, themes, and diagnostics");
    summary.debug_id("summary").style(fiui::text::caption);

    fiui::Text status_text("Runtime ready");
    status_text.debug_id("status_text").style(fiui::text::caption);
    state.status_text = &status_text;

    fiui::Text diagnostics_text(ai_diagnostics ? "Diagnostics: ai-friendly trace"
                                               : "Diagnostics: interactive basic");
    diagnostics_text.debug_id("diagnostics_text").style(fiui::text::caption);
    diagnostics_text.multiline();
    diagnostics_text.size(0, 48);
    state.diagnostics_text = &diagnostics_text;

    fiui::Text layout_mode_text("Layout mode: balanced");
    layout_mode_text.debug_id("layout_mode_text").style(fiui::text::caption);
    state.layout_mode_text = &layout_mode_text;

    fiui::Row layout_mode_row;
    layout_mode_row.debug_id("layout_mode_row").gap(fiui::space::md);
    fiui::Button balanced_layout_button("Balanced");
    balanced_layout_button.debug_id("balanced_layout_button");
    balanced_layout_button.tooltip("Use balanced flex sizing for the main workspace");
    balanced_layout_button.on_click(layout_balanced, &state);
    fiui::Button dialog_wide_button("Dialog wide");
    dialog_wide_button.debug_id("dialog_wide_button");
    dialog_wide_button.tooltip("Give the dialog panel more horizontal space");
    dialog_wide_button.on_click(layout_dialog_wide, &state);
    fiui::Button preview_tall_button("Preview tall");
    preview_tall_button.debug_id("preview_tall_button");
    preview_tall_button.tooltip("Increase the visual preview height");
    preview_tall_button.on_click(layout_preview_tall, &state);
    fiui::Button overflow_layout_button("Overflow test");
    overflow_layout_button.debug_id("overflow_layout_button");
    overflow_layout_button.style("danger");
    overflow_layout_button.tooltip("Force fixed sizes to inspect clipping and overflow");
    overflow_layout_button.on_click(layout_overflow_test, &state);
    fiui::Button overflow_clip_button("Clip");
    overflow_clip_button.debug_id("overflow_clip_button");
    overflow_clip_button.tooltip("Clip overflowing child content");
    overflow_clip_button.on_click(overflow_clip, &state);
    fiui::Button overflow_visible_button("Visible");
    overflow_visible_button.debug_id("overflow_visible_button");
    overflow_visible_button.tooltip("Allow overflowing child content to remain visible");
    overflow_visible_button.on_click(overflow_visible, &state);
    layout_mode_row.add(balanced_layout_button);
    layout_mode_row.add(dialog_wide_button);
    layout_mode_row.add(preview_tall_button);
    layout_mode_row.add(overflow_layout_button);
    layout_mode_row.add(overflow_clip_button);
    layout_mode_row.add(overflow_visible_button);

    fiui::Row status_row;
    status_row.debug_id("status_row").gap(fiui::space::md);
    state.status_row = &status_row;

    fiui::Column runtime_card;
    runtime_card.debug_id("runtime_card").padding(fiui::space::md).gap(fiui::space::sm).flex(1.0f);
    state.runtime_card = &runtime_card;
    fiui::Text runtime_title("Runtime");
    runtime_title.debug_id("runtime_title").style(fiui::text::title);
    fiui::Text runtime_body("Dirty tracking, diagnostics, scheduler, object table");
    runtime_body.debug_id("runtime_body").style(fiui::text::caption);
    fiui::CheckBox ai_diagnostics_toggle("AI diagnostics");
    ai_diagnostics_toggle.debug_id("ai_diagnostics_toggle");
    ai_diagnostics_toggle.checked(ai_diagnostics);
    ai_diagnostics_toggle.tooltip("Toggle high detail diagnostics for traces and frame reports");
    ai_diagnostics_toggle.on_change(toggle_ai_diagnostics, &state);
    state.ai_diagnostics_toggle = &ai_diagnostics_toggle;
    runtime_card.add(runtime_title);
    runtime_card.add(runtime_body);
    runtime_card.add(ai_diagnostics_toggle);

    fiui::Column backend_card;
    backend_card.debug_id("backend_card").padding(fiui::space::md).gap(fiui::space::sm).flex(1.0f);
    state.backend_card = &backend_card;
    fiui::Text backend_title("Backend");
    backend_title.debug_id("backend_title").style(fiui::text::title);
    fiui::Text backend_body("D3D11, text, image, clip, rounded, shadow fallback");
    backend_body.debug_id("backend_body").style(fiui::text::caption);
    fiui::CheckBox high_performance_toggle("High performance");
    high_performance_toggle.debug_id("high_performance_toggle");
    high_performance_toggle.tooltip("Switch the demo state to high performance mode");
    high_performance_toggle.on_change(toggle_high_performance, &state);
    state.high_performance_toggle = &high_performance_toggle;
    fiui::Switch live_preview_switch("Live preview");
    live_preview_switch.debug_id("live_preview_switch");
    live_preview_switch.checked(true);
    live_preview_switch.tooltip("Use a modern switch control for live preview state");
    live_preview_switch.on_change(toggle_live_preview, &state);
    state.live_preview_switch = &live_preview_switch;
    backend_card.add(backend_title);
    backend_card.add(backend_body);
    backend_card.add(high_performance_toggle);
    backend_card.add(live_preview_switch);

    status_row.add(runtime_card);
    status_row.add(backend_card);

    fiui::SplitView workspace_split;
    workspace_split.debug_id("workspace_split").fill_width().size(0.0f, 300.0f);
    workspace_split.ratio(0.34f).min_pane_size(220.0f).handle_size(8.0f);
    state.workspace_split = &workspace_split;

    fiui::Column profile_section;
    profile_section.debug_id("profile_section")
        .padding(fiui::space::md)
        .gap(fiui::space::sm)
        .flex(1.0f);
    state.profile_section = &profile_section;
    fiui::Text profile_title("Profile");
    profile_title.debug_id("profile_title").style(fiui::text::title);
    fiui::Input project_name;
    project_name.debug_id("project_name");
    project_name.placeholder("Project name").value("fiui-control-center");
    project_name.tooltip("Editable project id. Click inside text to place the caret.");
    state.project_name = &project_name;
    fiui::Input operator_name;
    operator_name.debug_id("operator_name");
    operator_name.placeholder("Operator").value("runtime-team");
    operator_name.tooltip("Editable operator name with selection and clipboard support");
    fiui::Row profile_mode_row;
    profile_mode_row.debug_id("profile_mode_row").gap(fiui::space::sm);
    fiui::RadioButton mode_auto("Auto");
    mode_auto.debug_id("mode_auto");
    mode_auto.group("profile_mode").checked(true);
    mode_auto.tooltip("Use automatic runtime profile selection");
    mode_auto.on_change(change_profile_mode, &state);
    state.mode_auto = &mode_auto;
    fiui::RadioButton mode_manual("Manual");
    mode_manual.debug_id("mode_manual");
    mode_manual.group("profile_mode");
    mode_manual.tooltip("Use manual runtime profile selection");
    mode_manual.on_change(change_profile_mode, &state);
    state.mode_manual = &mode_manual;
    fiui::RadioButton mode_custom("Custom");
    mode_custom.debug_id("mode_custom");
    mode_custom.group("profile_mode");
    mode_custom.tooltip("Use custom runtime profile selection");
    mode_custom.on_change(change_profile_mode, &state);
    state.mode_custom = &mode_custom;
    profile_mode_row.add(mode_auto);
    profile_mode_row.add(mode_manual);
    profile_mode_row.add(mode_custom);
    fiui::Select diagnostics_level_select;
    diagnostics_level_select.debug_id("diagnostics_level_select");
    diagnostics_level_select.placeholder("Diagnostics level");
    diagnostics_level_select.add_option("Basic");
    diagnostics_level_select.add_option("Balanced");
    diagnostics_level_select.add_option("Verbose");
    diagnostics_level_select.on_change(change_diagnostics_level, &state);
    state.diagnostics_level_select = &diagnostics_level_select;
    fiui::Text event_list_title("Runtime events");
    event_list_title.debug_id("event_list_title").style(fiui::text::caption);
    fiui::ListView runtime_event_list;
    runtime_event_list.debug_id("runtime_event_list");
    runtime_event_list.size(0.0f, 126.0f);
    runtime_event_list.add_item("Frame build")
        .add_item("Dirty merge")
        .add_item("Resource cache")
        .add_item("Input route");
    runtime_event_list.on_change(change_runtime_event, &state);
    state.runtime_event_list = &runtime_event_list;
    fiui::Text object_tree_title("Runtime objects");
    object_tree_title.debug_id("object_tree_title").style(fiui::text::caption);
    fiui::TreeView runtime_object_tree;
    runtime_object_tree.debug_id("runtime_object_tree").size(0.0f, 230.0f);
    runtime_object_tree.on_change(change_runtime_object, &state);
    state.runtime_object_tree = &runtime_object_tree;
    fiui::TreeItem runtime_group("Runtime");
    runtime_group.debug_id("tree_runtime_group");
    runtime_group.expanded(true);
    fiui::TreeItem object_table_item("Object table");
    object_table_item.debug_id("tree_object_table");
    fiui::TreeItem scheduler_item("Frame scheduler");
    scheduler_item.debug_id("tree_scheduler");
    runtime_group.add(object_table_item);
    runtime_group.add(scheduler_item);
    fiui::TreeItem layout_group("Layout");
    layout_group.debug_id("tree_layout_group");
    layout_group.expanded(true);
    fiui::TreeItem constraints_item("Constraints");
    constraints_item.debug_id("tree_constraints");
    fiui::TreeItem dirty_item("Dirty planner");
    dirty_item.debug_id("tree_dirty_planner");
    layout_group.add(constraints_item);
    layout_group.add(dirty_item);
    fiui::TreeItem render_group("Render");
    render_group.debug_id("tree_render_group");
    render_group.expanded(false);
    fiui::TreeItem display_list_item("Display list");
    display_list_item.debug_id("tree_display_list");
    fiui::TreeItem resources_item("Resource cache");
    resources_item.debug_id("tree_resource_cache");
    render_group.add(display_list_item);
    render_group.add(resources_item);
    fiui::TreeItem diagnostics_group("Diagnostics");
    diagnostics_group.debug_id("tree_diagnostics_group");
    diagnostics_group.expanded(false);
    fiui::TreeItem trace_item("Trace file");
    trace_item.debug_id("tree_trace_file");
    fiui::TreeItem frame_item("Frame report");
    frame_item.debug_id("tree_frame_report");
    diagnostics_group.add(trace_item);
    diagnostics_group.add(frame_item);
    runtime_object_tree.add_item(runtime_group)
        .add_item(layout_group)
        .add_item(render_group)
        .add_item(diagnostics_group);
    profile_section.add(profile_title);
    profile_section.add(project_name);
    profile_section.add(operator_name);
    profile_section.add(profile_mode_row);
    profile_section.add(diagnostics_level_select);
    profile_section.add(event_list_title);
    profile_section.add(runtime_event_list);
    profile_section.add(object_tree_title);
    profile_section.add(runtime_object_tree);

    fiui::Column dialog_section;
    dialog_section.debug_id("dialog_section")
        .padding(fiui::space::md)
        .gap(fiui::space::sm)
        .flex(2.0f);
    state.dialog_section = &dialog_section;
    fiui::Text dialog_preview_title("Dialog System");
    dialog_preview_title.debug_id("dialog_preview_title").style(fiui::text::title);
    fiui::Text dialog_preview_body("Use toolbar buttons to open a modal overlay dialog");
    dialog_preview_body.debug_id("dialog_preview_body").style(fiui::text::caption);
    fiui::Text dialog_preview_status("Backdrop click and Escape close the dialog");
    dialog_preview_status.debug_id("dialog_preview_status").style(fiui::text::caption);
    fiui::Text metrics_text("Layout: row/column flex ready");
    metrics_text.debug_id("metrics_text").style(fiui::text::caption);
    state.metrics_text = &metrics_text;
    fiui::Text resource_text("Resources: text/image cache ready");
    resource_text.debug_id("resource_text").style(fiui::text::caption);
    state.resource_text = &resource_text;
    fiui::TableView metrics_table;
    metrics_table.debug_id("metrics_table").size(0.0f, 154.0f);
    metrics_table.add_column("Metric", 150.0f)
        .add_column("Value", 86.0f)
        .add_column("State", 110.0f);
    metrics_table.on_change(change_metrics_table, &state);
    state.metrics_table = &metrics_table;
    refresh_metrics_table(state, "startup");
    fiui::TextArea runtime_notes;
    runtime_notes.debug_id("runtime_notes").size(0.0f, 96.0f);
    runtime_notes.placeholder("Runtime notes");
    runtime_notes.value("Editable runtime notes\nClick here, type text, press Enter, or Ctrl+A");
    state.runtime_notes = &runtime_notes;
    fiui::Tabs dialog_tabs;
    dialog_tabs.debug_id("dialog_tabs").fill();
    fiui::Column dialog_overview_tab;
    dialog_overview_tab.debug_id("dialog_overview_tab").gap(fiui::space::sm);
    fiui::Column dialog_metrics_tab;
    dialog_metrics_tab.debug_id("dialog_metrics_tab").gap(fiui::space::sm);
    dialog_overview_tab.add(dialog_preview_title);
    dialog_overview_tab.add(dialog_preview_body);
    dialog_overview_tab.add(dialog_preview_status);
    dialog_metrics_tab.add(metrics_text);
    dialog_metrics_tab.add(resource_text);
    dialog_metrics_tab.add(metrics_table);
    dialog_metrics_tab.add(runtime_notes);
    dialog_tabs.add_tab("Overview", dialog_overview_tab);
    dialog_tabs.add_tab("Metrics", dialog_metrics_tab);
    dialog_section.add(dialog_tabs);

    workspace_split.first(profile_section);
    workspace_split.second(dialog_section);

    fiui::Column visual_section;
    visual_section.debug_id("visual_section")
        .padding(fiui::space::md)
        .gap(fiui::space::sm)
        .size(0.0f, 200.0f);
    state.visual_section = &visual_section;
    fiui::Text visual_title("Visual Preview");
    visual_title.debug_id("visual_title").style(fiui::text::title);
    fiui::Image preview("control-center-preview.png");
    preview.debug_id("preview").flex();
    state.preview = &preview;
    fiui::Progress render_progress;
    render_progress.debug_id("render_progress");
    render_progress.value(0.42f);
    state.render_progress = &render_progress;
    fiui::Slider render_budget_slider;
    render_budget_slider.debug_id("render_budget_slider");
    render_budget_slider.value(0.42f);
    render_budget_slider.tooltip("Drag to update render budget and progress value");
    render_budget_slider.on_change(change_render_budget, &state);
    state.render_budget_slider = &render_budget_slider;
    visual_section.add(visual_title);
    visual_section.add(preview);
    visual_section.add(render_progress);
    visual_section.add(render_budget_slider);

    fiui::Toolbar action_toolbar;
    action_toolbar.debug_id("action_toolbar").fill_width();
    fiui::Button apply_button;
    apply_button.debug_id("apply_button");
    apply_button.style("primary");
    apply_button.on_click(apply_changes, &state);
    fiui::Row apply_content;
    apply_content.debug_id("apply_button_content").gap(fiui::space::sm);
    fiui::Image apply_icon("icon-check.png");
    apply_icon.debug_id("apply_button_icon").size(18.0f, 18.0f);
    apply_icon.radius(4.0f);
    fiui::Text apply_label("Apply");
    apply_label.debug_id("apply_button_label");
    apply_content.add(apply_icon);
    apply_content.add(apply_label);
    apply_button.content(apply_content);
    fiui::Button theme_button("Switch theme");
    theme_button.debug_id("theme_button");
    theme_button.on_click(switch_theme, &state);
    fiui::Button failure_button;
    failure_button.debug_id("failure_button");
    failure_button.style("danger");
    failure_button.on_click(throw_diagnostics_error);
    fiui::Row failure_content;
    failure_content.debug_id("failure_button_content").gap(fiui::space::sm);
    fiui::Image failure_icon("icon-warning.png");
    failure_icon.debug_id("failure_button_icon").size(18.0f, 18.0f);
    failure_icon.radius(4.0f);
    fiui::Text failure_label("Diagnostics");
    failure_label.debug_id("failure_button_label");
    failure_content.add(failure_icon);
    failure_content.add(failure_label);
    failure_button.content(failure_content);
    fiui::Button complex_button;
    complex_button.debug_id("complex_button");
    complex_button.normal_image("button-normal.png")
        .hover_image("button-hover.png")
        .pressed_image("button-pressed.png")
        .image_fit(fiui::ImageFit::Cover)
        .background(fiui::Color{33, 93, 180, 255})
        .hover_background(fiui::Color{46, 125, 230, 255})
        .pressed_background(fiui::Color{21, 71, 148, 255})
        .radius(14.0f)
        .text_padding(10.0f)
        .on_click(apply_changes, &state);
    fiui::Row complex_content;
    complex_content.debug_id("complex_button_content").gap(fiui::space::sm);
    fiui::Image complex_icon("button-icon.png");
    complex_icon.debug_id("complex_button_icon").size(20.0f, 20.0f);
    complex_icon.radius(4.0f);
    fiui::Text complex_label("Complex");
    complex_label.debug_id("complex_button_label");
    complex_content.add(complex_icon);
    complex_content.add(complex_label);
    complex_button.content(complex_content);
    action_toolbar.add(apply_button);
    action_toolbar.add(theme_button);
    action_toolbar.add(failure_button);

    fiui::Row dialog_action_row;
    dialog_action_row.debug_id("dialog_action_row").gap(fiui::space::md);
    fiui::Button layout_button;
    layout_button.debug_id("layout_button");
    layout_button.on_click(show_layout_dialog, &state);
    fiui::Row layout_content;
    layout_content.debug_id("layout_button_content").gap(fiui::space::sm);
    fiui::Image layout_icon("icon-layout.png");
    layout_icon.debug_id("layout_button_icon").size(18.0f, 18.0f);
    layout_icon.radius(4.0f);
    fiui::Text layout_label("Layout");
    layout_label.debug_id("layout_button_label");
    layout_content.add(layout_icon);
    layout_content.add(layout_label);
    layout_button.content(layout_content);
    fiui::Button resources_button;
    resources_button.debug_id("resources_button");
    resources_button.on_click(show_resource_dialog, &state);
    fiui::Row resources_content;
    resources_content.debug_id("resources_button_content").gap(fiui::space::sm);
    fiui::Image resources_icon("icon-resource.png");
    resources_icon.debug_id("resources_button_icon").size(18.0f, 18.0f);
    resources_icon.radius(4.0f);
    fiui::Text resources_label("Resources");
    resources_label.debug_id("resources_button_label");
    resources_content.add(resources_icon);
    resources_content.add(resources_label);
    resources_button.content(resources_content);
    fiui::Button export_button("Export report");
    export_button.debug_id("export_button");
    export_button.style("primary");
    export_button.on_click(show_export_dialog, &state);
    fiui::Button reset_button("Reset");
    reset_button.debug_id("reset_button");
    reset_button.on_click(reset_preview, &state);
    dialog_action_row.add(complex_button);
    dialog_action_row.add(layout_button);
    dialog_action_row.add(resources_button);
    dialog_action_row.add(export_button);
    dialog_action_row.add(reset_button);

    fiui::Column scroll_test_section;
    scroll_test_section.debug_id("scroll_test_section")
        .padding(fiui::space::md)
        .gap(fiui::space::sm);
    fiui::Text scroll_test_title("Scroll Diagnostics");
    scroll_test_title.debug_id("scroll_test_title").style(fiui::text::title);
    fiui::Text scroll_test_body(
        "Use the mouse wheel inside this right panel. The scrollbar thumb should move "
        "immediately and the diagnostics row should keep updating with input counters.");
    scroll_test_body.debug_id("scroll_test_body").style(fiui::text::caption);
    scroll_test_body.multiline();
    scroll_test_body.size(0, 54);
    fiui::Text scroll_step_01("01  Wheel input routes through PlatformSystem and EventSystem");
    scroll_step_01.debug_id("scroll_step_01").style(fiui::text::caption);
    fiui::Text scroll_step_02("02  ScrollView stores scroll_offset_y and scroll_content_height");
    scroll_step_02.debug_id("scroll_step_02").style(fiui::text::caption);
    fiui::Text scroll_step_03("03  Layout repositions the content node by the scroll offset");
    scroll_step_03.debug_id("scroll_step_03").style(fiui::text::caption);
    fiui::Text scroll_step_04("04  RenderSystem emits a scrollbar_thumb display command");
    scroll_step_04.debug_id("scroll_step_04").style(fiui::text::caption);
    fiui::Text scroll_step_05("05  The thumb is painted after content and clipped to the viewport");
    scroll_step_05.debug_id("scroll_step_05").style(fiui::text::caption);
    fiui::Text scroll_step_06("06  Dirty metadata records input, layout, paint, and fallback data");
    scroll_step_06.debug_id("scroll_step_06").style(fiui::text::caption);
    fiui::Text scroll_step_07("07  AI diagnostics can inspect frame JSON and trace JSONL");
    scroll_step_07.debug_id("scroll_step_07").style(fiui::text::caption);
    fiui::Text scroll_step_08("08  Resize the window to verify the thumb recalculates");
    scroll_step_08.debug_id("scroll_step_08").style(fiui::text::caption);
    fiui::Text scroll_step_09("09  Hover and text input should remain responsive while scrolling");
    scroll_step_09.debug_id("scroll_step_09").style(fiui::text::caption);
    fiui::Text scroll_step_10("10  This section intentionally makes the panel overflow");
    scroll_step_10.debug_id("scroll_step_10").style(fiui::text::caption);
    scroll_test_section.add(scroll_test_title);
    scroll_test_section.add(scroll_test_body);
    scroll_test_section.add(scroll_step_01);
    scroll_test_section.add(scroll_step_02);
    scroll_test_section.add(scroll_step_03);
    scroll_test_section.add(scroll_step_04);
    scroll_test_section.add(scroll_step_05);
    scroll_test_section.add(scroll_step_06);
    scroll_test_section.add(scroll_step_07);
    scroll_test_section.add(scroll_step_08);
    scroll_test_section.add(scroll_step_09);
    scroll_test_section.add(scroll_step_10);

    fiui::Dialog modal_dialog;
    modal_dialog.debug_id("modal_dialog").size(640.0f, 360.0f);
    modal_dialog.backdrop_closes(true).escape_closes(true);
    state.modal_dialog = &modal_dialog;
    fiui::Column modal_panel;
    modal_panel.debug_id("modal_panel").padding(fiui::space::lg).gap(fiui::space::md);
    fiui::Text dialog_title("Overview Dialog");
    dialog_title.debug_id("dialog_title").style(fiui::text::title);
    state.dialog_title = &dialog_title;
    fiui::Text dialog_body("Select a tool button to open a different runtime dialog");
    dialog_body.debug_id("dialog_body").style(fiui::text::caption);
    dialog_body.multiline();
    dialog_body.size(0.0f, 64.0f);
    state.dialog_body = &dialog_body;
    fiui::Text dialog_status("Dialog panel ready");
    dialog_status.debug_id("dialog_status").style(fiui::text::caption);
    state.dialog_status = &dialog_status;
    fiui::Row modal_actions;
    modal_actions.debug_id("modal_actions").gap(fiui::space::md);
    fiui::Button modal_apply("Apply");
    modal_apply.debug_id("modal_apply");
    modal_apply.style("primary");
    modal_apply.on_click(apply_changes, &state);
    fiui::Button modal_close("Close");
    modal_close.debug_id("modal_close");
    modal_close.on_click(close_modal_dialog, &state);
    modal_actions.add(modal_apply);
    modal_actions.add(modal_close);
    modal_panel.add(dialog_title);
    modal_panel.add(dialog_body);
    modal_panel.add(dialog_status);
    modal_panel.add(modal_actions);
    modal_dialog.content(modal_panel);

    content.add(title);
    content.add(summary);
    content.add(status_text);
    content.add(diagnostics_text);
    content.add(layout_mode_text);
    content.add(layout_mode_row);
    content.add(status_row);
    content.add(workspace_split);
    content.add(visual_section);
    content.add(scroll_test_section);
    content.add(action_toolbar);
    content.add(dialog_action_row);
    content_scroll.add(content);

    shell.add(nav);
    shell.add(content_scroll);
    app_root.add(menu_bar);
    app_root.add(menu_separator);
    app_root.add(shell);
    app_root.add(modal_dialog);
    window.content(app_root);
    update_diagnostics_panel(state, "startup");

    if (run_visible) {
        return fiui::run(window);
    }

    const bool duplicate_attach_rejected = !content.add(apply_button);
    const bool apply_ok = apply_button.click();
    const bool theme_callback_ok = theme_button.click();
    const bool layout_dialog_ok = layout_button.click();
    const bool resource_dialog_ok = resources_button.click();
    const bool export_dialog_ok = export_button.click();
    const bool reset_dialog_ok = reset_button.click();
    const bool failure_captured = !failure_button.click();

    fiui::FrameReport first = fiui::render_frame(window);
    update_diagnostics_panel(state, "first_frame");

    const bool balanced_ok = balanced_layout_button.click();
    fiui::FrameReport balanced_frame = fiui::render_frame(window);
    const float balanced_profile_width = profile_section.bounds().width;
    const float balanced_dialog_width = dialog_section.bounds().width;
    const float balanced_visual_height = visual_section.bounds().height;

    const bool dialog_wide_ok = dialog_wide_button.click();
    fiui::FrameReport dialog_wide_frame = fiui::render_frame(window);
    const float dialog_wide_profile_width = profile_section.bounds().width;
    const float dialog_wide_dialog_width = dialog_section.bounds().width;
    const bool dialog_wide_layout_ok =
        dialog_wide_dialog_width > balanced_dialog_width &&
        dialog_wide_profile_width < balanced_profile_width;

    const bool preview_tall_ok = preview_tall_button.click();
    fiui::FrameReport preview_tall_frame = fiui::render_frame(window);
    const float preview_tall_visual_height = visual_section.bounds().height;
    const bool preview_tall_layout_ok = preview_tall_visual_height > balanced_visual_height;

    const bool overflow_layout_ok = overflow_layout_button.click();
    fiui::FrameReport overflow_frame = fiui::render_frame(window);
    const float overflow_profile_width = profile_section.bounds().width;
    const float overflow_dialog_width = dialog_section.bounds().width;
    const float split_handle_width = 8.0f;
    const bool split_bounds_ok =
        std::abs((overflow_profile_width + overflow_dialog_width + split_handle_width) -
                 workspace_split.bounds().width) <= 1.0f;
    const bool overflow_clip_ok = overflow_clip_button.click();
    fiui::FrameReport overflow_clip_frame = fiui::render_frame(window);
    const bool overflow_visible_ok = overflow_visible_button.click();
    fiui::FrameReport overflow_visible_frame = fiui::render_frame(window);

    app.theme("modern.light");
    project_name.value("fiui-control-center-light");
    render_progress.value(0.78f);
    preview.source("control-center-preview-light.png");

    fiui::FrameReport second = fiui::render_frame(window);

    fiui::flush_diagnostics();
    const std::string frame_json =
        read_text_file("fiui-diagnostics/control_center_demo/fiui-frame.json");
    const bool overflow_json_ok =
        frame_json.find("\"overflow_x\":true") != std::string::npos ||
        frame_json.find("\"overflow_y\":true") != std::string::npos;
    const bool overflow_policy_json_ok =
        frame_json.find("\"overflow_policy\":\"visible\"") != std::string::npos;

    print_report("control_center:first", first);
    print_report("control_center:balanced", balanced_frame);
    print_report("control_center:dialog_wide", dialog_wide_frame);
    print_report("control_center:preview_tall", preview_tall_frame);
    print_report("control_center:overflow", overflow_frame);
    print_report("control_center:overflow_clip", overflow_clip_frame);
    print_report("control_center:overflow_visible", overflow_visible_frame);
    print_report("control_center:second", second);
    std::ostringstream self_test;
    std::cout << "control_center apply_count=" << state.apply_count
              << " theme_switch_count=" << state.theme_switch_count
              << " duplicate_attach_rejected=" << (duplicate_attach_rejected ? "true" : "false")
              << " apply_ok=" << (apply_ok ? "true" : "false")
              << " theme_callback_ok=" << (theme_callback_ok ? "true" : "false")
              << " layout_dialog_ok=" << (layout_dialog_ok ? "true" : "false")
              << " resource_dialog_ok=" << (resource_dialog_ok ? "true" : "false")
              << " export_dialog_ok=" << (export_dialog_ok ? "true" : "false")
              << " reset_dialog_ok=" << (reset_dialog_ok ? "true" : "false")
              << " balanced_ok=" << (balanced_ok ? "true" : "false")
              << " dialog_wide_ok=" << (dialog_wide_ok ? "true" : "false")
              << " dialog_wide_layout_ok=" << (dialog_wide_layout_ok ? "true" : "false")
              << " preview_tall_ok=" << (preview_tall_ok ? "true" : "false")
              << " preview_tall_layout_ok=" << (preview_tall_layout_ok ? "true" : "false")
              << " overflow_layout_ok=" << (overflow_layout_ok ? "true" : "false")
              << " overflow_clip_ok=" << (overflow_clip_ok ? "true" : "false")
              << " overflow_visible_ok=" << (overflow_visible_ok ? "true" : "false")
              << " split_bounds_ok=" << (split_bounds_ok ? "true" : "false")
              << " overflow_json_ok=" << (overflow_json_ok ? "true" : "false")
              << " overflow_policy_json_ok=" << (overflow_policy_json_ok ? "true" : "false")
              << " failure_captured=" << (failure_captured ? "true" : "false") << '\n';
    self_test << "duplicate_attach_rejected=" << (duplicate_attach_rejected ? "true" : "false")
              << "\napply_ok=" << (apply_ok ? "true" : "false")
              << "\ntheme_callback_ok=" << (theme_callback_ok ? "true" : "false")
              << "\nlayout_dialog_ok=" << (layout_dialog_ok ? "true" : "false")
              << "\nresource_dialog_ok=" << (resource_dialog_ok ? "true" : "false")
              << "\nexport_dialog_ok=" << (export_dialog_ok ? "true" : "false")
              << "\nreset_dialog_ok=" << (reset_dialog_ok ? "true" : "false")
              << "\nbalanced_ok=" << (balanced_ok ? "true" : "false")
              << "\ndialog_wide_ok=" << (dialog_wide_ok ? "true" : "false")
              << "\ndialog_wide_layout_ok=" << (dialog_wide_layout_ok ? "true" : "false")
              << "\npreview_tall_ok=" << (preview_tall_ok ? "true" : "false")
              << "\npreview_tall_layout_ok=" << (preview_tall_layout_ok ? "true" : "false")
              << "\noverflow_layout_ok=" << (overflow_layout_ok ? "true" : "false")
              << "\noverflow_clip_ok=" << (overflow_clip_ok ? "true" : "false")
              << "\noverflow_visible_ok=" << (overflow_visible_ok ? "true" : "false")
              << "\nsplit_bounds_ok=" << (split_bounds_ok ? "true" : "false")
              << "\noverflow_json_ok=" << (overflow_json_ok ? "true" : "false")
              << "\noverflow_policy_json_ok=" << (overflow_policy_json_ok ? "true" : "false")
              << "\nfailure_captured=" << (failure_captured ? "true" : "false")
              << "\napply_count=" << state.apply_count
              << "\ntheme_switch_count=" << state.theme_switch_count
              << "\ndialog_count=" << state.dialog_count
              << "\nfirst_frame=" << first.frame_id
              << "\nsecond_frame=" << second.frame_id
              << "\nbalanced_profile_width=" << balanced_profile_width
              << "\nbalanced_dialog_width=" << balanced_dialog_width
              << "\nbalanced_visual_height=" << balanced_visual_height
              << "\ndialog_wide_profile_width=" << dialog_wide_profile_width
              << "\ndialog_wide_dialog_width=" << dialog_wide_dialog_width
              << "\npreview_tall_visual_height=" << preview_tall_visual_height
              << "\nworkspace_width=" << workspace_split.bounds().width
              << "\noverflow_profile_width=" << overflow_profile_width
              << "\noverflow_dialog_width=" << overflow_dialog_width
              << "\n";
    write_text_file("fiui-diagnostics/control_center_demo/self-test.txt", self_test.str());

    const bool ok = duplicate_attach_rejected && apply_ok && theme_callback_ok &&
                    layout_dialog_ok && resource_dialog_ok && export_dialog_ok &&
                    reset_dialog_ok && balanced_ok && dialog_wide_ok && dialog_wide_layout_ok &&
                    preview_tall_ok && preview_tall_layout_ok && overflow_layout_ok &&
                    overflow_clip_ok && overflow_visible_ok && split_bounds_ok &&
                    overflow_json_ok && overflow_policy_json_ok && failure_captured &&
                    state.apply_count == 1 && state.theme_switch_count == 1 &&
                    state.dialog_count >= 4 &&
                    second.frame_id > first.frame_id;
    return ok ? 0 : 1;
}
