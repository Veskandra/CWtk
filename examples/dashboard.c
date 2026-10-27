/* WinTk dashboard example: build with `mingw32-make dashboard`. */
#include "wintk.h"

#include <stdio.h>

typedef struct DashboardState {
    WtkWidget *window;
    WtkWidget *slider;
    WtkWidget *progress;
    WtkWidget *value_label;
    WtkWidget *selection_label;
    WtkWidget *start_button;
    unsigned timer;
} DashboardState;

static void update_value(DashboardState *state, int value)
{
    char text[64];
    if (value < 0)
        value = 0;
    if (value > 100)
        value = 100;
    wtk_progress_set(state->progress, (double)value / 100.0);
    snprintf(text, sizeof(text), "当前进度：%d%%", value);
    wtk_label_set_text(state->value_label, text);
}

static void on_slider_change(WtkWidget *widget, WtkEvent *event, void *data)
{
    DashboardState *state = (DashboardState *)data;
    (void)widget;
    (void)event;
    update_value(state, wtk_slider_get(state->slider));
}

static void animation_tick(WtkApp *app, void *data)
{
    DashboardState *state = (DashboardState *)data;
    int value = wtk_slider_get(state->slider) + 2;
    state->timer = 0;
    if (value > 100)
        value = 0;
    wtk_slider_set(state->slider, value);
    update_value(state, value);
    state->timer = wtk_after(app, 80, animation_tick, state);
}

static void on_start_stop(WtkWidget *widget, WtkEvent *event, void *data)
{
    DashboardState *state = (DashboardState *)data;
    (void)widget;
    (void)event;
    if (state->timer) {
        wtk_cancel_timer(wtk_widget_app(state->window), state->timer);
        state->timer = 0;
        wtk_button_set_text(state->start_button, "开始演示");
        return;
    }
    wtk_button_set_text(state->start_button, "暂停演示");
    state->timer = wtk_after(wtk_widget_app(state->window), 80,
                             animation_tick, state);
}

static void on_list_change(WtkWidget *widget, WtkEvent *event, void *data)
{
    DashboardState *state = (DashboardState *)data;
    int index;
    const char *item;
    char text[256];

    (void)event;
    index = wtk_listbox_get_selection(widget);
    if (index < 0) {
        wtk_label_set_text(state->selection_label, "尚未选择任务。");
        return;
    }
    item = wtk_listbox_get_item(widget, index);
    snprintf(text, sizeof(text), "已选择：%s", item ? item : "");
    wtk_label_set_text(state->selection_label, text);
}

static void on_close(WtkWidget *widget, WtkEvent *event, void *data)
{
    DashboardState *state = (DashboardState *)data;
    (void)widget;
    (void)event;
    if (state->timer) {
        wtk_cancel_timer(wtk_widget_app(state->window), state->timer);
        state->timer = 0;
    }
}

static WtkWidget *make_card(WtkWidget *parent, const WtkTheme *theme)
{
    WtkWidget *card = wtk_frame(parent);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 14);
    wtk_widget_set_padding(card, 22, 20, 22, 18);
    return card;
}

