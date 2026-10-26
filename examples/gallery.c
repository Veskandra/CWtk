/*
 * WinTk Gallery - a compact demo of the toolkit.
 *
 * Build: mingw32-make gallery
 * Run:   build\wintk_gallery.exe
 */
#include "wintk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct GalleryState GalleryState;

typedef struct NavData {
    GalleryState *state;
    int index;
} NavData;

struct GalleryState {
    WtkApp *app;
    WtkWidget *window;
    const WtkTheme *theme;

    WtkWidget *page_title;
    WtkWidget *page_subtitle;
    WtkWidget *pages[4];
    WtkWidget *nav[4];
    NavData nav_data[4];
    int page;

    WtkWidget *dash_entry;
    WtkWidget *dash_greeting;
    WtkWidget *dash_check;
    WtkWidget *dash_slider;
    WtkWidget *dash_slider_label;
    WtkWidget *dash_progress;

    WtkWidget *signin_name;
    WtkWidget *signin_password;
    WtkWidget *signin_status;
    WtkWidget *opt_check1;
    WtkWidget *opt_check2;
    WtkWidget *opt_radio;
    WtkWidget *opt_status;
    WtkWidget *list;
    WtkWidget *list_status;
    int list_adds;
    WtkWidget *notes;
    WtkWidget *notes_status;

    WtkWidget *canvas;
    WtkWidget *canvas_mouse;
    HFONT chart_font;
    double chart[10];
    int chart_hover;
};

static const char *PAGE_TITLES[4] = {
    "总览",
    "控件",
    "画布",
    "关于 WinTk"
};

static const char *PAGE_SUBTITLES[4] = {
    "一个紧凑的 C API，托起完整的原生界面。",
    "原生控件、自绘外观与统一的事件模型。",
    "双缓冲 GDI 画布，支持鼠标、悬停与绘制回调。",
    "这个轻量框架背后的设计取舍。"
};

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static WtkWidget *new_label(WtkWidget *parent, const char *text, int size,
                            unsigned style, WtkColor color)
{
    WtkWidget *label = wtk_label(parent, text);
    wtk_widget_set_font(label, NULL, size, style);
    if (color != 0)
        wtk_widget_set_foreground(label, color);
    return label;
}

static void pack_top(WtkWidget *widget, WtkFill fill, int expand, int padx,
                     int pady)
{
    wtk_pack(widget, WTK_SIDE_TOP, fill, expand, padx, pady);
}

/* Count Unicode code points in a UTF-8 string (not bytes). */
static unsigned utf8_length(const char *text)
{
    unsigned count = 0;
    const unsigned char *p = (const unsigned char *)text;
    if (!p)
        return 0;
    while (*p) {
        if ((*p & 0xC0) != 0x80)
            count++;
        p++;
    }
    return count;
}


static WtkWidget *new_card(WtkWidget *parent, int padding)
{
    WtkWidget *card = wtk_frame(parent);
    WtkApp *app = wtk_widget_app(card);
    const WtkTheme *theme = wtk_app_get_theme(app);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 12);
    wtk_widget_set_padding(card, padding, padding, padding, padding);
    return card;
}

