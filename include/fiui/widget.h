#pragma once

#include "fiui/export.h"
#include "fiui/style.h"
#include "fiui/types.h"

#include <cstdint>

namespace fiui {

struct WidgetImpl;
class PlatformSystem;
class Window;
struct FrameReport;
FIUI_API FrameReport render_frame(const Window& window);

using EventCallback = void (*)(void* user_data);

class FIUI_API Widget {
public:
    Widget();
    Widget(const Widget& other) noexcept;
    Widget(Widget&& other) noexcept;
    Widget& operator=(const Widget& other) noexcept;
    Widget& operator=(Widget&& other) noexcept;
    ~Widget();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] ObjectId object_id() const noexcept;
    [[nodiscard]] std::uint32_t generation() const noexcept;
    [[nodiscard]] std::uint32_t use_count() const noexcept;
    [[nodiscard]] WidgetKind kind() const noexcept;
    [[nodiscard]] LifecycleState lifecycle_state() const noexcept;
    [[nodiscard]] Rect bounds() const noexcept;
    [[nodiscard]] Rect paint_bounds() const noexcept;
    [[nodiscard]] Rect clip_bounds() const noexcept;
    [[nodiscard]] DirtyReason dirty_reason() const noexcept;
    [[nodiscard]] std::uint64_t last_mutation_frame() const noexcept;
    [[nodiscard]] const char* debug_id() const noexcept;
    [[nodiscard]] const char* path() const noexcept;
    [[nodiscard]] std::uint32_t child_count() const noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] bool visible() const noexcept;

    Widget& debug_id(const char* value);
    Widget& style(const char* value);
    Widget& enabled(bool value);
    Widget& visible(bool value);
    Widget& padding(float value);
    Widget& gap(float value);
    Widget& size(float width, float height);
    Widget& fill_width();
    Widget& fill_height();
    Widget& fill();
    Widget& flex(float grow = 1.0f);
    Widget& overflow(OverflowPolicy policy);
    Widget& tooltip(const char* value);
    Widget& mark_dirty(DirtyReason reason);

    bool add(const Widget& child);
    bool detach();

protected:
    explicit Widget(WidgetKind kind, const char* initial_text = nullptr);
    [[nodiscard]] WidgetImpl* impl() const noexcept;

private:
    friend FIUI_API FrameReport render_frame(const Window& window);
    friend class PlatformSystem;
    WidgetImpl* impl_ = nullptr;
};

class FIUI_API Window : public Widget {
public:
    Window();
    explicit Window(const char* title);

    Window& title(const char* value);
    Window& content(const Widget& child);
    Window& size(float width, float height);

private:
    friend FIUI_API FrameReport render_frame(const Window& window);
};

class FIUI_API Text : public Widget {
public:
    explicit Text(const char* value = "");
    Text& text(const char* value);
    Text& style(const char* value);
    Text& multiline(bool enabled = true);
};

class FIUI_API Button : public Widget {
public:
    explicit Button(const char* label = "");
    Button& label(const char* value);
    Button& content(const Widget& child);
    Button& normal_image(const char* resource_path);
    Button& hover_image(const char* resource_path);
    Button& pressed_image(const char* resource_path);
    Button& click_image(const char* resource_path);
    Button& image_fit(ImageFit value);
    Button& text_padding(float value);
    Button& background(Color value);
    Button& hover_background(Color value);
    Button& pressed_background(Color value);
    Button& click_background(Color value);
    Button& radius(float value);
    Button& on_click(EventCallback callback, void* user_data = nullptr);
    bool click();
};

class FIUI_API CheckBox : public Widget {
public:
    explicit CheckBox(const char* label = "");
    CheckBox& label(const char* value);
    CheckBox& checked(bool value);
    [[nodiscard]] bool checked() const noexcept;
    CheckBox& on_change(EventCallback callback, void* user_data = nullptr);
    bool toggle();
};

class FIUI_API RadioButton : public Widget {
public:
    explicit RadioButton(const char* label = "");
    RadioButton& label(const char* value);
    RadioButton& checked(bool value);
    [[nodiscard]] bool checked() const noexcept;
    RadioButton& group(const char* value);
    RadioButton& on_change(EventCallback callback, void* user_data = nullptr);
    bool select();
};

class FIUI_API Switch : public Widget {
public:
    explicit Switch(const char* label = "");
    Switch& label(const char* value);
    Switch& checked(bool value);
    [[nodiscard]] bool checked() const noexcept;
    Switch& on_change(EventCallback callback, void* user_data = nullptr);
    bool toggle();
};