static int run_dashboard(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *header;
    WtkWidget *title;
    WtkWidget *subtitle;
    WtkWidget *body;
    WtkWidget *left;
    WtkWidget *right;
    WtkWidget *left_title;
    WtkWidget *left_hint;
    WtkWidget *list;
    WtkWidget *right_title;
    WtkWidget *right_hint;
    WtkWidget *row;
    WtkWidget *start;
    WtkWidget *separator;
    const WtkTheme *theme;
    DashboardState state;

    if (!app)
        return 1;
    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk 任务面板", 1040, 680);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 820, 600);

    header = wtk_frame(window);
    wtk_widget_set_background(header, theme->window_bg);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 28, 20);

    title = wtk_label(header, "任务面板");
    wtk_widget_set_font(title, NULL, 26, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    subtitle = wtk_label(header,
                         "列表选择、滑块、进度条和定时器的组合示例。");
    wtk_widget_set_foreground(subtitle, theme->muted);
    wtk_widget_set_font(subtitle, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(subtitle, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 5);

    body = wtk_frame(window);
    wtk_widget_set_background(body, theme->window_bg);
    wtk_pack(body, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 28, 0);

    left = make_card(body, theme);
    wtk_widget_set_size(left, 300, 0);
    wtk_pack(left, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 16);

    left_title = wtk_label(left, "今日任务");
    wtk_widget_set_font(left_title, NULL, 15, WTK_FONT_BOLD);
    wtk_pack(left_title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    left_hint = wtk_label(left, "单击列表项查看选择。");
    wtk_widget_set_foreground(left_hint, theme->muted);
    wtk_widget_set_font(left_hint, NULL, 9, WTK_FONT_NORMAL);
    wtk_pack(left_hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    list = wtk_listbox(left);
    wtk_listbox_add(list, "整理 API 文档");
    wtk_listbox_add(list, "补充 Canvas 示例");
    wtk_listbox_add(list, "检查高 DPI 布局");
    wtk_listbox_add(list, "发布 Windows 构建");
    wtk_widget_set_size(list, 0, 0);
    wtk_pack(list, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    wtk_listbox_select(list, 0);

    right = make_card(body, theme);
    wtk_pack(right, WTK_SIDE_LEFT, WTK_FILL_BOTH, 1, 0, 0);

    right_title = wtk_label(right, "进度演示");
    wtk_widget_set_font(right_title, NULL, 15, WTK_FONT_BOLD);
    wtk_pack(right_title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    right_hint = wtk_label(right,
                           "拖动滑块，或启动自动演示。进度由 wtk_after() 每 80ms 更新。");
    wtk_widget_set_foreground(right_hint, theme->muted);
    wtk_widget_set_font(right_hint, NULL, 9, WTK_FONT_NORMAL);
    wtk_label_set_wrap_width(right_hint, 520);
    wtk_pack(right_hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    state.value_label = wtk_label(right, "当前进度：35%");
    wtk_widget_set_font(state.value_label, NULL, 13, WTK_FONT_BOLD);
    wtk_pack(state.value_label, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 18);

    state.slider = wtk_slider(right, 0, 100, 35);
    wtk_widget_set_size(state.slider, 0, 32);
    wtk_pack(state.slider, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);

    state.progress = wtk_progress(right);
    wtk_progress_set_show_percent(state.progress, 0);
    wtk_widget_set_size(state.progress, 0, 22);
    wtk_pack(state.progress, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);

    row = wtk_frame(right);
    wtk_widget_set_background(row, theme->surface);
    wtk_pack(row, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    start = wtk_button(row, "开始演示");
    state.start_button = start;
    wtk_button_set_style(start, WTK_BUTTON_PRIMARY);
    wtk_widget_set_size(start, 160, 38);
    wtk_pack(start, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);

    separator = wtk_separator(right);
    wtk_pack(separator, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 18);

    state.selection_label = wtk_label(right, "已选择：整理 API 文档");
    wtk_widget_set_foreground(state.selection_label, theme->text);
    wtk_widget_set_font(state.selection_label, NULL, 10, WTK_FONT_NORMAL);
    wtk_label_set_wrap_width(state.selection_label, 520);
    wtk_pack(state.selection_label, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    state.window = window;
    state.timer = 0;
    update_value(&state, 35);
    wtk_bind(state.slider, WTK_EVENT_CHANGE, on_slider_change, &state);
    wtk_bind(start, WTK_EVENT_CLICK, on_start_stop, &state);
    wtk_bind(list, WTK_EVENT_CHANGE, on_list_change, &state);
    wtk_bind(window, WTK_EVENT_CLOSE, on_close, &state);

    wtk_window_center(window);
    wtk_run(app);
    wtk_app_free(app);
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
                   int show_command)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;
    return run_dashboard();
}
