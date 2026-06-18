#include <fiui/fiui.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct SettingsState {
    int save_count = 0;
    int theme_count = 0;
    int dialog_count = 0;
    int nav_count = 0;
    int layout_mode = 0;
    bool dark_theme = true;
    bool advanced_visible = false;
    fiui::Application* app = nullptr;
    fiui::Text* status = nullptr;
    fiui::Text* layout_status = nullptr;
    fiui::Text* diagnostics = nullptr;
    fiui::Text* dialog_body = nullptr;
    fiui::Dialog* confirm_dialog = nullptr;
    fiui::SplitView* workspace = nullptr;
    fiui::Tabs* tabs = nullptr;
    fiui::Input* project_name = nullptr;
    fiui::TextArea* advanced_notes = nullptr;
    fiui::Progress* progress = nullptr;
    fiui::Slider* render_budget = nullptr;
    fiui::Select* environment = nullptr;
    fiui::CheckBox* telemetry = nullptr;
    fiui::Switch* live_preview = nullptr;
    fiui::TreeView* navigation = nullptr;
    fiui::TableView* audit_table = nullptr;
};

void refresh_diagnostics(SettingsState& state, const char* reason)
{
    if (state.diagnostics == nullptr) {
        return;
    }
    std::ostringstream text;
    text << "Diagnostics: saves=" << state.save_count
         << " theme=" << (state.dark_theme ? "dark" : "light")
         << " dialogs=" << state.dialog_count
         << " nav=" << state.nav_count
         << " env=" << (state.environment == nullptr ? "" : state.environment->selected_text())
         << " budget="
         << (state.render_budget == nullptr ? 0 : static_cast<int>(state.render_budget->value() * 100.0f))
         << "% reason=" << (reason == nullptr ? "update" : reason);
    state.diagnostics->text(text.str().c_str());
}

void refresh_audit_table(SettingsState& state, const char* reason)
{
    if (state.audit_table == nullptr) {
        return;
    }
    std::ostringstream saves;
    saves << state.save_count;
    std::ostringstream theme_count;
    theme_count << state.theme_count;
    std::ostringstream budget;
    budget << (state.render_budget == nullptr ? 0 : static_cast<int>(state.render_budget->value() * 100.0f)) << "%";
    state.audit_table->clear_rows()
        .add_row("Project", state.project_name == nullptr ? "" : state.project_name->value(), "active")
        .add_row("Environment", state.environment == nullptr ? "" : state.environment->selected_text(), "selected")
        .add_row("Render budget", budget.str().c_str(), "slider")
        .add_row("Telemetry", state.telemetry != nullptr && state.telemetry->checked() ? "enabled" : "disabled", "checkbox")
        .add_row("Live preview", state.live_preview != nullptr && state.live_preview->checked() ? "on" : "off", "switch")
        .add_row("Save count", saves.str().c_str(), "callback")
        .add_row("Theme count", theme_count.str().c_str(), "callback")
        .add_row("Last reason", reason == nullptr ? "startup" : reason, "diagnostic");
    state.audit_table->selected_row(0);
}

void set_status(SettingsState& state, const char* text)
{
    if (state.status != nullptr) {
        state.status->text(text);
    }
    refresh_diagnostics(state, text);
    refresh_audit_table(state, text);
}

void save_settings(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr) {
        return;
    }
    ++state->save_count;
    if (state->progress != nullptr) {
        state->progress->value(state->save_count % 2 == 0 ? 0.72f : 0.48f);
    }
    set_status(*state, "Settings saved");
}

void switch_theme(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr || state->app == nullptr) {
        return;
    }
    ++state->theme_count;
    state->dark_theme = !state->dark_theme;
    state->app->theme(state->dark_theme ? "modern.dark" : "modern.light");
    set_status(*state, state->dark_theme ? "Theme switched to dark" : "Theme switched to light");
}

void open_confirm_dialog(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr) {
        return;
    }
    ++state->dialog_count;
    if (state->confirm_dialog != nullptr) {
        state->confirm_dialog->open();
    }
    if (state->dialog_body != nullptr) {
        state->dialog_body->text("Apply the current profile, theme, and diagnostic options?");
    }
    set_status(*state, "Confirmation dialog opened");
}

