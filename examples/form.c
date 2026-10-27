/* WinTk form example: build with `mingw32-make form`. */
#include "wintk.h"

#include <stdio.h>
#include <string.h>

typedef struct FormState {
    WtkWidget *window;
    WtkWidget *name;
    WtkWidget *email;
    WtkWidget *password;
    WtkWidget *remember;
    WtkWidget *status;
} FormState;

static void set_status(FormState *state, const char *text, WtkColor color)
{
    wtk_label_set_text(state->status, text);
    wtk_widget_set_foreground(state->status, color);
}

static void submit_form(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    FormState *state = (FormState *)userdata;
    const WtkTheme *theme = wtk_app_get_theme(wtk_widget_app(widget));
    const char *name = wtk_entry_get_text(state->name);
    const char *email = wtk_entry_get_text(state->email);
    const char *password = wtk_entry_get_text(state->password);
    char message[512];

    (void)event;
    if (!name || !name[0]) {
        set_status(state, "请先填写姓名。", theme->danger);
        wtk_widget_focus(state->name);
        return;
    }
    if (!email || strchr(email, '@') == NULL) {
        set_status(state, "邮箱地址需要包含 @。", theme->danger);
        wtk_widget_focus(state->email);
        return;
    }
    if (!password || strlen(password) < 6) {
        set_status(state, "密码至少需要 6 个字符。", theme->danger);
        wtk_widget_focus(state->password);
        return;
    }
    set_status(state, "表单验证通过，可以提交。", theme->success);
    snprintf(message, sizeof(message),
             "姓名：%s\n邮箱：%s\n记住登录：%s\n\n"
             "这个示例演示了输入校验、焦点、复选框和原生消息框。",
             name, email,
             wtk_checkbox_get(state->remember) ? "是" : "否");
    wtk_message_box(state->window, "表单已提交", message, WTK_ICON_INFO);
}

static void reset_form(WtkWidget *widget, WtkEvent *event, void *userdata)
{
    FormState *state = (FormState *)userdata;
    const WtkTheme *theme = wtk_app_get_theme(wtk_widget_app(widget));

    (void)event;
    if (!wtk_ask_yes_no(state->window, "清空表单",
                        "确定要清空所有输入吗？"))
        return;
    wtk_entry_set_text(state->name, "");
    wtk_entry_set_text(state->email, "");
    wtk_entry_set_text(state->password, "");
    wtk_checkbox_set(state->remember, 0);
    set_status(state, "表单已清空。", theme->muted);
    wtk_widget_focus(state->name);
}

static WtkWidget *add_field(WtkWidget *parent, const char *caption,
                            WtkWidget **field_out, const WtkTheme *theme)
{
    WtkWidget *row = wtk_frame(parent);
    WtkWidget *label = wtk_label(row, caption);
    WtkWidget *field = wtk_entry(row);

    /* A widget belongs to the parent passed to its constructor. Packing it
     * later does not reparent it; the entry must be created on the row. */
    wtk_widget_set_background(row, theme->surface);
    wtk_pack(row, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 10);

    wtk_widget_set_size(label, 82, 38);
    wtk_widget_set_foreground(label, theme->muted);
    wtk_widget_set_font(label, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(label, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 0);

    wtk_widget_set_size(field, 0, 38);
    wtk_pack(field, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    *field_out = field;
    return row;
}

static int run_form(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *card;
    WtkWidget *header;
    WtkWidget *title;
    WtkWidget *subtitle;
    WtkWidget *buttons;
    WtkWidget *submit;
    WtkWidget *reset;
    WtkWidget *remember;
    const WtkTheme *theme;
    FormState state;

    if (!app)
        return 1;
    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk 表单示例", 720, 600);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 580, 500);

    card = wtk_frame(window);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 16);
    wtk_widget_set_padding(card, 30, 26, 30, 24);
    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 28, 28);

    header = wtk_frame(card);
    wtk_widget_set_background(header, theme->surface);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    title = wtk_label(header, "创建账户");
    wtk_widget_set_font(title, NULL, 24, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    subtitle = wtk_label(header,
                         "一个完整的表单：校验、焦点、回车提交和清空确认。");
    wtk_widget_set_foreground(subtitle, theme->muted);
    wtk_widget_set_font(subtitle, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(subtitle, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 14);

    add_field(card, "姓名", &state.name, theme);
    wtk_entry_set_placeholder(state.name, "例如：张三");

    add_field(card, "邮箱", &state.email, theme);
    wtk_entry_set_placeholder(state.email, "name@example.com");

    add_field(card, "密码", &state.password, theme);
    wtk_entry_set_password(state.password, 1);
    wtk_entry_set_placeholder(state.password, "至少 6 个字符");

    remember = wtk_checkbox(card, "记住这次登录", 1);
    wtk_widget_set_font(remember, NULL, 10, WTK_FONT_NORMAL);
    wtk_pack(remember, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    buttons = wtk_frame(card);
    wtk_widget_set_background(buttons, theme->surface);
    wtk_pack(buttons, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 12);

    submit = wtk_button(buttons, "提交");
    wtk_button_set_style(submit, WTK_BUTTON_PRIMARY);
    wtk_widget_set_size(submit, 116, 38);
    wtk_pack(submit, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 8);

    reset = wtk_button(buttons, "清空");
    wtk_button_set_style(reset, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(reset, 92, 38);
    wtk_pack(reset, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 0, 0);

    state.status = wtk_label(card, "请填写上面的信息。");
    wtk_widget_set_foreground(state.status, theme->muted);
    wtk_widget_set_font(state.status, NULL, 9, WTK_FONT_NORMAL);
    wtk_pack(state.status, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 10);

    state.window = window;
    state.remember = remember;
    wtk_bind(submit, WTK_EVENT_CLICK, submit_form, &state);
    wtk_bind(reset, WTK_EVENT_CLICK, reset_form, &state);
    wtk_bind(state.name, WTK_EVENT_SUBMIT, submit_form, &state);
    wtk_bind(state.email, WTK_EVENT_SUBMIT, submit_form, &state);
    wtk_bind(state.password, WTK_EVENT_SUBMIT, submit_form, &state);

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
    return run_form();
}
