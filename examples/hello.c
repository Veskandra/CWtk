/* WinTk "hello" example: build with `mingw32-make hello`. */
#include "wintk.h"

#include <stdio.h>
#include <string.h>

typedef struct HelloState {
    WtkWidget *entry;
    WtkWidget *output;
} HelloState;

static void style_card(WtkWidget *widget, int padding)
{
    WtkApp *app = wtk_widget_app(widget);
    const WtkTheme *theme = wtk_app_get_theme(app);
    wtk_widget_set_background(widget, theme->surface);
    wtk_widget_set_border(widget, theme->border, 1, 14);
    wtk_widget_set_padding(widget, padding, padding, padding, padding);
}

static void greet(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    HelloState *state = (HelloState *)userdata;
    const char *name = wtk_entry_get_text(state->entry);
    char message[512];

    (void)widget;
    (void)event;
    if (!name || !name[0])
        name = "朋友";
    snprintf(message, sizeof(message), "你好，%s！这条文字由纯 C 绘制。",
             name);
    wtk_label_set_text(state->output, message);
    wtk_entry_select_all(state->entry);
}

static void menu_exit(WtkWidget *window, void *userdata)
{
    (void)window;
    wtk_quit((WtkApp *)userdata);
}

static void menu_about(WtkWidget *window, void *userdata)
{
    (void)userdata;
    wtk_message_box(window, "关于这个示例",
                    "WinTk\n一个受 Tk 启发的原生 C UI 工具包。\n\n"
                    "试试输入框、按钮和菜单栏。输入框支持中文输入法。",
                    WTK_ICON_INFO);
}

static int run_hello(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *card;
    WtkWidget *title;
    WtkWidget *subtitle;
    WtkWidget *entry;
    WtkWidget *button;
    WtkWidget *output;
    WtkWidget *spacer;
    WtkWidget *footer;
    const WtkTheme *theme;
    WtkMenu *bar;
    WtkMenu *file_menu;
    WtkMenu *help_menu;
    HelloState state;

    if (!app)
        return 1;

    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk 中文示例", 720, 500);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 560, 420);

    card = wtk_frame(window);
    style_card(card, 30);
    wtk_widget_set_border(card, theme->border, 1, 16);
    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 28, 28);

    title = wtk_label(card, "你好，WinTk");
    wtk_widget_set_font(title, NULL, 26, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    subtitle = wtk_label(card,
                         "用纯 C 写成的原生 Windows 界面。");
    wtk_widget_set_foreground(subtitle, theme->muted);
    wtk_widget_set_font(subtitle, NULL, 11, WTK_FONT_NORMAL);
    wtk_pack(subtitle, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    entry = wtk_entry(card);
    wtk_widget_set_size(entry, 0, 42);
    wtk_entry_set_placeholder(entry, "请输入你的名字…");
    wtk_pack(entry, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 18);

    button = wtk_button(card, "打个招呼");
    wtk_button_set_style(button, WTK_BUTTON_PRIMARY);
    wtk_widget_set_font(button, NULL, 10, WTK_FONT_BOLD);
    wtk_pack(button, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 0, 10);

    output = wtk_label(card, "问候会显示在这里。");
    wtk_widget_set_font(output, NULL, 12, WTK_FONT_NORMAL);
    wtk_widget_set_foreground(output, theme->text);
    wtk_pack(output, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 14);

    spacer = wtk_frame(card);
    wtk_pack(spacer, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);

    footer = wtk_label(card, "提示：在输入框中按 Enter 可以直接提交。");
    wtk_widget_set_foreground(footer, theme->muted);
    wtk_widget_set_font(footer, NULL, 9, WTK_FONT_NORMAL);
    wtk_pack(footer, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    bar = wtk_menubar(window);
    if (bar) {
        file_menu = wtk_menu_submenu(bar, "文件(&F)");
        wtk_menu_add_command(file_menu, "退出(&X)\tAlt+F4", menu_exit, app);
        help_menu = wtk_menu_submenu(bar, "帮助(&H)");
        wtk_menu_add_command(help_menu, "关于(&A)...", menu_about, NULL);
    }

    state.entry = entry;
    state.output = output;
    wtk_bind(button, WTK_EVENT_CLICK, greet, &state);
    wtk_bind(entry, WTK_EVENT_SUBMIT, greet, &state);

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
    return run_hello();
}