static WtkWidget *new_page(WtkWidget *content, GalleryState *state, int index)
{
    WtkWidget *page = wtk_frame(content);
    wtk_pack(page, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    state->pages[index] = page;
    return page;
}

static void add_card_heading(WtkWidget *card, GalleryState *state,
                             const char *title, const char *subtitle)
{
    WtkWidget *heading =
        new_label(card, title, 14, WTK_FONT_BOLD, state->theme->text);
    WtkWidget *caption =
        new_label(card, subtitle, 10, WTK_FONT_NORMAL, state->theme->muted);
    pack_top(heading, WTK_FILL_X, 0, 0, 0);
    pack_top(caption, WTK_FILL_X, 0, 0, 4);
}

/* ------------------------------------------------------------------------- */
/* Page switching                                                            */
/* ------------------------------------------------------------------------- */

static void switch_page(GalleryState *state, int index)
{
    int i;
    if (!state || index < 0 || index >= 4)
        return;
    state->page = index;

    for (i = 0; i < 4; ++i) {
        if (state->pages[i])
            wtk_widget_set_visible(state->pages[i], i == index);
    }
    for (i = 0; i < 4; ++i) {
        if (!state->nav[i])
            continue;
        wtk_button_set_style(state->nav[i],
                             i == index ? WTK_BUTTON_PRIMARY
                                        : WTK_BUTTON_GHOST);
        wtk_widget_set_foreground(state->nav[i],
                                  i == index ? state->theme->on_accent
                                             : WTK_RGB(64, 78, 100));
    }
    wtk_label_set_text(state->page_title, PAGE_TITLES[index]);
    wtk_label_set_text(state->page_subtitle, PAGE_SUBTITLES[index]);
}

static void on_nav_click(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    NavData *nav = (NavData *)userdata;
    (void)widget;
    (void)event;
    switch_page(nav->state, nav->index);
}

static void on_about_click(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    (void)widget;
    (void)event;
    switch_page((GalleryState *)userdata, 3);
}

/* ------------------------------------------------------------------------- */
/* Sidebar and header                                                        */
/* ------------------------------------------------------------------------- */

static void build_sidebar(GalleryState *state, WtkWidget *window)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *sidebar = wtk_frame(window);
    WtkWidget *brand;
    WtkWidget *tagline;
    WtkWidget *section;
    WtkWidget *spacer;
    WtkWidget *footer;
    WtkWidget *hint;
    int i;
    static const char *names[4] = {
        "总览", "控件", "画布", "关于"
    };

    wtk_widget_set_size(sidebar, 232, 0);
    wtk_widget_set_background(sidebar, WTK_RGB(240, 244, 251));
    wtk_widget_set_padding(sidebar, 20, 24, 20, 20);
    wtk_pack(sidebar, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 0);

    brand = new_label(sidebar, "WinTk", 21, WTK_FONT_BOLD, theme->text);
    pack_top(brand, WTK_FILL_X, 0, 0, 0);
    tagline = new_label(sidebar, "C / Win32 原生工具包", 9, WTK_FONT_NORMAL,
                        theme->muted);
    pack_top(tagline, WTK_FILL_X, 0, 0, 2);

    section = new_label(sidebar, "工作区", 8, WTK_FONT_BOLD, theme->muted);
    pack_top(section, WTK_FILL_X, 0, 2, 30);

    for (i = 0; i < 4; ++i) {
        WtkWidget *button = wtk_button(sidebar, names[i]);
        wtk_button_set_align(button, WTK_ALIGN_LEFT);
        wtk_button_set_style(button, i == 0 ? WTK_BUTTON_PRIMARY
                                            : WTK_BUTTON_GHOST);
        wtk_widget_set_foreground(button, i == 0 ? theme->on_accent
                                                 : WTK_RGB(64, 78, 100));
        wtk_widget_set_font(button, NULL, 10, WTK_FONT_BOLD);
        wtk_widget_set_padding(button, 14, 8, 14, 8);
        wtk_pack(button, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 3);
        state->nav[i] = button;
        state->nav_data[i].state = state;
        state->nav_data[i].index = i;
        wtk_bind(button, WTK_EVENT_CLICK, on_nav_click,
                 &state->nav_data[i]);
    }

    spacer = wtk_frame(sidebar);
    pack_top(spacer, WTK_FILL_BOTH, 1, 0, 0);

    hint = new_label(sidebar, "Tab 切换焦点\nEnter 激活按钮", 9,
                     WTK_FONT_NORMAL, theme->muted);
    pack_top(hint, WTK_FILL_X, 0, 2, 10);

    footer = new_label(sidebar, "WinTk 1.0  /  原生 C11", 8,
                       WTK_FONT_NORMAL, theme->muted);
    pack_top(footer, WTK_FILL_X, 0, 2, 0);
}