class FIUI_API Input : public Widget {
public:
    Input();
    Input& placeholder(const char* value);
    Input& value(const char* value);
    [[nodiscard]] const char* value() const noexcept;
    Input& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API TextArea : public Widget {
public:
    TextArea();
    TextArea& placeholder(const char* value);
    TextArea& value(const char* value);
    [[nodiscard]] const char* value() const noexcept;
    TextArea& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API Image : public Widget {
public:
    Image();
    explicit Image(const char* resource_path);
    Image& source(const char* resource_path);
    Image& fit(ImageFit value);
    Image& radius(float value);
};

class FIUI_API Progress : public Widget {
public:
    Progress();
    Progress& value(float value);
};

class FIUI_API Slider : public Widget {
public:
    Slider();
    Slider& value(float value);
    [[nodiscard]] float value() const noexcept;
    Slider& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API Select : public Widget {
public:
    Select();
    Select& placeholder(const char* value);
    Select& add_option(const char* label);
    Select& selected_index(std::uint32_t index);
    [[nodiscard]] std::uint32_t selected_index() const noexcept;
    [[nodiscard]] const char* selected_text() const noexcept;
    Select& on_change(EventCallback callback, void* user_data = nullptr);
    bool open();
    bool close();
};

class FIUI_API ListView : public Widget {
public:
    ListView();
    ListView& add_item(const char* label);
    ListView& selected_index(std::uint32_t index);
    [[nodiscard]] std::uint32_t selected_index() const noexcept;
    [[nodiscard]] const char* selected_text() const noexcept;
    ListView& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API TreeItem : public Widget {
public:
    explicit TreeItem(const char* label = "");
    TreeItem& label(const char* value);
    TreeItem& expanded(bool value);
    TreeItem& toggle_expand();
    TreeItem& selected(bool value);
    [[nodiscard]] bool expanded() const noexcept;
    [[nodiscard]] bool selected() const noexcept;
    bool select();
};

class FIUI_API TreeView : public Widget {
public:
    TreeView();
    TreeView& add_item(const TreeItem& item);
    TreeView& selected_id(ObjectId object_id);
    [[nodiscard]] ObjectId selected_id() const noexcept;
    [[nodiscard]] const char* selected_text() const noexcept;
    TreeView& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API TableView : public Widget {
public:
    TableView();
    TableView& add_column(const char* label, float width = 0.0f);
    TableView& add_row(const char* first = "",
                       const char* second = "",
                       const char* third = "",
                       const char* fourth = "");
    TableView& set_cell(std::uint32_t row, std::uint32_t column, const char* value);
    TableView& clear_rows();
    TableView& clear_columns();
    TableView& clear();
    TableView& selected_row(std::uint32_t row);
    TableView& sort_by_column(std::uint32_t column, bool ascending = true);
    [[nodiscard]] std::uint32_t selected_row() const noexcept;
    [[nodiscard]] const char* selected_text() const noexcept;
    [[nodiscard]] std::uint32_t row_count() const noexcept;
    [[nodiscard]] std::uint32_t column_count() const noexcept;
    [[nodiscard]] std::uint32_t sorted_column() const noexcept;
    [[nodiscard]] bool sort_ascending() const noexcept;
    [[nodiscard]] float column_width(std::uint32_t column) const noexcept;
    TableView& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API Tabs : public Widget {
public:
    Tabs();
    Tabs& add_tab(const char* label, const Widget& content);
    Tabs& selected_index(std::uint32_t index);
    [[nodiscard]] std::uint32_t selected_index() const noexcept;
    Tabs& on_change(EventCallback callback, void* user_data = nullptr);
};

class FIUI_API Toolbar : public Widget {
public:
    Toolbar();
};

class FIUI_API Column : public Widget {
public:
    Column();
};

class FIUI_API Row : public Widget {
public:
    Row();
};

class FIUI_API Padding : public Widget {
public:
    explicit Padding(float value = 0.0f);
};

class FIUI_API Align : public Widget {
public:
    Align();
};

class FIUI_API SizedBox : public Widget {
public:
    SizedBox(float width, float height);
};

class FIUI_API ScrollView : public Widget {
public:
    ScrollView();
};

class FIUI_API Dialog : public Widget {
public:
    Dialog();
    Dialog& content(const Widget& child);
    Dialog& open(bool value = true);
    Dialog& close();
    Dialog& modal(bool value = true);
    Dialog& backdrop_closes(bool value = true);
    Dialog& escape_closes(bool value = true);
    [[nodiscard]] bool is_open() const noexcept;
};

class FIUI_API SplitView : public Widget {
public:
    SplitView();
    SplitView& first(const Widget& child);
    SplitView& second(const Widget& child);
    SplitView& orientation(SplitOrientation value);
    SplitView& ratio(float value);
    SplitView& min_pane_size(float value);
    SplitView& handle_size(float value);
    [[nodiscard]] float ratio() const noexcept;
    [[nodiscard]] SplitOrientation orientation() const noexcept;
};

class FIUI_API MenuBar : public Widget {
public:
    MenuBar();
};

class FIUI_API MenuItem : public Widget {
public:
    explicit MenuItem(const char* label = "");
    MenuItem& label(const char* value);
    MenuItem& enabled(bool value);
    MenuItem& checked(bool value);
    MenuItem& shortcut(const char* value);
    MenuItem& on_click(EventCallback callback, void* user_data = nullptr);
    bool click();
};

class FIUI_API Separator : public Widget {
public:
    Separator();
};

class FIUI_API Spacer : public Widget {
public:
    Spacer();
};

} // namespace fiui
