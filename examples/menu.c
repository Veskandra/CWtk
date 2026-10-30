/* WinTk QMenu-style menu example: build with `mingw32-make menu`. */
#include "wintk.h"

#include <stdio.h>

typedef struct MenuState {
    WtkWidget *window;
    WtkWidget *status;
    WtkMenuAction *auto_save;
    WtkMenuAction *theme_light;
    WtkMenuAction *theme_dark;
} MenuState;

static void set_status(MenuState *state, const char *text)
{
    wtk_label_set_text(state->status, text);
}

static void on_new(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)window;
    (void)action;
    set_status(state, "新建页面");
}

static void on_save(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)window;
    (void)action;
    set_status(state, "保存完成 · Ctrl+S");
}

static void on_auto_save(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)window;
    set_status(state, wtk_menu_action_is_checked(action)
                         ? "已开启自动保存"
                         : "已关闭自动保存");
}

static void on_theme(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)window;
    set_status(state, action == state->theme_dark ? "主题：深色"
                                                  : "主题：浅色");
}

static void on_recent(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    char text[256];
    (void)window;
    snprintf(text, sizeof(text), "打开最近文件：%s",
             wtk_menu_action_get_text(action));
    set_status(state, text);
}

static void on_about(WtkWidget *window, WtkMenuAction *action, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)action;
    wtk_message_box(window, "关于菜单示例",
                    "WinTk 的菜单封装提供菜单栏、子菜单、动作（QAction 风格）、\n"
                    "复选、单选、启用状态和快捷键提示。",
                    WTK_ICON_INFO);
    set_status(state, "已打开关于对话框");
}

static void on_exit(WtkWidget *window, WtkMenuAction *action, void *data)
{
    (void)action;
    (void)data;
    wtk_quit(wtk_widget_app(window));
}

static void on_ctrl_s(WtkWidget *window, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)window;
    set_status(state, "全局快捷键触发：保存完成 · Ctrl+S");
}

static void on_trigger_auto(WtkWidget *button, WtkEvent *event, void *data)
{
    MenuState *state = (MenuState *)data;
    (void)button;
    (void)event;
    wtk_menu_action_trigger(state->auto_save);
}

static int run_menu(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *card;
    WtkWidget *title;
    WtkWidget *hint;
    WtkWidget *trigger;
    WtkMenu *bar;
    WtkMenu *file_menu;
    WtkMenu *edit_menu;
    WtkMenu *recent_menu;
    WtkMenu *help_menu;
    WtkMenuAction *open_action;
    WtkMenuAction *save_action;
    WtkMenuAction *undo_action;
    WtkMenuAction *redo_action;
    WtkMenuAction *light;
    WtkMenuAction *dark;
    WtkMenuAction *recent_a;
    WtkMenuAction *recent_b;
    WtkMenuAction *about;
    WtkMenuAction *exit_action;
    const WtkTheme *theme;
    MenuState state;

    if (!app)
        return 1;
    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk 菜单示例", 760, 460);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 560, 360);

    card = wtk_frame(window);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 16);
    wtk_widget_set_padding(card, 28, 24, 28, 22);
    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 24, 24);

    title = wtk_label(card, "QMenu 风格菜单");
    wtk_widget_set_font(title, NULL, 23, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    hint = wtk_label(card,
                     "文件 / 编辑 / 帮助；动作支持复选、单选、禁用和快捷键提示。");
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_widget_set_font(hint, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    trigger = wtk_button(card, "程序触发“自动保存”动作");
    wtk_button_set_style(trigger, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(trigger, 260, 38);
    wtk_pack(trigger, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 0, 20);

    state.status = wtk_label(card, "从菜单栏选择一个命令。");
    wtk_widget_set_foreground(state.status, theme->accent);
    wtk_widget_set_font(state.status, NULL, 11, WTK_FONT_BOLD);
    wtk_widget_set_size(state.status, 0, 30);
    wtk_pack(state.status, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);

    state.window = window;

    bar = wtk_menubar(window);
    file_menu = wtk_menu_submenu(bar, "文件(&F)");
    wtk_menu_add_action(file_menu, "新建(&N)", on_new, &state);
    open_action = wtk_menu_add_action(file_menu, "打开(&O)...", on_new, &state);
    wtk_menu_action_set_enabled(open_action, 0);
    save_action = wtk_menu_add_action(file_menu, "保存(&S)", on_save, &state);
    wtk_menu_action_set_shortcut_hint(save_action, "Ctrl+S");
    wtk_menu_add_separator(file_menu);
    exit_action = wtk_menu_add_action(file_menu, "退出(&X)", on_exit, &state);
    (void)exit_action;

    edit_menu = wtk_menu_submenu(bar, "编辑(&E)");
    undo_action = wtk_menu_add_action(edit_menu, "撤销(&U)", on_new, &state);
    redo_action = wtk_menu_add_action(edit_menu, "重做(&R)", on_new, &state);
    wtk_menu_action_set_enabled(undo_action, 0);
    wtk_menu_action_set_enabled(redo_action, 0);
    wtk_menu_add_separator(edit_menu);
    state.auto_save = wtk_menu_add_check_action(
        edit_menu, "自动保存(&A)", 1, on_auto_save, &state);
    wtk_menu_action_set_shortcut_hint(state.auto_save, "Ctrl+Shift+S");
    wtk_menu_add_separator(edit_menu);
    recent_menu = wtk_menu_submenu(edit_menu, "最近文件");
    recent_a = wtk_menu_add_action(recent_menu, "dashboard.c", on_recent, &state);
    recent_b = wtk_menu_add_action(recent_menu, "form.c", on_recent, &state);
    (void)recent_a;
    (void)recent_b;

    light = wtk_menu_add_radio_action(edit_menu, "浅色主题", 1, on_theme, &state);
    dark = wtk_menu_add_radio_action(edit_menu, "深色主题", 0, on_theme, &state);
    state.theme_light = light;
    state.theme_dark = dark;
    wtk_menu_add_separator(edit_menu);
    wtk_menu_add_action(edit_menu, "设置(&P)...", on_new, &state);

    help_menu = wtk_menu_submenu(bar, "帮助(&H)");
    about = wtk_menu_add_action(help_menu, "关于(&A)...", on_about, &state);
    (void)about;

    wtk_app_add_shortcut(app, 'S', WTK_MOD_CONTROL, on_ctrl_s, &state);
    wtk_bind(trigger, WTK_EVENT_CLICK, on_trigger_auto, &state);

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
    return run_menu();
}