void close_confirm_dialog(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr) {
        return;
    }
    if (state->confirm_dialog != nullptr) {
        state->confirm_dialog->close();
    }
    set_status(*state, "Confirmation dialog closed");
}

void toggle_advanced(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr || state->advanced_notes == nullptr) {
        return;
    }
    state->advanced_visible = !state->advanced_visible;
    state->advanced_notes->visible(state->advanced_visible);
    set_status(*state, state->advanced_visible ? "Advanced notes shown" : "Advanced notes hidden");
}

void change_navigation(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr || state->navigation == nullptr) {
        return;
    }
    ++state->nav_count;
    std::ostringstream text;
    text << "Navigation: " << state->navigation->selected_text();
    set_status(*state, text.str().c_str());
}

void change_runtime_option(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state != nullptr) {
        set_status(*state, "Runtime option changed");
    }
}

void change_budget(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state == nullptr || state->render_budget == nullptr) {
        return;
    }
    if (state->progress != nullptr) {
        state->progress->value(state->render_budget->value());
    }
    set_status(*state, "Render budget changed");
}

void apply_layout_mode(SettingsState& state, int mode, const char* label)
{
    state.layout_mode = mode;
    if (state.workspace != nullptr) {
        if (mode == 1) {
            state.workspace->ratio(0.20f);
            state.workspace->min_pane_size(160.0f);
        } else if (mode == 2) {
            state.workspace->ratio(0.34f);
            state.workspace->min_pane_size(220.0f);
        } else if (mode == 3) {
            state.workspace->ratio(0.26f);
            state.workspace->min_pane_size(180.0f);
        } else {
            state.workspace->ratio(0.28f);
            state.workspace->min_pane_size(180.0f);
        }
    }
    if (state.tabs != nullptr) {
        if (mode == 2) {
            state.tabs->selected_index(1);
        } else if (mode == 3) {
            state.tabs->selected_index(2);
        } else {
            state.tabs->selected_index(0);
        }
    }
    if (state.navigation != nullptr) {
        state.navigation->size(0.0f, mode == 1 ? 170.0f : 240.0f);
    }
    if (state.audit_table != nullptr) {
        state.audit_table->size(0.0f, mode == 2 ? 300.0f : 210.0f);
    }
    if (state.advanced_notes != nullptr) {
        const bool show_advanced = mode == 1 || mode == 3 || state.advanced_visible;
        state.advanced_notes->visible(show_advanced);
    }
    if (state.layout_status != nullptr) {
        std::ostringstream text;
        text << "Layout mode: " << (label == nullptr ? "balanced" : label)
             << " | split=" << (state.workspace == nullptr ? 0 : static_cast<int>(state.workspace->ratio() * 100.0f))
             << "% | tab=" << (state.tabs == nullptr ? 0 : state.tabs->selected_index());
        state.layout_status->text(text.str().c_str());
    }
    set_status(state, label == nullptr ? "Layout mode changed" : label);
}

void layout_balanced(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 0, "Balanced layout");
    }
}

void layout_form_focus(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 1, "Form focus layout");
    }
}

void layout_audit_focus(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 2, "Audit focus layout");
    }
}

void layout_diagnostics_focus(void* user_data)
{
    auto* state = static_cast<SettingsState*>(user_data);
    if (state != nullptr) {
        apply_layout_mode(*state, 3, "Diagnostics focus layout");
    }
}

void throw_diagnostics_error(void*)
{
    throw std::runtime_error("intentional settings_panel callback failure");
}

void print_report(const char* label, const fiui::FrameReport& report)
{
    std::cout << label << " frame_id=" << report.frame_id
              << " dirty=" << report.original_dirty_rects << " merged=" << report.merged_dirty_rects
              << " full_repaint=" << (report.full_repaint ? "true" : "false")
              << " fallback=" << report.fallback_reason << " layout_ms=" << report.layout_ms
              << " display_ms=" << report.display_list_ms << '\n';
}

bool has_arg(int argc, char** argv, const char* value)
{
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], value) == 0) {
            return true;
        }
    }
    return false;
}