static void build_header(GalleryState *state, WtkWidget *main)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *header = wtk_frame(main);
    WtkWidget *actions = wtk_frame(header);
    WtkWidget *about = wtk_button(actions, "关于");
    WtkWidget *titles = wtk_frame(header);

    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 16);
    wtk_pack(actions, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);
    wtk_button_set_style(about, WTK_BUTTON_SECONDARY);
    wtk_pack(about, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(about, WTK_EVENT_CLICK, on_about_click, state);

    wtk_pack(titles, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    state->page_title =
        new_label(titles, "总览", 20, WTK_FONT_BOLD, theme->text);
    pack_top(state->page_title, WTK_FILL_X, 0, 0, 0);
    state->page_subtitle =
        new_label(titles, PAGE_SUBTITLES[0], 10, WTK_FONT_NORMAL,
                  theme->muted);
    pack_top(state->page_subtitle, WTK_FILL_X, 0, 0, 2);
}

/* ------------------------------------------------------------------------- */
/* Dashboard                                                                 */
/* ------------------------------------------------------------------------- */

static void on_dash_entry_change(WtkWidget *widget, WtkEvent *event,
                                 void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    const char *text = wtk_entry_get_text(state->dash_entry);
    char message[256];
    (void)widget;
    (void)event;
    if (!text || !text[0])
        snprintf(message, sizeof(message),
                 "实时预览：输入名字后，这里会显示问候。");
    else
        snprintf(message, sizeof(message), "实时预览：你好，%s！", text);
    wtk_label_set_text(state->dash_greeting, message);
}

static void on_dash_greet(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    const char *text = wtk_entry_get_text(state->dash_entry);
    char message[512];
    (void)widget;
    (void)event;
    if (!text || !text[0])
        text = "朋友";
    snprintf(message, sizeof(message),
             "你好，%s！\n\n这个对话框是一个原生 Win32 消息框，"
             "由 WinTk 窗口拥有。",
             text);
    wtk_message_box(state->window, "WinTk 向你问好", message, WTK_ICON_INFO);
}

static void on_dash_check(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    (void)widget;
    (void)event;
    wtk_progress_set_show_percent(state->dash_progress,
                                  wtk_checkbox_get(state->dash_check));
}

static void on_dash_slider(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    char label[64];
    int value;
    (void)widget;
    value = event ? event->value : wtk_slider_get(state->dash_slider);
    snprintf(label, sizeof(label), "音量：%d%%", value);
    wtk_label_set_text(state->dash_slider_label, label);
    wtk_progress_set(state->dash_progress, value / 100.0);
}

static WtkWidget *stat_card(WtkWidget *parent, const char *value,
                            const char *caption, WtkColor accent)
{
    WtkApp *app = wtk_widget_app(parent);
    const WtkTheme *theme = wtk_app_get_theme(app);
    WtkWidget *card = new_card(parent, 16);
    WtkWidget *number = new_label(card, value, 18, WTK_FONT_BOLD, accent);
    WtkWidget *text = new_label(card, caption, 9, WTK_FONT_NORMAL,
                                theme->muted);
    pack_top(number, WTK_FILL_X, 0, 0, 0);
    pack_top(text, WTK_FILL_X, 0, 0, 1);
    return card;
}

static void build_dashboard(GalleryState *state, WtkWidget *page)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *hero;
    WtkWidget *eyebrow;
    WtkWidget *headline;
    WtkWidget *description;
    WtkWidget *actions;
    WtkWidget *explore;
    WtkWidget *hello;
    WtkWidget *stats;
    WtkWidget *stat;
    WtkWidget *lower;
    WtkWidget *form;
    WtkWidget *native;
    WtkWidget *entry;
    WtkWidget *greeting;
    WtkWidget *checkbox;
    WtkWidget *button;
    WtkWidget *value_label;
    WtkWidget *slider;
    WtkWidget *progress;

    hero = new_card(page, 24);
    pack_top(hero, WTK_FILL_X, 0, 0, 0);

    eyebrow = new_label(hero, "原生 C / WINDOWS", 8, WTK_FONT_BOLD,
                        theme->accent);
    pack_top(eyebrow, WTK_FILL_X, 0, 0, 0);

    headline = new_label(hero, "不必离开 C，也能构建桌面 UI。",
                         24, WTK_FONT_BOLD, theme->text);
    pack_top(headline, WTK_FILL_X, 0, 0, 2);

    description = new_label(
        hero,
        "Pack / Grid / Place 布局、DPI 感知的 GDI 绘制，以及原生输入控件和紧凑的回调 API。",
        11, WTK_FONT_NORMAL, theme->muted);
    pack_top(description, WTK_FILL_X, 0, 0, 4);

    actions = wtk_frame(hero);
    pack_top(actions, WTK_FILL_X, 0, 0, 12);
    explore = wtk_button(actions, "浏览控件");
    wtk_button_set_style(explore, WTK_BUTTON_PRIMARY);
    wtk_widget_set_font(explore, NULL, 10, WTK_FONT_BOLD);
    wtk_pack(explore, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 2);
    wtk_bind(explore, WTK_EVENT_CLICK, on_nav_click, &state->nav_data[1]);

    hello = wtk_button(actions, "打个招呼");
    wtk_button_set_style(hello, WTK_BUTTON_SECONDARY);
    wtk_widget_set_font(hello, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(hello, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 10, 2);
    wtk_bind(hello, WTK_EVENT_CLICK, on_dash_greet, state);

    stats = wtk_frame(page);
    wtk_pack(stats, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 10);
    wtk_grid_column_configure(stats, 0, 1, 0);
    wtk_grid_column_configure(stats, 1, 1, 0);
    wtk_grid_column_configure(stats, 2, 1, 0);

    stat = stat_card(stats, "12", "核心控件", theme->accent);
    wtk_grid(stat, 0, 0, 1, 1, WTK_STICKY_ALL, 4, 0);
    stat = stat_card(stats, "3", "布局管理器", theme->success);
    wtk_grid(stat, 0, 1, 1, 1, WTK_STICKY_ALL, 4, 0);
    stat = stat_card(stats, "0", "运行时依赖", theme->warning);
    wtk_grid(stat, 0, 2, 1, 1, WTK_STICKY_ALL, 4, 0);

    lower = wtk_frame(page);
    wtk_pack(lower, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 8);
    wtk_grid_column_configure(lower, 0, 1, 0);
    wtk_grid_column_configure(lower, 1, 1, 0);
    wtk_grid_row_configure(lower, 0, 1, 0);

    form = new_card(lower, 20);
    wtk_grid(form, 0, 0, 1, 1, WTK_STICKY_ALL, 5, 0);
    add_card_heading(form, state, "试试绑定系统",
                     "输入触发 WTK_EVENT_CHANGE；按 Enter 提交。");

    entry = wtk_entry(form);
    state->dash_entry = entry;
    wtk_entry_set_placeholder(entry, "请输入你的名字…");
    wtk_widget_set_size(entry, 0, 40);
    wtk_pack(entry, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);
    wtk_entry_set_text(entry, "张三");

    greeting = new_label(form, "实时预览：你好，张三！", 11,
                         WTK_FONT_BOLD, theme->accent);
    state->dash_greeting = greeting;
    pack_top(greeting, WTK_FILL_X, 0, 0, 0);

    checkbox = wtk_checkbox(form, "我喜欢小巧的 API", 1);
    wtk_pack(checkbox, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);

    button = wtk_button(form, "发送问候");
    wtk_button_set_style(button, WTK_BUTTON_PRIMARY);
    wtk_widget_set_font(button, NULL, 10, WTK_FONT_BOLD);
    wtk_pack(button, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 0, 12);

    wtk_bind(entry, WTK_EVENT_CHANGE, on_dash_entry_change, state);
    wtk_bind(entry, WTK_EVENT_SUBMIT, on_dash_greet, state);
    wtk_bind(button, WTK_EVENT_CLICK, on_dash_greet, state);

    native = new_card(lower, 20);
    wtk_grid(native, 0, 1, 1, 1, WTK_STICKY_ALL, 5, 0);
    add_card_heading(native, state, "原生状态",
                     "自绘滑块驱动下方进度条。");

    value_label = new_label(native, "音量：35%", 11, WTK_FONT_BOLD,
                            theme->text);
    state->dash_slider_label = value_label;
    pack_top(value_label, WTK_FILL_X, 0, 0, 14);

    slider = wtk_slider(native, 0, 100, 35);
    state->dash_slider = slider;
    wtk_pack(slider, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);

    progress = wtk_progress(native);
    state->dash_progress = progress;
    wtk_progress_set(progress, 0.35);
    wtk_progress_set_show_percent(progress, 1);
    wtk_pack(progress, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 10);

    checkbox = wtk_checkbox(native, "在进度条下方显示百分比", 1);
    state->dash_check = checkbox;
    wtk_pack(checkbox, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);
    wtk_bind(checkbox, WTK_EVENT_CHANGE, on_dash_check, state);
    wtk_bind(slider, WTK_EVENT_CHANGE, on_dash_slider, state);
}

/* ------------------------------------------------------------------------- */
/* Widgets page                                                              */
/* ------------------------------------------------------------------------- */

static void update_options_status(GalleryState *state)
{
    char message[256];
    int radio = wtk_radiogroup_get(state->opt_radio);
    snprintf(message, sizeof(message),
             "通知：%s   测试：%s   密度：%s",
             wtk_checkbox_get(state->opt_check1) ? "开" : "关",
             wtk_checkbox_get(state->opt_check2) ? "开" : "关",
             radio == 0 ? "舒适" : (radio == 1 ? "紧凑" : "宽松"));
    wtk_label_set_text(state->opt_status, message);
}

static void on_option_change(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    (void)widget;
    (void)event;
    update_options_status((GalleryState *)userdata);
}

static void on_signin(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    const char *name = wtk_entry_get_text(state->signin_name);
    const char *password = wtk_entry_get_text(state->signin_password);
    char status[384];
    (void)widget;
    (void)event;
    if (!name || !name[0]) {
        wtk_widget_set_foreground(state->signin_status, state->theme->danger);
        wtk_label_set_text(state->signin_status, "请先输入名字再登录。");
        return;
    }
    wtk_widget_set_foreground(state->signin_status, state->theme->success);
    snprintf(status, sizeof(status), "已登录：%s（密码 %u 个字符）",
             name, utf8_length(password));
    wtk_label_set_text(state->signin_status, status);
}

static void on_clear_signin(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    (void)widget;
    (void)event;
    wtk_entry_set_text(state->signin_name, "");
    wtk_entry_set_text(state->signin_password, "");
    wtk_widget_set_foreground(state->signin_status, state->theme->muted);
    wtk_label_set_text(state->signin_status, "字段已清空。");
}

static void on_list_change(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    int index = wtk_listbox_get_selection(state->list);
    char status[256];
    (void)widget;
    (void)event;
    if (index < 0) {
        wtk_label_set_text(state->list_status, "没有选中项目。");
        return;
    }
    snprintf(status, sizeof(status), "已选择第 %d 项：%s", index,
             wtk_listbox_get_item(state->list, index));
    wtk_label_set_text(state->list_status, status);
}

static void on_list_add(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    char item[64];
    int index;
    (void)widget;
    (void)event;
    state->list_adds++;
    snprintf(item, sizeof(item), "新项目 %d", state->list_adds);
    index = wtk_listbox_add(state->list, item);
    if (index >= 0)
        wtk_listbox_select(state->list, index);
    on_list_change(state->list, NULL, state);
}

static void on_list_clear(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    (void)widget;
    (void)event;
    wtk_listbox_clear(state->list);
    wtk_label_set_text(state->list_status, "列表已清空。");
}

static void on_notes_submit(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    const char *value = wtk_text_get_text(state->notes);
    char status[256];
    (void)widget;
    (void)event;
    {
        unsigned lines = 1;
        const char *cursor = value;
        if (value) {
            while (*cursor) {
                if (*cursor == '\n')
                    lines++;
                cursor++;
            }
        }
        snprintf(status, sizeof(status), "草稿：%u 个字符，%u 行。",
                 utf8_length(value ? value : ""), lines);
    }
    wtk_widget_set_foreground(state->notes_status, state->theme->success);
    wtk_label_set_text(state->notes_status, status);
}

static void build_widgets_page(GalleryState *state, WtkWidget *page)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *cards[4];
    WtkWidget *card;
    WtkWidget *entry;
    WtkWidget *button;
    WtkWidget *row;
    WtkWidget *status;
    WtkWidget *check;
    WtkWidget *radio_group;
    WtkWidget *radio;
    WtkWidget *list;
    WtkWidget *text;
    int i;

    wtk_grid_column_configure(page, 0, 1, 0);
    wtk_grid_column_configure(page, 1, 1, 0);
    wtk_grid_row_configure(page, 0, 1, 0);
    wtk_grid_row_configure(page, 1, 1, 0);

    for (i = 0; i < 4; ++i) {
        cards[i] = new_card(page, 14);
        wtk_grid(cards[i], i / 2, i % 2, 1, 1, WTK_STICKY_ALL, 5, 4);
    }

    /* Inputs / events. */
    card = cards[0];
    add_card_heading(card, state, "文本输入",
                     "原生 EDIT 控件，套用自绘圆角宿主。");
    state->signin_name = wtk_entry(card);
    wtk_entry_set_placeholder(state->signin_name, "你的名字");
    wtk_pack(state->signin_name, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);
    state->signin_password = wtk_entry(card);
    wtk_entry_set_placeholder(state->signin_password, "密码");
    wtk_entry_set_password(state->signin_password, 1);
    wtk_pack(state->signin_password, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    row = wtk_frame(card);
    pack_top(row, WTK_FILL_X, 0, 0, 8);
    button = wtk_button(row, "登录");
    wtk_button_set_style(button, WTK_BUTTON_PRIMARY);
    wtk_pack(button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(button, WTK_EVENT_CLICK, on_signin, state);
    button = wtk_button(row, "清空");
    wtk_button_set_style(button, WTK_BUTTON_SECONDARY);
    wtk_pack(button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 8, 0);
    wtk_bind(button, WTK_EVENT_CLICK, on_clear_signin, state);

    state->signin_status = new_label(
        card, "这里不会真的校验账号，仅作演示。", 9,
        WTK_FONT_NORMAL, theme->muted);
    pack_top(state->signin_status, WTK_FILL_X, 0, 0, 10);

    /* Options. */
    card = cards[1];
    add_card_heading(card, state, "选择",
                     "复选框与单选框共享同一种 CHANGE 事件。");
    state->opt_check1 = wtk_checkbox(card, "启用通知", 1);
    wtk_pack(state->opt_check1, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);
    state->opt_check2 = wtk_checkbox(card, "加入测试通道", 0);
    wtk_pack(state->opt_check2, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 2);

    radio_group = wtk_radiogroup(card);
    state->opt_radio = radio_group;
    wtk_pack(radio_group, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 10);
    radio = wtk_radio(radio_group, "舒适密度");
    wtk_pack(radio, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 1);
    radio = wtk_radio(radio_group, "紧凑密度");
    wtk_pack(radio, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 1);
    radio = wtk_radio(radio_group, "宽松密度");
    wtk_pack(radio, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 1);

    wtk_bind(state->opt_check1, WTK_EVENT_CHANGE, on_option_change, state);
    wtk_bind(state->opt_check2, WTK_EVENT_CHANGE, on_option_change, state);
    wtk_bind(radio_group, WTK_EVENT_CHANGE, on_option_change, state);

    state->opt_status = new_label(card, "", 9, WTK_FONT_NORMAL,
                                  theme->muted);
    pack_top(state->opt_status, WTK_FILL_X, 0, 0, 10);
    update_options_status(state);

    /* Listbox. */
    card = cards[2];
    add_card_heading(card, state, "列表框",
                     "原生列表控件，使用 WinTk 焦点样式。");
    list = wtk_listbox(card);
    state->list = list;
    wtk_listbox_add(list, "北京");
    wtk_listbox_add(list, "上海");
    wtk_listbox_add(list, "广州");
    wtk_listbox_add(list, "深圳");
    wtk_listbox_add(list, "杭州");
    wtk_listbox_add(list, "成都");
    wtk_listbox_select(list, 0);
    wtk_widget_set_size(list, 0, 100);
    wtk_pack(list, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 12);
    wtk_bind(list, WTK_EVENT_CHANGE, on_list_change, state);

    row = wtk_frame(card);
    pack_top(row, WTK_FILL_X, 0, 0, 8);
    button = wtk_button(row, "添加项目");
    wtk_button_set_style(button, WTK_BUTTON_PRIMARY);
    wtk_pack(button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(button, WTK_EVENT_CLICK, on_list_add, state);
    button = wtk_button(row, "清空");
    wtk_button_set_style(button, WTK_BUTTON_SECONDARY);
    wtk_pack(button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 8, 0);
    wtk_bind(button, WTK_EVENT_CLICK, on_list_clear, state);
    state->list_status = new_label(card, "已选择第 0 项：北京", 9,
                                   WTK_FONT_NORMAL, theme->muted);
    pack_top(state->list_status, WTK_FILL_X, 0, 0, 8);

    /* Multiline editor. */
    card = cards[3];
    add_card_heading(card, state, "多行文本",
                     "Ctrl+Enter 提交，子 EDIT 负责原生滚动。");
    text = wtk_text(card);
    state->notes = text;
    wtk_text_set_placeholder(text,
                             "在这里写点什么…\nCtrl+Enter 提交。");
    wtk_widget_set_size(text, 0, 115);
    wtk_pack(text, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 12);
    row = wtk_frame(card);
    pack_top(row, WTK_FILL_X, 0, 0, 8);
    button = wtk_button(row, "统计字符");
    wtk_button_set_style(button, WTK_BUTTON_PRIMARY);
    wtk_pack(button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(button, WTK_EVENT_CLICK, on_notes_submit, state);
    wtk_bind(text, WTK_EVENT_SUBMIT, on_notes_submit, state);
    state->notes_status = new_label(card, "尚未提交。", 9,
                                    WTK_FONT_NORMAL, theme->muted);
    pack_top(state->notes_status, WTK_FILL_X, 0, 0, 8);

    (void)entry;
    (void)status;
    (void)check;
}

/* ------------------------------------------------------------------------- */
/* Canvas page                                                               */
/* ------------------------------------------------------------------------- */

static void chart_draw(WtkWidget *canvas, HDC dc, int width, int height,
                       void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    int scale = wtk_app_dpi(state->app);
    int margin_left = MulDiv(44, scale, 96);
    int margin_right = MulDiv(20, scale, 96);
    int margin_top = MulDiv(24, scale, 96);
    int margin_bottom = MulDiv(34, scale, 96);
    int plot_w;
    int plot_h;
    int i;
    HPEN grid_pen;
    HPEN accent_pen;
    HPEN point_pen;
    HBRUSH area_brush;
    HBRUSH point_brush;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_font;
    POINT area[12];
    POINT line[10];
    WCHAR value_text[16];
    RECT label_rect;
    double max_value = 100.0;

    (void)canvas;
    if (!state || !state->chart_font || width <= 0 || height <= 0)
        return;

    plot_w = width - margin_left - margin_right;
    plot_h = height - margin_top - margin_bottom;
    if (plot_w < 40 || plot_h < 30)
        return;

    SetBkMode(dc, TRANSPARENT);
    old_font = SelectObject(dc, state->chart_font);
    SetTextColor(dc, RGB(112, 123, 140));

    /* Area under the line. */
    area[0].x = margin_left;
    area[0].y = margin_top + plot_h;
    for (i = 0; i < 10; ++i) {
        line[i].x = margin_left + plot_w * i / 9;
        line[i].y = margin_top + plot_h -
                    (LONG)(plot_h * state->chart[i] / max_value);
        area[i + 1] = line[i];
    }
    area[11].x = margin_left + plot_w;
    area[11].y = margin_top + plot_h;

    area_brush = CreateSolidBrush(RGB(235, 241, 255));
    old_brush = SelectObject(dc, area_brush);
    {
        HPEN area_pen = CreatePen(PS_SOLID, 1, RGB(235, 241, 255));
        HGDIOBJ old_area_pen = SelectObject(dc, area_pen);
        Polygon(dc, area, 12);
        SelectObject(dc, old_area_pen);
        DeleteObject(area_pen);
    }
    SelectObject(dc, old_brush);
    DeleteObject(area_brush);

    /* Grid over the area. */
    grid_pen = CreatePen(PS_SOLID, 1, RGB(223, 230, 242));
    old_pen = SelectObject(dc, grid_pen);
    for (i = 0; i <= 4; ++i) {
        int y = margin_top + plot_h * i / 4;
        MoveToEx(dc, margin_left, y, NULL);
        LineTo(dc, margin_left + plot_w, y);
        swprintf(value_text, sizeof(value_text) / sizeof(value_text[0]),
                 L"%d", (int)(max_value - max_value * i / 4.0));
        label_rect.left = 4;
        label_rect.right = margin_left - 8;
        label_rect.top = y - MulDiv(8, scale, 96);
        label_rect.bottom = y + MulDiv(10, scale, 96);
        DrawTextW(dc, value_text, -1, &label_rect,
                  DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    SelectObject(dc, old_pen);
    DeleteObject(grid_pen);

    accent_pen = CreatePen(PS_SOLID, MulDiv(3, scale, 96), RGB(45, 91, 255));
    old_pen = SelectObject(dc, accent_pen);
    Polyline(dc, line, 10);
    SelectObject(dc, old_pen);
    DeleteObject(accent_pen);

    point_pen = CreatePen(PS_SOLID, MulDiv(2, scale, 96), RGB(45, 91, 255));
    point_brush = CreateSolidBrush(RGB(255, 255, 255));
    old_pen = SelectObject(dc, point_pen);
    old_brush = SelectObject(dc, point_brush);
    for (i = 0; i < 10; ++i) {
        int r = (i == state->chart_hover) ? MulDiv(7, scale, 96)
                                          : MulDiv(4, scale, 96);
        int center_x = line[i].x;
        int center_y = line[i].y;
        if (i == state->chart_hover) {
            HBRUSH hover = CreateSolidBrush(RGB(45, 91, 255));
            HGDIOBJ old_hover = SelectObject(dc, hover);
            Ellipse(dc, center_x - r, center_y - r, center_x + r + 1,
                    center_y + r + 1);
            SelectObject(dc, old_hover);
            DeleteObject(hover);
            SetTextColor(dc, RGB(45, 91, 255));
            swprintf(value_text,
                     sizeof(value_text) / sizeof(value_text[0]), L"%d",
                     (int)(state->chart[i] + 0.5));
            label_rect.left = center_x - MulDiv(22, scale, 96);
            label_rect.right = center_x + MulDiv(22, scale, 96);
            label_rect.top = center_y - r - MulDiv(24, scale, 96);
            label_rect.bottom = center_y - r - MulDiv(2, scale, 96);
            DrawTextW(dc, value_text, -1, &label_rect,
                      DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(dc, RGB(112, 123, 140));
        } else {
            Ellipse(dc, center_x - r, center_y - r, center_x + r + 1,
                    center_y + r + 1);
        }
    }
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(point_pen);
    DeleteObject(point_brush);

    SetTextColor(dc, RGB(112, 123, 140));
    for (i = 0; i < 10; ++i) {
        static const WCHAR *days[10] = {
            L"周一", L"周二", L"周三", L"周四", L"周五",
            L"周六", L"周日", L"周一", L"周二", L"周三"
        };
        label_rect.left = line[i].x - MulDiv(20, scale, 96);
        label_rect.right = line[i].x + MulDiv(20, scale, 96);
        label_rect.top = margin_top + plot_h + MulDiv(8, scale, 96);
        label_rect.bottom = label_rect.top + MulDiv(18, scale, 96);
        DrawTextW(dc, days[i], -1, &label_rect,
                  DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    SelectObject(dc, old_font);
}

static void on_canvas_mouse(WtkWidget *canvas, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    char text[128];
    RECT rc;
    int index = -1;
    (void)canvas;

    if (event->type == WTK_EVENT_MOUSE_LEAVE) {
        wtk_label_set_text(state->canvas_mouse,
                           "将鼠标移到图表上查看数据点。");
        if (state->chart_hover != -1) {
            state->chart_hover = -1;
            wtk_canvas_redraw(state->canvas);
        }
        return;
    }
    if (event->type != WTK_EVENT_MOUSE_MOVE)
        return;

    GetClientRect(wtk_widget_hwnd(state->canvas), &rc);
    if (rc.right > 0) {
        int dpi = wtk_app_dpi(state->app);
        int margin_left = MulDiv(44, dpi, 96);
        int margin_right = MulDiv(20, dpi, 96);
        int plot_width = rc.right - margin_left - margin_right;
        if (plot_width > 0 && event->x >= margin_left) {
            index = (int)((event->x - margin_left) * 10 / plot_width);
            if (index < 0)
                index = 0;
            if (index > 9)
                index = 9;
        }
    }
    if (index >= 0)
        snprintf(text, sizeof(text), "坐标：%d, %d 像素   数值：%.0f",
                 event->x, event->y, state->chart[index]);
    else
        snprintf(text, sizeof(text), "坐标：%d, %d 像素",
                 event->x, event->y);
    wtk_label_set_text(state->canvas_mouse, text);

    if (index != state->chart_hover) {
        state->chart_hover = index;
        wtk_canvas_redraw(state->canvas);
    }
}

static void on_canvas_randomize(WtkWidget *widget, WtkEvent *event,
                                void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    int i;
    (void)widget;
    (void)event;
    for (i = 0; i < 10; ++i)
        state->chart[i] = 24.0 + (rand() % 72);
    state->chart_hover = -1;
    wtk_canvas_redraw(state->canvas);
    wtk_label_set_text(state->canvas_mouse, "已生成一组新数据。");
}

static void build_canvas_page(GalleryState *state, WtkWidget *page)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *card = new_card(page, 20);
    WtkWidget *header;
    WtkWidget *title;
    WtkWidget *action;
    WtkWidget *canvas;
    WtkWidget *footer;
    LOGFONTW lf;
    int i;

    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);

    header = wtk_frame(card);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);
    action = wtk_button(header, "随机化数据");
    wtk_button_set_style(action, WTK_BUTTON_PRIMARY);
    wtk_pack(action, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(action, WTK_EVENT_CLICK, on_canvas_randomize, state);

    title = new_label(header, "请求量趋势", 16, WTK_FONT_BOLD,
                      theme->text);
    wtk_pack(title, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);

    canvas = wtk_canvas(card, 780, 380);
    state->canvas = canvas;
    wtk_widget_set_background(canvas, theme->surface);
    wtk_pack(canvas, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 10);
    wtk_canvas_set_draw(canvas, chart_draw, state);
    wtk_bind(canvas, WTK_EVENT_MOUSE_MOVE, on_canvas_mouse, state);
    wtk_bind(canvas, WTK_EVENT_MOUSE_LEAVE, on_canvas_mouse, state);
    wtk_bind(canvas, WTK_EVENT_DOUBLE_CLICK, on_canvas_randomize, state);

    footer = new_label(
        card, "将鼠标移到图表上查看数据点，双击可随机化。",
        9, WTK_FONT_NORMAL, theme->muted);
    state->canvas_mouse = footer;
    pack_top(footer, WTK_FILL_X, 0, 0, 2);

    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(9, wtk_app_dpi(state->app), 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    MultiByteToWideChar(CP_UTF8, 0, wtk_app_ui_font_family(state->app), -1,
                        lf.lfFaceName, LF_FACESIZE);
    state->chart_font = CreateFontIndirectW(&lf);

    state->chart_hover = -1;
    for (i = 0; i < 10; ++i) {
        static const double initial[10] = {
            26, 42, 36, 58, 49, 72, 64, 86, 75, 93
        };
        state->chart[i] = initial[i];
    }
}

/* ------------------------------------------------------------------------- */
/* About page                                                                */
/* ------------------------------------------------------------------------- */

static void on_quit_click(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    GalleryState *state = (GalleryState *)userdata;
    (void)widget;
    (void)event;
    if (wtk_ask_yes_no(state->window, "退出 WinTk 演示",
                       "确定要关闭画廊并结束演示吗？"))
        wtk_window_close(state->window);
}

static void on_back_dashboard(WtkWidget *widget, WtkEvent *event,
                              void *userdata)
{
    (void)widget;
    (void)event;
    switch_page((GalleryState *)userdata, 0);
}

static void build_about_page(GalleryState *state, WtkWidget *page)
{
    const WtkTheme *theme = state->theme;
    WtkWidget *intro = new_card(page, 24);
    WtkWidget *eyebrow;
    WtkWidget *heading;
    WtkWidget *paragraph;
    WtkWidget *bullets;
    WtkWidget *code_card;
    WtkWidget *code;
    WtkWidget *actions;
    WtkWidget *back;
    WtkWidget *quit;

    wtk_pack(intro, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);
    eyebrow = new_label(intro, "项目说明", 8, WTK_FONT_BOLD,
                        theme->accent);
    pack_top(eyebrow, WTK_FILL_X, 0, 0, 0);
    heading = new_label(intro, "一个小巧、刻意的工具包。",
                        22, WTK_FONT_BOLD, theme->text);
    pack_top(heading, WTK_FILL_X, 0, 0, 2);
    paragraph = new_label(
        intro,
        "WinTk 保留了 Tkinter 最有用的形态——控件、命令回调和三种布局"
        "管理器——同时保持为一个小而可读的 Win32/C11 库。它是一个学习"
        "项目，也是一个起点，而不是大型 GUI 框架的替代品。",
        11, WTK_FONT_NORMAL, theme->muted);
    wtk_label_set_wrap_width(paragraph, 620);
    pack_top(paragraph, WTK_FILL_X, 0, 0, 6);
    bullets = new_label(
        intro,
        "· 自绘控件全部使用 GDI 双缓冲\n"
        "· 原生 EDIT/LISTBOX 控件嵌入圆角宿主\n"
        "· UTF-8 API，支持中文与输入法，DPI 感知\n"
        "· Pack/Grid/Place 布局、定时器、菜单、对话框与画布",
        10, WTK_FONT_NORMAL, theme->text);
    pack_top(bullets, WTK_FILL_X, 0, 0, 12);

    code_card = new_card(page, 18);
    wtk_widget_set_background(code_card, WTK_RGB(17, 26, 43));
    wtk_widget_set_border(code_card, WTK_RGB(30, 43, 67), 1, 12);
    wtk_pack(code_card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 10);
    {
        WtkWidget *code_header = wtk_frame(code_card);
        WtkWidget *filename = new_label(code_header, "example.c", 9,
                                        WTK_FONT_BOLD,
                                        WTK_RGB(126, 166, 255));
        WtkWidget *lang = new_label(code_header, "C11  /  无第三方依赖", 9,
                                    WTK_FONT_NORMAL, WTK_RGB(121, 139, 166));
        wtk_pack(code_header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);
        wtk_pack(filename, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
        wtk_pack(lang, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);
        code = new_label(
            code_card,
            "static void on_click(WtkWidget *w, WtkEvent *e, void *data) {\n"
            "    wtk_label_set_text((WtkWidget *)data, \"Clicked!\");\n"
            "}\n"
            "int main(void) {\n"
            "    WtkApp *app = wtk_app_new();\n"
            "    WtkWidget *win = wtk_window(app, \"Hello\", 720, 500);\n"
            "    WtkWidget *button = wtk_button(win, \"Click me\");\n"
            "    wtk_pack(button, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 24, 24);\n"
            "    wtk_bind(button, WTK_EVENT_CLICK, on_click, win);\n"
            "    wtk_run(app);\n"
            "    wtk_app_free(app);\n"
            "    return 0;\n"
            "}",
            9, WTK_FONT_NORMAL, WTK_RGB(213, 226, 247));
        wtk_widget_set_font(code, "Consolas", 9, WTK_FONT_NORMAL);
        pack_top(code, WTK_FILL_BOTH, 1, 0, 0);
    }

    actions = wtk_frame(page);
    pack_top(actions, WTK_FILL_X, 0, 0, 10);
    back = wtk_button(actions, "返回总览");
    wtk_button_set_style(back, WTK_BUTTON_PRIMARY);
    wtk_pack(back, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);
    wtk_bind(back, WTK_EVENT_CLICK, on_back_dashboard, state);
    quit = wtk_button(actions, "退出演示");
    wtk_button_set_style(quit, WTK_BUTTON_DANGER);
    wtk_pack(quit, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 8, 0);
    wtk_bind(quit, WTK_EVENT_CLICK, on_quit_click, state);
}

/* ------------------------------------------------------------------------- */
/* Assembly                                                                  */
/* ------------------------------------------------------------------------- */

static int run_gallery(void)
{
    GalleryState state;
    WtkApp *app;
    WtkWidget *window;
    WtkWidget *main;
    WtkWidget *content;
    int i;

    memset(&state, 0, sizeof(state));
    app = wtk_app_new();
    if (!app)
        return 1;
    state.app = app;
    state.theme = wtk_app_get_theme(app);

    window = wtk_window(app, "WinTk 中文界面演示 - 原生 C UI 工具包", 1180, 820);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    state.window = window;
    wtk_window_set_min_size(window, 1000, 820);

    build_sidebar(&state, window);

    main = wtk_frame(window);
    wtk_widget_set_padding(main, 28, 24, 28, 24);
    wtk_pack(main, WTK_SIDE_LEFT, WTK_FILL_BOTH, 1, 0, 0);

    build_header(&state, main);

    content = wtk_frame(main);
    wtk_pack(content, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);

    build_dashboard(&state, new_page(content, &state, 0));
    build_widgets_page(&state, new_page(content, &state, 1));
    build_canvas_page(&state, new_page(content, &state, 2));
    build_about_page(&state, new_page(content, &state, 3));

    for (i = 1; i < 4; ++i)
        wtk_widget_set_visible(state.pages[i], 0);
    switch_page(&state, 0);

    srand((unsigned)time(NULL));
    wtk_window_center(window);
    wtk_run(app);

    if (state.chart_font)
        DeleteObject(state.chart_font);
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
    return run_gallery();
}