void write_text_file(const char* path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
}

} // namespace

int main(int argc, char** argv)
{
    const bool run_visible = has_arg(argc, argv, "--run") || has_arg(argc, argv, "--run-ai");
    const bool ai_diagnostics = has_arg(argc, argv, "--ai-diagnostics") || has_arg(argc, argv, "--run-ai");

    fiui::DiagnosticsConfig diagnostics;
    diagnostics.mode = (run_visible && !ai_diagnostics) ? fiui::DebugMode::Basic
                                                        : fiui::DebugMode::AiFriendly;
    diagnostics.output_directory = "fiui-diagnostics/settings_panel";
    fiui::configure_diagnostics(diagnostics);

    fiui::Application app;
    app.theme("modern.dark");
    SettingsState state;
    state.app = &app;

    fiui::Window main_window("Settings");
    main_window.size(980, 680);
    main_window.debug_id("settings_window");

    fiui::Column root;
    root.debug_id("settings_root").padding(fiui::space::page).gap(fiui::space::md).fill();

    fiui::MenuBar menu_bar;
    menu_bar.debug_id("settings_menu").size(0.0f, 26.0f);
    fiui::MenuItem file_menu("File");
    file_menu.debug_id("menu_file");
    fiui::MenuItem file_save("Save settings");
    file_save.debug_id("menu_file_save");
    file_save.shortcut("Ctrl+S");
    file_save.on_click(save_settings, &state);
    fiui::MenuItem file_confirm("Confirm changes");
    file_confirm.debug_id("menu_file_confirm");
    file_confirm.on_click(open_confirm_dialog, &state);
    fiui::MenuItem view_menu("View");
    view_menu.debug_id("menu_view");
    fiui::MenuItem view_theme("Toggle theme");
    view_theme.debug_id("menu_view_theme");
    view_theme.shortcut("Ctrl+T");
    view_theme.on_click(switch_theme, &state);
    fiui::MenuItem view_advanced("Toggle advanced notes");
    view_advanced.debug_id("menu_view_advanced");
    view_advanced.on_click(toggle_advanced, &state);
    fiui::MenuItem diagnostics_menu("Diagnostics");
    diagnostics_menu.debug_id("menu_diagnostics");
    fiui::MenuItem diagnostics_throw("Trigger callback failure");
    diagnostics_throw.debug_id("menu_diagnostics_throw");
    diagnostics_throw.on_click(throw_diagnostics_error);
    file_menu.add(file_save);
    file_menu.add(file_confirm);
    view_menu.add(view_theme);
    view_menu.add(view_advanced);
    diagnostics_menu.add(diagnostics_throw);
    menu_bar.add(file_menu);
    menu_bar.add(view_menu);
    menu_bar.add(diagnostics_menu);

    fiui::Text title("Settings Acceptance");
    title.debug_id("title").style(fiui::text::title);
    fiui::Text subtitle("Public C++ API settings tool: navigation, forms, tables, dialogs, themes");
    subtitle.debug_id("subtitle").style(fiui::text::caption);

    fiui::Toolbar toolbar;
    toolbar.debug_id("toolbar").gap(fiui::space::sm).size(0.0f, 44.0f);
    fiui::Button save_button("Save");
    save_button.debug_id("save_button");
    save_button.style("primary");
    save_button.on_click(save_settings, &state);
    fiui::Button theme_button("Theme");
    theme_button.debug_id("theme_button");
    theme_button.on_click(switch_theme, &state);
    fiui::Button advanced_button("Advanced");
    advanced_button.debug_id("advanced_button");
    advanced_button.on_click(toggle_advanced, &state);
    fiui::Button confirm_button("Confirm");
    confirm_button.debug_id("confirm_button");
    confirm_button.on_click(open_confirm_dialog, &state);
    fiui::Button failing_button("Diagnostics");
    failing_button.debug_id("failing_button");
    failing_button.style("danger");
    failing_button.on_click(throw_diagnostics_error);
    fiui::Button balanced_layout_button("Balanced");
    balanced_layout_button.debug_id("balanced_layout_button");
    balanced_layout_button.tooltip("Use the default settings split layout");
    balanced_layout_button.on_click(layout_balanced, &state);
    fiui::Button form_layout_button("Form");
    form_layout_button.debug_id("form_layout_button");
    form_layout_button.tooltip("Narrow the navigation pane and show the form controls");
    form_layout_button.on_click(layout_form_focus, &state);
    fiui::Button audit_layout_button("Audit");
    audit_layout_button.debug_id("audit_layout_button");
    audit_layout_button.tooltip("Open the runtime tab and expand the audit table");
    audit_layout_button.on_click(layout_audit_focus, &state);
    fiui::Button diagnostics_layout_button("Trace");
    diagnostics_layout_button.debug_id("diagnostics_layout_button");
    diagnostics_layout_button.tooltip("Open the diagnostics tab and reveal advanced notes");
    diagnostics_layout_button.on_click(layout_diagnostics_focus, &state);
    toolbar.add(save_button);
    toolbar.add(theme_button);
    toolbar.add(advanced_button);
    toolbar.add(confirm_button);
    toolbar.add(failing_button);
    toolbar.add(balanced_layout_button);
    toolbar.add(form_layout_button);
    toolbar.add(audit_layout_button);
    toolbar.add(diagnostics_layout_button);

    fiui::SplitView workspace;
    workspace.debug_id("workspace");
    workspace.flex();
    workspace.ratio(0.28f);
    workspace.min_pane_size(180.0f);
    state.workspace = &workspace;

    fiui::Column navigation_panel;
    navigation_panel.debug_id("navigation_panel").padding(fiui::space::md).gap(fiui::space::sm);
    fiui::Text navigation_title("Sections");
    navigation_title.debug_id("navigation_title").style(fiui::text::caption);
    fiui::TreeView navigation;
    navigation.debug_id("navigation_tree");
    navigation.size(0.0f, 220.0f);
    navigation.on_change(change_navigation, &state);
    state.navigation = &navigation;
    fiui::TreeItem project_section("Project");
    project_section.debug_id("nav_project");
    fiui::TreeItem runtime_section("Runtime");
    runtime_section.debug_id("nav_runtime");
    fiui::TreeItem diagnostics_section("Diagnostics");
    diagnostics_section.debug_id("nav_diagnostics");
    runtime_section.add(diagnostics_section);
    navigation.add_item(project_section);
    navigation.add_item(runtime_section);
    navigation.selected_id(project_section.object_id());
    fiui::Text navigation_hint("Tree selection updates the status and diagnostics panels.");
    navigation_hint.debug_id("navigation_hint");
    navigation_hint.style(fiui::text::caption);
    navigation_hint.multiline();
    navigation_panel.add(navigation_title);
    navigation_panel.add(navigation);
    navigation_panel.add(navigation_hint);

    fiui::Column content_panel;
    content_panel.debug_id("content_panel").padding(fiui::space::md).gap(fiui::space::sm).flex();

    fiui::Tabs tabs;
    tabs.debug_id("settings_tabs").fill();
    state.tabs = &tabs;
    fiui::Column project_tab;
    project_tab.debug_id("project_tab").gap(fiui::space::sm);
    fiui::Input project_name;
    project_name.debug_id("project_name");
    project_name.placeholder("Project name");
    project_name.value("fiui-runtime");
    state.project_name = &project_name;
    fiui::Select environment;
    environment.debug_id("environment_select");
    environment.placeholder("Environment");
    environment.add_option("Development");
    environment.add_option("Staging");
    environment.add_option("Production");
    environment.on_change(change_runtime_option, &state);
    state.environment = &environment;
    fiui::CheckBox telemetry("Enable telemetry diagnostics");
    telemetry.debug_id("telemetry");
    telemetry.checked(true);
    telemetry.on_change(change_runtime_option, &state);
    state.telemetry = &telemetry;
    fiui::Switch live_preview("Live preview");
    live_preview.debug_id("live_preview");
    live_preview.checked(true);
    live_preview.on_change(change_runtime_option, &state);
    state.live_preview = &live_preview;
    fiui::TextArea advanced_notes;
    advanced_notes.debug_id("advanced_notes");
    advanced_notes.placeholder("Advanced notes");
    advanced_notes.value("Advanced settings are hidden by default.\nUse the toolbar to reveal them.");
    advanced_notes.visible(false);
    state.advanced_notes = &advanced_notes;
    project_tab.add(project_name);
    project_tab.add(environment);
    project_tab.add(telemetry);
    project_tab.add(live_preview);
    project_tab.add(advanced_notes);

    fiui::Column runtime_tab;
    runtime_tab.debug_id("runtime_tab").gap(fiui::space::sm);
    fiui::Text budget_label("Render budget");
    budget_label.debug_id("budget_label").style(fiui::text::caption);
    fiui::Slider render_budget;
    render_budget.debug_id("render_budget");
    render_budget.value(0.42f);
    render_budget.on_change(change_budget, &state);
    state.render_budget = &render_budget;
    fiui::Progress progress;
    progress.debug_id("progress");
    progress.value(0.42f);
    state.progress = &progress;
    fiui::TableView audit_table;
    audit_table.debug_id("audit_table").size(0.0f, 210.0f);
    audit_table.add_column("Setting", 150.0f)
        .add_column("Value", 130.0f)
        .add_column("Source", 110.0f);
    state.audit_table = &audit_table;
    runtime_tab.add(budget_label);
    runtime_tab.add(render_budget);
    runtime_tab.add(progress);
    runtime_tab.add(audit_table);

    fiui::Column diagnostics_tab;
    diagnostics_tab.debug_id("diagnostics_tab").gap(fiui::space::sm);
    fiui::Text diagnostics_text("Diagnostics: ready");
    diagnostics_text.debug_id("diagnostics_text");
    diagnostics_text.style(fiui::text::caption);
    diagnostics_text.multiline();
    state.diagnostics = &diagnostics_text;
    fiui::Button diagnostics_button("Trigger callback failure");
    diagnostics_button.debug_id("diagnostics_button");
    diagnostics_button.style("danger");
    diagnostics_button.on_click(throw_diagnostics_error);
    diagnostics_tab.add(diagnostics_text);
    diagnostics_tab.add(diagnostics_button);

    tabs.add_tab("Project", project_tab);
    tabs.add_tab("Runtime", runtime_tab);
    tabs.add_tab("Diagnostics", diagnostics_tab);
    content_panel.add(tabs);

    workspace.first(navigation_panel);
    workspace.second(content_panel);

    fiui::Text status("Status: ready");
    status.debug_id("status").style(fiui::text::caption);
    state.status = &status;
    fiui::Text layout_status("Layout mode: balanced");
    layout_status.debug_id("layout_status");
    layout_status.style(fiui::text::caption);
    state.layout_status = &layout_status;

    fiui::Dialog confirm_dialog;
    confirm_dialog.debug_id("confirm_dialog");
    confirm_dialog.open(false);
    confirm_dialog.backdrop_closes(true);
    confirm_dialog.escape_closes(true);
    state.confirm_dialog = &confirm_dialog;
    fiui::Column dialog_panel;
    dialog_panel.debug_id("dialog_panel").padding(fiui::space::md).gap(fiui::space::sm).size(360.0f, 0.0f);
    fiui::Text dialog_title("Confirm Changes");
    dialog_title.debug_id("dialog_title").style(fiui::text::title);
    fiui::Text dialog_body("Apply the current settings?");
    dialog_body.debug_id("dialog_body");
    dialog_body.style(fiui::text::caption);
    dialog_body.multiline();
    state.dialog_body = &dialog_body;
    fiui::Row dialog_actions;
    dialog_actions.debug_id("dialog_actions").gap(fiui::space::sm);
    fiui::Button dialog_apply("Apply");
    dialog_apply.debug_id("dialog_apply");
    dialog_apply.style("primary");
    dialog_apply.on_click(save_settings, &state);
    fiui::Button dialog_close("Close");
    dialog_close.debug_id("dialog_close");
    dialog_close.on_click(close_confirm_dialog, &state);
    dialog_actions.add(dialog_apply);
    dialog_actions.add(dialog_close);
    dialog_panel.add(dialog_title);
    dialog_panel.add(dialog_body);
    dialog_panel.add(dialog_actions);
    confirm_dialog.content(dialog_panel);

    root.add(menu_bar);
    root.add(title);
    root.add(subtitle);
    root.add(toolbar);
    root.add(workspace);
    root.add(layout_status);
    root.add(status);
    root.add(confirm_dialog);

    main_window.content(root);
    refresh_audit_table(state, "startup");
    refresh_diagnostics(state, "startup");

    if (run_visible) {
        return fiui::run(main_window);
    }

    const bool duplicate_attach_rejected = !root.add(save_button);
    const bool save_ok = save_button.click();
    const bool theme_ok = theme_button.click();
    const bool advanced_shown = advanced_button.click() && advanced_notes.visible();
    const bool confirm_opened = confirm_button.click() && confirm_dialog.is_open();
    const bool dialog_closed = dialog_close.click() && !confirm_dialog.is_open();
    const bool failure_captured = !failing_button.click();
    const bool balanced_click_ok = balanced_layout_button.click();
    const fiui::FrameReport balanced_frame = fiui::render_frame(main_window);
    const float balanced_nav_width = navigation_panel.bounds().width;
    const float balanced_audit_height = audit_table.bounds().height;
    const bool form_layout_ok = form_layout_button.click() && state.layout_mode == 1 &&
                                tabs.selected_index() == 0 && advanced_notes.visible();
    const fiui::FrameReport form_frame = fiui::render_frame(main_window);
    const float form_nav_width = navigation_panel.bounds().width;
    const bool audit_layout_ok = audit_layout_button.click() && state.layout_mode == 2 &&
                                 tabs.selected_index() == 1;
    const fiui::FrameReport audit_frame = fiui::render_frame(main_window);
    const float audit_table_height = audit_table.bounds().height;
    const bool diagnostics_layout_ok = diagnostics_layout_button.click() && state.layout_mode == 3 &&
                                       tabs.selected_index() == 2 && advanced_notes.visible();
    const fiui::FrameReport diagnostics_frame = fiui::render_frame(main_window);
    const bool balanced_layout_ok = balanced_layout_button.click() && state.layout_mode == 0 &&
                                    tabs.selected_index() == 0;
    const fiui::FrameReport final_balanced_frame = fiui::render_frame(main_window);
    const bool form_layout_bounds_ok = form_nav_width < balanced_nav_width;
    const bool audit_layout_bounds_ok = audit_table_height > balanced_audit_height;
    const bool layout_frame_order_ok =
        balanced_frame.frame_id < form_frame.frame_id && form_frame.frame_id < audit_frame.frame_id &&
        audit_frame.frame_id < diagnostics_frame.frame_id &&
        diagnostics_frame.frame_id < final_balanced_frame.frame_id;

    project_name.value("fiui-runtime-v0");
    environment.selected_index(2);
    render_budget.value(0.75f);
    refresh_audit_table(state, "programmatic_update");
    refresh_diagnostics(state, "programmatic_update");

    fiui::FrameReport first = fiui::render_frame(main_window);

    app.theme("modern.light");
    state.dark_theme = false;
    refresh_diagnostics(state, "final_light_theme");
    fiui::FrameReport second = fiui::render_frame(main_window);

    fiui::flush_diagnostics();

    print_report("settings_panel:balanced", balanced_frame);
    print_report("settings_panel:form", form_frame);
    print_report("settings_panel:audit", audit_frame);
    print_report("settings_panel:diagnostics", diagnostics_frame);
    print_report("settings_panel:final_balanced", final_balanced_frame);
    print_report("settings_panel:first", first);
    print_report("settings_panel:second", second);
    std::ostringstream self_test;
    self_test << "duplicate_attach_rejected=" << (duplicate_attach_rejected ? "true" : "false")
              << "\nsave_ok=" << (save_ok ? "true" : "false")
              << "\ntheme_ok=" << (theme_ok ? "true" : "false")
              << "\nadvanced_shown=" << (advanced_shown ? "true" : "false")
              << "\nconfirm_opened=" << (confirm_opened ? "true" : "false")
              << "\ndialog_closed=" << (dialog_closed ? "true" : "false")
              << "\nbalanced_click_ok=" << (balanced_click_ok ? "true" : "false")
              << "\nform_layout_ok=" << (form_layout_ok ? "true" : "false")
              << "\nform_layout_bounds_ok=" << (form_layout_bounds_ok ? "true" : "false")
              << "\naudit_layout_ok=" << (audit_layout_ok ? "true" : "false")
              << "\naudit_layout_bounds_ok=" << (audit_layout_bounds_ok ? "true" : "false")
              << "\ndiagnostics_layout_ok=" << (diagnostics_layout_ok ? "true" : "false")
              << "\nbalanced_layout_ok=" << (balanced_layout_ok ? "true" : "false")
              << "\nlayout_frame_order_ok=" << (layout_frame_order_ok ? "true" : "false")
              << "\nfailure_captured=" << (failure_captured ? "true" : "false")
              << "\nsave_count=" << state.save_count
              << "\ntheme_count=" << state.theme_count
              << "\ndialog_count=" << state.dialog_count
              << "\nfinal_layout_mode=" << state.layout_mode
              << "\nfinal_theme=" << (state.dark_theme ? "dark" : "light")
              << "\nfinal_environment=" << environment.selected_text()
              << "\nfinal_project_name=" << project_name.value()
              << "\nrender_budget=" << render_budget.value()
              << "\nbalanced_frame=" << balanced_frame.frame_id
              << "\nform_frame=" << form_frame.frame_id
              << "\naudit_frame=" << audit_frame.frame_id
              << "\ndiagnostics_frame=" << diagnostics_frame.frame_id
              << "\nfinal_balanced_frame=" << final_balanced_frame.frame_id
              << "\nfirst_frame=" << first.frame_id
              << "\nsecond_frame=" << second.frame_id
              << "\nworkspace_width=" << workspace.bounds().width
              << "\nworkspace_height=" << workspace.bounds().height
              << "\nbalanced_nav_width=" << balanced_nav_width
              << "\nform_nav_width=" << form_nav_width
              << "\nbalanced_audit_height=" << balanced_audit_height
              << "\naudit_table_height=" << audit_table_height
              << "\nfinal_tab_index=" << tabs.selected_index()
              << "\nadvanced_visible=" << (advanced_notes.visible() ? "true" : "false")
              << "\nstatus_debug_id=" << status.debug_id()
              << "\n";
    write_text_file("fiui-diagnostics/settings_panel/self-test.txt", self_test.str());

    std::cout << "settings_panel save_count=" << state.save_count
              << " theme_count=" << state.theme_count
              << " duplicate_attach_rejected=" << (duplicate_attach_rejected ? "true" : "false")
              << " save_ok=" << (save_ok ? "true" : "false")
              << " theme_ok=" << (theme_ok ? "true" : "false")
              << " advanced_shown=" << (advanced_shown ? "true" : "false")
              << " confirm_opened=" << (confirm_opened ? "true" : "false")
              << " dialog_closed=" << (dialog_closed ? "true" : "false")
              << " balanced_click_ok=" << (balanced_click_ok ? "true" : "false")
              << " form_layout_ok=" << (form_layout_ok ? "true" : "false")
              << " form_layout_bounds_ok=" << (form_layout_bounds_ok ? "true" : "false")
              << " audit_layout_ok=" << (audit_layout_ok ? "true" : "false")
              << " audit_layout_bounds_ok=" << (audit_layout_bounds_ok ? "true" : "false")
              << " diagnostics_layout_ok=" << (diagnostics_layout_ok ? "true" : "false")
              << " balanced_layout_ok=" << (balanced_layout_ok ? "true" : "false")
              << " layout_frame_order_ok=" << (layout_frame_order_ok ? "true" : "false")
              << " failure_captured=" << (failure_captured ? "true" : "false") << '\n';

    return duplicate_attach_rejected && save_ok && theme_ok && advanced_shown &&
                   confirm_opened && dialog_closed && balanced_click_ok && form_layout_ok &&
                   form_layout_bounds_ok && audit_layout_ok && audit_layout_bounds_ok &&
                   diagnostics_layout_ok && balanced_layout_ok && layout_frame_order_ok &&
                   failure_captured && state.save_count >= 1
               ? 0
               : 1;
}
