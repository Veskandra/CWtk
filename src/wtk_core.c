#include "wtk_internal.h"

/* 模态窗口状态（wtk_window_set_modal / wtk__modal_release 使用） */
static WtkWidget *g_modal_window = NULL;
static HWND g_modal_disabled[32];
static int g_modal_disabled_count = 0;
void wtk__modal_release(void);

#ifndef WTK_CLASS_WINDOW
#define WTK_CLASS_WINDOW  L"WinTkTopLevel"
#define WTK_CLASS_MESSAGE L"WinTkMessage"
#endif

static int g_classes_registered = 0;
static int g_dpi_initialized = 0;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam);

/* ------------------------------------------------------------------------- */
/* Small utilities                                                           */
/* ------------------------------------------------------------------------- */

void *wtk__alloc_zero(size_t size)
{
    void *ptr = calloc(1, size);
    if (!ptr) {
        MessageBoxW(NULL, L"WinTk could not allocate memory.", L"WinTk",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
    return ptr;
}

void *wtk__realloc_array(void *ptr, size_t size)
{
    void *result = realloc(ptr, size);
    if (!result) {
        MessageBoxW(NULL, L"WinTk could not allocate memory.", L"WinTk",
                    MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
    return result;
}

WCHAR *wtk__u8_to_u16(const char *text)
{
    int length;
    WCHAR *result;

    if (!text)
        text = "";

    length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (length <= 0) {
        result = (WCHAR *)wtk__alloc_zero(sizeof(WCHAR));
        return result;
    }

    result = (WCHAR *)wtk__alloc_zero((size_t)length * sizeof(WCHAR));
    if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, result, length)) {
        result[0] = L'\0';
    }
    return result;
}

char *wtk__u16_to_u8(const WCHAR *text)
{
    int length;
    char *result;

    if (!text)
        text = L"";

    length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (length <= 0) {
        result = (char *)wtk__alloc_zero(1);
        return result;
    }

    result = (char *)wtk__alloc_zero((size_t)length);
    if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, result, length, NULL, NULL)) {
        result[0] = '\0';
    }
    return result;
}

COLORREF wtk__colorref(WtkColor color)
{
    return RGB((BYTE)((color >> 16) & 0xFF),
               (BYTE)((color >> 8) & 0xFF),
               (BYTE)(color & 0xFF));
}

WtkColor wtk__shade(WtkColor color, int percent)
{
    int r = (int)((color >> 16) & 0xFF);
    int g = (int)((color >> 8) & 0xFF);
    int b = (int)(color & 0xFF);
    int target;

    if (percent > 100)
        percent = 100;
    if (percent < -100)
        percent = -100;

    target = percent >= 0 ? 255 : 0;
    percent = percent >= 0 ? percent : -percent;

    r += (target - r) * percent / 100;
    g += (target - g) * percent / 100;
    b += (target - b) * percent / 100;

    return WTK_RGB(r, g, b);
}

int wtk__logical(WtkApp *app, int pixels)
{
    if (!app)
        return pixels;
    return MulDiv(pixels, app->dpi, 96);
}

int wtk__scale_widget(WtkWidget *widget, int pixels)
{
    return wtk__logical(widget ? widget->app : NULL, pixels);
}

int wtk_app_dpi(WtkApp *app)
{
    return app ? app->dpi : 96;
}

int wtk_scale(WtkApp *app, int logical_pixels)
{
    return wtk__logical(app, logical_pixels);
}

int wtk__is_field(WtkWidget *widget)
{
    return widget && (widget->type == WTK_TYPE_ENTRY ||
                      widget->type == WTK_TYPE_TEXT ||
                      widget->type == WTK_TYPE_LISTBOX);
}

int wtk__is_container(WtkWidget *widget)
{
    return widget && (widget->type == WTK_TYPE_WINDOW ||
                      widget->type == WTK_TYPE_FRAME ||
                      widget->type == WTK_TYPE_RADIOGROUP);
}

WtkWidget *wtk__toplevel(WtkWidget *widget)
{
    if (!widget)
        return NULL;
    while (widget->parent)
        widget = widget->parent;
    return widget;
}

int wtk__style_is_native(WtkWidget *widget)
{
    return widget && widget->app &&
           widget->app->style_resolved == WTK_STYLE_NATIVE;
}

int wtk__style_is_fusion(WtkWidget *widget)
{
    return widget && widget->app &&
           widget->app->style_resolved == WTK_STYLE_FUSION;
}

WtkWidgetKind wtk_widget_kind(WtkWidget *widget)
{
    if (!widget)
        return WTK_KIND_WINDOW;
    switch (widget->type) {
    case WTK_TYPE_FRAME:
        return WTK_KIND_FRAME;
    case WTK_TYPE_LABEL:
        return WTK_KIND_LABEL;
    case WTK_TYPE_BUTTON:
        return WTK_KIND_BUTTON;
    case WTK_TYPE_ENTRY:
        return WTK_KIND_ENTRY;
    case WTK_TYPE_TEXT:
        return WTK_KIND_TEXT;
    case WTK_TYPE_LISTBOX:
        return WTK_KIND_LISTBOX;
    case WTK_TYPE_CHECKBOX:
        return WTK_KIND_CHECKBOX;
    case WTK_TYPE_RADIOGROUP:
        return WTK_KIND_RADIOGROUP;
    case WTK_TYPE_RADIO:
        return WTK_KIND_RADIO;
    case WTK_TYPE_SLIDER:
        return WTK_KIND_SLIDER;
    case WTK_TYPE_PROGRESS:
        return WTK_KIND_PROGRESS;
    case WTK_TYPE_SEPARATOR:
        return WTK_KIND_SEPARATOR;
    case WTK_TYPE_CANVAS:
        return WTK_KIND_CANVAS;
    case WTK_TYPE_WINDOW:
    default:
        return WTK_KIND_WINDOW;
    }
}

void wtk__build_paint_state(WtkWidget *widget, WtkPaintState *state)
{
    WtkTheme *theme;
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    if (!widget || !widget->app)
        return;
    theme = &widget->app->theme;
    state->kind = wtk_widget_kind(widget);
    state->text = widget->text;
    state->font = widget->font;
    state->background = widget->bg_set
                            ? widget->bg
                            : (wtk__is_field(widget)
                                   ? wtk__field_bg(widget)
                                   : wtk__widget_bg(widget));
    state->foreground = widget->fg_set
                            ? widget->fg
                            : (wtk__is_field(widget)
                                   ? wtk__field_fg(widget)
                                   : theme->text);
    state->border = widget->border_set ? widget->border : theme->border;
    state->background_set = widget->bg_set;
    state->foreground_set = widget->fg_set;
    state->border_set = widget->border_set;
    state->border_width = widget->border_width;
    state->border_radius = widget->border_radius;
    state->pad_left = widget->pad_l;
    state->pad_top = widget->pad_t;
    state->pad_right = widget->pad_r;
    state->pad_bottom = widget->pad_b;
    state->width = widget->w;
    state->height = widget->h;
    state->enabled = widget->enabled;
    state->hover = widget->hover;
    state->pressed = widget->pressed;
    state->focused = widget->focused;
    state->checked = widget->checked;
    state->selected = wtk__radio_selected(widget);
    state->focus_ring = widget->focus_ring;
    state->show_percent = widget->show_percent;
    state->password = widget->password;
    state->readonly = widget->readonly;
    state->button_style = widget->button_style;
    state->progress = widget->progress;
    state->value = widget->value;
    state->value_min = widget->value_min;
    state->value_max = widget->value_max;
    if (state->kind == WTK_KIND_ENTRY || state->kind == WTK_KIND_TEXT ||
        state->kind == WTK_KIND_LISTBOX)
        state->text = NULL;
}

WtkColor wtk__system_color(int index)
{
    COLORREF color = GetSysColor(index);
    return WTK_RGB(GetRValue(color), GetGValue(color), GetBValue(color));
}

HTHEME wtk__theme_button(WtkWidget *widget)
{
    return widget && widget->app ? widget->app->theme_button : NULL;
}

HTHEME wtk__theme_edit(WtkWidget *widget)
{
    return widget && widget->app ? widget->app->theme_edit : NULL;
}

HTHEME wtk__theme_listbox(WtkWidget *widget)
{
    return widget && widget->app ? widget->app->theme_listbox : NULL;
}

HTHEME wtk__theme_trackbar(WtkWidget *widget)
{
    return widget && widget->app ? widget->app->theme_trackbar : NULL;
}

HTHEME wtk__theme_progress(WtkWidget *widget)
{
    return widget && widget->app ? widget->app->theme_progress : NULL;
}

WtkColor wtk__container_bg(WtkWidget *container)
{
    if (!container)
        return WTK_RGB(245, 246, 250);
    if (container->bg_set)
        return container->bg;
    if (!container->parent)
        return container->app->theme.window_bg;
    return wtk__container_bg(container->parent);
}

WtkColor wtk__widget_bg(WtkWidget *widget)
{
    if (!widget)
        return WTK_RGB(245, 246, 250);
    if (widget->bg_set)
        return widget->bg;
    if (!widget->parent)
        return widget->app->theme.window_bg;
    return wtk__container_bg(widget->parent);
}

WtkColor wtk__field_bg(WtkWidget *widget)
{
    if (!widget)
        return WTK_WHITE;
    if (widget->bg_set)
        return widget->bg;
    if (!widget->enabled || widget->readonly)
        return widget->app->theme.surface_alt;
    return widget->app->theme.surface;
}

WtkColor wtk__field_fg(WtkWidget *widget)
{
    if (!widget)
        return WTK_BLACK;
    if (widget->fg_set)
        return widget->fg;
    if (!widget->enabled)
        return widget->app->theme.muted;
    return widget->app->theme.text;
}

void wtk_theme_default(WtkTheme *theme)
{
    if (!theme)
        return;

    memset(theme, 0, sizeof(*theme));
    theme->window_bg = WTK_RGB(244, 246, 250);
    theme->surface = WTK_RGB(255, 255, 255);
    theme->surface_alt = WTK_RGB(237, 241, 247);
    theme->text = WTK_RGB(23, 32, 51);
    theme->muted = WTK_RGB(108, 119, 137);
    theme->border = WTK_RGB(220, 226, 236);
    theme->accent = WTK_RGB(45, 91, 255);
    theme->accent_hover = WTK_RGB(75, 118, 255);
    theme->accent_pressed = WTK_RGB(31, 73, 209);
    theme->on_accent = WTK_RGB(255, 255, 255);
    theme->success = WTK_RGB(22, 163, 74);
    theme->warning = WTK_RGB(217, 119, 6);
    theme->danger = WTK_RGB(220, 61, 67);
    theme->corner_radius = 10;
    theme->control_height = 40;
}

void wtk_theme_fusion(WtkTheme *theme)
{
    if (!theme)
        return;

    memset(theme, 0, sizeof(*theme));
    theme->window_bg = WTK_RGB(239, 239, 239);
    theme->surface = WTK_RGB(255, 255, 255);
    theme->surface_alt = WTK_RGB(247, 247, 247);
    theme->text = WTK_RGB(26, 26, 26);
    theme->muted = WTK_RGB(112, 112, 112);
    theme->border = WTK_RGB(189, 189, 189);
    theme->accent = WTK_RGB(48, 140, 198);
    theme->accent_hover = WTK_RGB(64, 156, 214);
    theme->accent_pressed = WTK_RGB(37, 116, 167);
    theme->on_accent = WTK_RGB(255, 255, 255);
    theme->success = WTK_RGB(46, 139, 87);
    theme->warning = WTK_RGB(214, 144, 0);
    theme->danger = WTK_RGB(192, 57, 43);
    theme->corner_radius = 3;
    theme->control_height = 30;
}

void wtk_theme_native(WtkTheme *theme)
{
    WtkColor accent;
    if (!theme)
        return;

    memset(theme, 0, sizeof(*theme));
    accent = wtk__system_color(COLOR_HIGHLIGHT);
    theme->window_bg = wtk__system_color(COLOR_3DFACE);
    theme->surface = wtk__system_color(COLOR_WINDOW);
    theme->surface_alt = wtk__system_color(COLOR_3DFACE);
    theme->text = wtk__system_color(COLOR_WINDOWTEXT);
    theme->muted = wtk__system_color(COLOR_GRAYTEXT);
    theme->border = wtk__system_color(COLOR_3DSHADOW);
    theme->accent = accent;
    theme->accent_hover = wtk__shade(accent, 8);
    theme->accent_pressed = wtk__shade(accent, -10);
    theme->on_accent = wtk__system_color(COLOR_HIGHLIGHTTEXT);
    theme->success = WTK_RGB(0, 128, 0);
    theme->warning = WTK_RGB(191, 98, 0);
    theme->danger = WTK_RGB(192, 0, 0);
    theme->corner_radius = 0;
    theme->control_height = 28;
}

/* ------------------------------------------------------------------------- */
/* Graveyard / dispatch depth                                                */
/* ------------------------------------------------------------------------- */

static void post_sweep(WtkApp *app)
{
    if (!app || !app->message_hwnd || app->shutting_down || app->sweep_posted)
        return;
    if (app->dispatch_depth > 0)
        return;
    if (PostMessageW(app->message_hwnd, WTK_WM_SWEEP, 0, 0))
        app->sweep_posted = 1;
}

void wtk__begin_dispatch(WtkApp *app)
{
    if (app)
        app->dispatch_depth++;
}

void wtk__end_dispatch(WtkApp *app)
{
    if (!app)
        return;
    if (app->dispatch_depth > 0)
        app->dispatch_depth--;
    if (app->dispatch_depth == 0 && app->grave_count > 0)
        post_sweep(app);
}

void wtk__queue_graveyard(WtkWidget *widget)
{
    WtkApp *app;
    if (!widget || !widget->app)
        return;
    app = widget->app;

    if (app->grave_count >= app->grave_cap) {
        int new_cap = app->grave_cap ? app->grave_cap * 2 : 8;
        app->graveyard = (WtkWidget **)wtk__realloc_array(
            app->graveyard, (size_t)new_cap * sizeof(WtkWidget *));
        app->grave_cap = new_cap;
    }
    app->graveyard[app->grave_count++] = widget;
    post_sweep(app);
}

void wtk__sweep_graveyard(WtkApp *app)
{
    int i;
    if (!app)
        return;
    for (i = 0; i < app->grave_count; ++i)
        wtk__free_widget_tree(app->graveyard[i]);
    app->grave_count = 0;
}

/* ------------------------------------------------------------------------- */
/* Window classes                                                            */
/* ------------------------------------------------------------------------- */

static LRESULT CALLBACK message_proc(HWND hwnd, UINT message, WPARAM wparam,
                                     LPARAM lparam)
{
    WtkApp *app = (WtkApp *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
        app = (WtkApp *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        return TRUE;
    }

    if (!app)
        return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case WM_TIMER: {
        unsigned id = (unsigned)wparam;
        int i;
        WtkTimer timer;
        int found = 0;
        for (i = 0; i < app->timer_count; ++i) {
            if (app->timers[i].id == id) {
                timer = app->timers[i];
                memmove(&app->timers[i], &app->timers[i + 1],
                        (size_t)(app->timer_count - i - 1) * sizeof(WtkTimer));
                app->timer_count--;
                found = 1;
                break;
            }
        }
        if (found) {
            KillTimer(app->message_hwnd, (UINT_PTR)id);
            wtk__begin_dispatch(app);
            if (timer.callback)
                timer.callback(app, timer.userdata);
            wtk__end_dispatch(app);
        }
        return 0;
    }

    case WTK_WM_SWEEP:
        app->sweep_posted = 0;
        if (app->dispatch_depth == 0)
            wtk__sweep_graveyard(app);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void wtk__register_window_class(HINSTANCE instance)
{
    WNDCLASSEXW wc;

    if (g_classes_registered)
        return;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = message_proc;
    wc.hInstance = instance;
    wc.lpszClassName = WTK_CLASS_MESSAGE;
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, L"WINTK_ICON");
    if (!wc.hIcon)
        wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = WTK_CLASS_WINDOW;
    RegisterClassExW(&wc);

    wtk__register_widget_class(instance);
    g_classes_registered = 1;
}

/* ------------------------------------------------------------------------- */
/* Application                                                               */
/* ------------------------------------------------------------------------- */

static void initialize_process_dpi(void)
{
    if (!g_dpi_initialized) {
        SetProcessDPIAware();
        g_dpi_initialized = 1;
    }
}

static int CALLBACK font_available_cb(const LOGFONTW *logfont,
                                      const TEXTMETRICW *metrics,
                                      DWORD font_type, LPARAM lparam)
{
    (void)logfont;
    (void)metrics;
    (void)font_type;
    *(int *)lparam = 1;
    return 0; /* Stop after the first match. */
}

static int font_available(const WCHAR *family)
{
    HDC dc;
    LOGFONTW lf;
    int found = 0;

    if (!family || !family[0])
        return 0;
    dc = GetDC(NULL);
    if (!dc)
        return 0;

    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, family, LF_FACESIZE - 1);
    EnumFontFamiliesExW(dc, &lf, font_available_cb, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}

/* Pick a face that actually has CJK glyphs on the current Windows UI
 * language, instead of depending on GDI font linking. */
static const WCHAR *choose_ui_font_family(void)
{
    LANGID language = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(language);
    WORD sub = SUBLANGID(language);

    if (primary == LANG_CHINESE) {
        int traditional =
            (sub == SUBLANG_CHINESE_TRADITIONAL ||
             sub == SUBLANG_CHINESE_HONGKONG ||
             sub == SUBLANG_CHINESE_MACAU);
        const WCHAR *preferred =
            traditional ? L"Microsoft JhengHei UI" : L"Microsoft YaHei UI";
        const WCHAR *alternate =
            traditional ? L"Microsoft YaHei UI" : L"Microsoft JhengHei UI";

        if (font_available(preferred))
            return preferred;
        if (font_available(alternate))
            return alternate;
    } else if (primary == LANG_JAPANESE) {
        if (font_available(L"Yu Gothic UI"))
            return L"Yu Gothic UI";
    } else if (primary == LANG_KOREAN) {
        if (font_available(L"Malgun Gothic"))
            return L"Malgun Gothic";
    }

    return L"Segoe UI";
}

static WtkStyle style_from_environment(void)
{
    WCHAR value[32];
    DWORD length;
    int i;

    length = GetEnvironmentVariableW(L"WINTK_STYLE", value, 32);
    if (length == 0 || length >= 32)
        return WTK_STYLE_AUTO;
    for (i = 0; value[i]; ++i) {
        if (value[i] >= L'A' && value[i] <= L'Z')
            value[i] = (WCHAR)(value[i] - L'A' + L'a');
    }
    if (wcscmp(value, L"wintk") == 0 || wcscmp(value, L"custom") == 0 ||
        wcscmp(value, L"classic") == 0)
        return WTK_STYLE_WINTK;
    if (wcscmp(value, L"native") == 0 || wcscmp(value, L"system") == 0)
        return WTK_STYLE_NATIVE;
    if (wcscmp(value, L"fusion") == 0 || wcscmp(value, L"qt") == 0 ||
        wcscmp(value, L"qtfusion") == 0)
        return WTK_STYLE_FUSION;
    return WTK_STYLE_AUTO;
}

static WtkStyle resolve_style(WtkStyle requested)
{
    /* The core's actual drawing is the original WinTk GDI style. AUTO resolves
     * to that hint; Native/Fusion are hints for an installed painter. */
    if (requested == WTK_STYLE_AUTO)
        return WTK_STYLE_WINTK;
    return requested;
}

static void open_theme_handles(WtkApp *app)
{
    if (!app || !app->message_hwnd)
        return;
    app->theme_button = OpenThemeData(app->message_hwnd, L"BUTTON");
    app->theme_edit = OpenThemeData(app->message_hwnd, L"EDIT");
    app->theme_listbox = OpenThemeData(app->message_hwnd, L"LISTBOX");
    app->theme_trackbar = OpenThemeData(app->message_hwnd, L"TRACKBAR");
    app->theme_progress = OpenThemeData(app->message_hwnd, L"PROGRESS");
}

static void close_theme_handles(WtkApp *app)
{
    if (!app)
        return;
    if (app->theme_button)
        CloseThemeData(app->theme_button);
    if (app->theme_edit)
        CloseThemeData(app->theme_edit);
    if (app->theme_listbox)
        CloseThemeData(app->theme_listbox);
    if (app->theme_trackbar)
        CloseThemeData(app->theme_trackbar);
    if (app->theme_progress)
        CloseThemeData(app->theme_progress);
    app->theme_button = NULL;
    app->theme_edit = NULL;
    app->theme_listbox = NULL;
    app->theme_trackbar = NULL;
    app->theme_progress = NULL;
}

static void reset_style_metrics(WtkWidget *widget)
{
    int i;
    if (!widget)
        return;
    widget->style_margin = -1;
    for (i = 0; i < widget->child_count; ++i)
        reset_style_metrics(widget->children[i]);
}

static void refresh_style_tree(WtkWidget *widget)
{
    int i;
    if (!widget)
        return;
    if (wtk__is_field(widget)) {
        wtk__native_theme_changed(widget);
        wtk__native_position(widget);
    }
    if (widget->type == WTK_TYPE_BUTTON && !widget->cursor_set)
        widget->cursor = WTK_CURSOR_HAND;
    wtk__invalidate(widget);
    for (i = 0; i < widget->child_count; ++i)
        refresh_style_tree(widget->children[i]);
}

WtkApp *wtk_app_new(void)
{
    WtkApp *app;
    HDC screen;
    INITCOMMONCONTROLSEX controls;
    int dpi;

    initialize_process_dpi();

    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);

    dpi = 96;
    screen = GetDC(NULL);
    if (screen) {
        dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(NULL, screen);
    }
    if (dpi < 72 || dpi > 480)
        dpi = 96;

    app = (WtkApp *)wtk__alloc_zero(sizeof(*app));
    app->instance = GetModuleHandleW(NULL);
    app->dpi = dpi;
    app->next_menu_id = 1000;
    app->next_timer_id = 1;
    app->style_requested = style_from_environment();
    app->style_resolved = resolve_style(app->style_requested);
    app->theme_custom = 0;
    wtk_theme_default(&app->theme);
    wcsncpy(app->ui_font_family, choose_ui_font_family(),
            LF_FACESIZE - 1);
    app->ui_font_family[LF_FACESIZE - 1] = L'\0';
    app->ui_font_family_utf8 = wtk__u16_to_u8(app->ui_font_family);

    wtk__register_window_class(app->instance);
    {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        if (!GetClassInfoExW(app->instance, WTK_CLASS_WINDOW, &wc)) {
            free(app->ui_font_family_utf8);
            free(app);
            return NULL;
        }
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        if (!GetClassInfoExW(app->instance, WTK_CLASS_MESSAGE, &wc)) {
            free(app->ui_font_family_utf8);
            free(app);
            return NULL;
        }
    }

    app->message_hwnd = CreateWindowExW(
        0, WTK_CLASS_MESSAGE, L"WinTk message window", 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, app->instance, app);
    if (!app->message_hwnd) {
        free(app->ui_font_family_utf8);
        free(app);
        return NULL;
    }
    open_theme_handles(app);

    return app;
}

void wtk_app_free(WtkApp *app)
{
    int i;

    if (!app)
        return;

    app->shutting_down = 1;

    for (i = 0; i < app->timer_count; ++i)
        KillTimer(app->message_hwnd, (UINT_PTR)app->timers[i].id);
    free(app->timers);
    app->timers = NULL;
    app->timer_count = 0;

    close_theme_handles(app);
    if (app->message_hwnd) {
        DestroyWindow(app->message_hwnd);
        app->message_hwnd = NULL;
    }

    wtk__sweep_graveyard(app);

    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (window->hwnd && IsWindow(window->hwnd))
            DestroyWindow(window->hwnd);
    }

    for (i = 0; i < app->window_count; ++i)
        wtk__free_widget_tree(app->windows[i]);

    free(app->windows);
    free(app->shortcuts);
    free(app->graveyard);
    free(app->ui_font_family_utf8);
    free(app);
}

static void refresh_theme_tree(WtkWidget *widget)
{
    int i;
    if (!widget)
        return;
    wtk__native_theme_changed(widget);
    wtk__invalidate(widget);
    for (i = 0; i < widget->child_count; ++i)
        refresh_theme_tree(widget->children[i]);
}

void wtk_app_set_theme(WtkApp *app, const WtkTheme *theme)
{
    int i;
    if (!app || !theme)
        return;
    app->theme = *theme;
    app->theme_custom = 1;

    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (!window->hwnd)
            continue;
        refresh_theme_tree(window);
        RedrawWindow(window->hwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

const WtkTheme *wtk_app_get_theme(WtkApp *app)
{
    return app ? &app->theme : NULL;
}

void wtk_app_set_style_painter(WtkApp *app,
                               const WtkStylePainter *painter,
                               void *userdata)
{
    int i;
    if (!app)
        return;
    app->style_painter = painter;
    app->style_painter_data = userdata;
    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (!window->hwnd)
            continue;
        reset_style_metrics(window);
        refresh_style_tree(window);
        wtk__layout_window(window);
        RedrawWindow(window->hwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

const WtkStylePainter *wtk_app_get_style_painter(WtkApp *app)
{
    return app ? app->style_painter : NULL;
}

void *wtk_app_get_style_painter_data(WtkApp *app)
{
    return app ? app->style_painter_data : NULL;
}

void wtk_app_set_style(WtkApp *app, WtkStyle style)
{
    int i;
    if (!app)
        return;
    if (style != WTK_STYLE_AUTO && style != WTK_STYLE_NATIVE &&
        style != WTK_STYLE_WINTK && style != WTK_STYLE_FUSION)
        style = WTK_STYLE_AUTO;
    app->style_requested = style;
    app->style_resolved = resolve_style(style);

    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (!window->hwnd)
            continue;
        refresh_style_tree(window);
        wtk__layout_window(window);
        RedrawWindow(window->hwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

WtkStyle wtk_app_get_style(WtkApp *app)
{
    return app ? app->style_requested : WTK_STYLE_AUTO;
}

WtkStyle wtk_app_resolved_style(WtkApp *app)
{
    return app ? app->style_resolved : WTK_STYLE_WINTK;
}

const char *wtk_style_name(WtkStyle style)
{
    switch (style) {
    case WTK_STYLE_NATIVE:
        return "Native";
    case WTK_STYLE_WINTK:
        return "WinTk";
    case WTK_STYLE_FUSION:
        return "Fusion";
    case WTK_STYLE_AUTO:
    default:
        return "Auto";
    }
}

const char *wtk_app_ui_font_family(WtkApp *app)
{
    if (app && app->ui_font_family_utf8)
        return app->ui_font_family_utf8;
    return "Segoe UI";
}

void wtk__add_window(WtkApp *app, WtkWidget *window)
{
    if (app->window_count >= app->window_cap) {
        int new_cap = app->window_cap ? app->window_cap * 2 : 4;
        app->windows = (WtkWidget **)wtk__realloc_array(
            app->windows, (size_t)new_cap * sizeof(WtkWidget *));
        app->window_cap = new_cap;
    }
    app->windows[app->window_count++] = window;
}

WtkWidget *wtk__window_from_hwnd(WtkApp *app, HWND hwnd)
{
    int i;
    if (!app || !hwnd)
        return NULL;
    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (!window->hwnd)
            continue;
        if (window->hwnd == hwnd || IsChild(window->hwnd, hwnd))
            return window;
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Timers and dialogs                                                        */
/* ------------------------------------------------------------------------- */

unsigned wtk_after(WtkApp *app, unsigned delay_ms, WtkTimerFn callback,
                   void *userdata)
{
    unsigned id;
    int guard = 0;

    if (!app || !app->message_hwnd || !callback)
        return 0;
    if (delay_ms == 0)
        delay_ms = 1;

    do {
        id = app->next_timer_id++;
        if (app->next_timer_id == 0)
            app->next_timer_id = 1;
        guard++;
    } while (id == 0 && guard < 1000);

    if (!id)
        return 0;

    if (app->timer_count >= app->timer_cap) {
        int new_cap = app->timer_cap ? app->timer_cap * 2 : 8;
        app->timers = (WtkTimer *)wtk__realloc_array(
            app->timers, (size_t)new_cap * sizeof(WtkTimer));
        app->timer_cap = new_cap;
    }

    app->timers[app->timer_count].id = id;
    app->timers[app->timer_count].callback = callback;
    app->timers[app->timer_count].userdata = userdata;
    app->timer_count++;

    if (!SetTimer(app->message_hwnd, (UINT_PTR)id, delay_ms, NULL)) {
        app->timer_count--;
        return 0;
    }
    return id;
}

void wtk_cancel_timer(WtkApp *app, unsigned timer_id)
{
    int i;
    if (!app || !timer_id)
        return;
    for (i = 0; i < app->timer_count; ++i) {
        if (app->timers[i].id == timer_id) {
            KillTimer(app->message_hwnd, (UINT_PTR)timer_id);
            memmove(&app->timers[i], &app->timers[i + 1],
                    (size_t)(app->timer_count - i - 1) * sizeof(WtkTimer));
            app->timer_count--;
            return;
        }
    }
}

static UINT icon_to_flags(WtkIcon icon)
{
    switch (icon) {
    case WTK_ICON_WARNING:
        return MB_ICONWARNING;
    case WTK_ICON_ERROR:
        return MB_ICONERROR;
    case WTK_ICON_QUESTION:
        return MB_ICONQUESTION;
    case WTK_ICON_INFO:
        return MB_ICONINFORMATION;
    default:
        return 0;
    }
}

void wtk_message_box(WtkWidget *owner, const char *title, const char *message,
                     WtkIcon icon)
{
    WCHAR *wtitle = wtk__u8_to_u16(title);
    WCHAR *wmessage = wtk__u8_to_u16(message);
    WtkWidget *window = wtk__toplevel(owner);
    HWND hwnd = (window && window->hwnd) ? window->hwnd : NULL;

    MessageBoxW(hwnd, wmessage, wtitle, MB_OK | icon_to_flags(icon));
    free(wtitle);
    free(wmessage);
}

int wtk_ask_yes_no(WtkWidget *owner, const char *title, const char *message)
{
    WCHAR *wtitle = wtk__u8_to_u16(title);
    WCHAR *wmessage = wtk__u8_to_u16(message);
    WtkWidget *window = wtk__toplevel(owner);
    HWND hwnd = (window && window->hwnd) ? window->hwnd : NULL;
    int result = MessageBoxW(hwnd, wmessage, wtitle,
                             MB_YESNO | MB_ICONQUESTION);

    free(wtitle);
    free(wmessage);
    return result == IDYES;
}

/* ------------------------------------------------------------------------- */
/* Window                                                                    */
/* ------------------------------------------------------------------------- */

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam)
{
    WtkWidget *window =
        (WtkWidget *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
        window = (WtkWidget *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
        window->hwnd = hwnd;
        return TRUE;
    }

    if (!window)
        return DefWindowProcW(hwnd, message, wparam, lparam);

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        wtk__paint_window_background(window, dc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (!window->hwnd)
            return 0;
        {
            RECT rc;
            int width;
            int height;
            GetClientRect(hwnd, &rc);
            width = rc.right - rc.left;
            height = rc.bottom - rc.top;
            if (!window->app->in_layout) {
                wtk__layout_window(window);
            }
            if (window->hwnd) {
                WtkEvent event;
                memset(&event, 0, sizeof(event));
                event.type = WTK_EVENT_RESIZE;
                event.width = width;
                event.height = height;
                wtk__dispatch(window, &event);
            }
            RedrawWindow(hwnd, NULL, NULL,
                         RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lparam;
        if (window->min_w > 0)
            mmi->ptMinTrackSize.x = window->min_w;
        if (window->min_h > 0)
            mmi->ptMinTrackSize.y = window->min_h;
        return 0;
    }

    case WM_CLOSE: {
        WtkEvent event;
        memset(&event, 0, sizeof(event));
        event.type = WTK_EVENT_CLOSE;
        wtk__dispatch(window, &event);
        if (event.cancel)
            return 0;
        if (window->hwnd && IsWindow(window->hwnd))
            DestroyWindow(window->hwnd);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) >= 1000 && lparam == 0) {
            wtk__handle_menu_command(window, (UINT)LOWORD(wparam));
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_DESTROY:
        if (window->open_counted) {
            window->open_counted = 0;
            if (window->app->open_windows > 0)
                window->app->open_windows--;
        }
        if (window == g_modal_window)
            wtk__modal_release(); /* 模态窗口被关掉：自动恢复其它窗口 */
        window->shown = 0;
        window->hwnd = NULL;
        if (window->app->running && window->app->open_windows == 0 &&
            !window->app->shutting_down) {
            PostQuitMessage(0);
        }
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, message, wparam, lparam);

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

WtkWidget *wtk_window(WtkApp *app, const char *title, int width, int height)
{
    WtkWidget *window;
    WCHAR *wtitle;
    HWND hwnd;
    DWORD style;
    int physical_w;
    int physical_h;

    if (!app)
        return NULL;
    if (width <= 0)
        width = 960;
    if (height <= 0)
        height = 640;

    window = wtk__alloc_widget(app, NULL, WTK_TYPE_WINDOW);
    if (!window)
        return NULL;
    window->resizable = 1;
    window->visible = 1;
    window->min_w = wtk__logical(app, 320);
    window->min_h = wtk__logical(app, 240);
    window->app = app;

    physical_w = wtk__logical(app, width);
    physical_h = wtk__logical(app, height);
    wtitle = wtk__u8_to_u16(title ? title : "");
    style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    /* WS_EX_COMPOSITED：让 Windows 把整窗（含所有子窗口）双缓冲合成后再显示。
     * 拖动分栏等高频重排时，子窗口不会各自先擦后画，视觉上就没有闪烁。
     * 只影响绘制路径，不改布局/交互，风险很低。 */
    hwnd = CreateWindowExW(WS_EX_COMPOSITED, WTK_CLASS_WINDOW, wtitle,
                           style, CW_USEDEFAULT, CW_USEDEFAULT,
                           physical_w, physical_h,
                           NULL, NULL, app->instance, window);
    if (hwnd)
        SetWindowTextW(hwnd, wtitle);
    free(wtitle);
    if (!hwnd) {
        wtk__free_widget_tree(window);
        return NULL;
    }

    window->hwnd = hwnd;
    window->open_counted = 1;
    app->open_windows++;
    wtk__add_window(app, window);
    return window;
}

void wtk_window_show(WtkWidget *window)
{
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;
    window->visible = 1;
    wtk__layout_window(window);
    ShowWindow(window->hwnd, SW_SHOW);
    UpdateWindow(window->hwnd);
    window->shown = 1;
}

void wtk_window_center(WtkWidget *window)
{
    RECT rc;
    MONITORINFO mi;
    HMONITOR monitor;
    int width;
    int height;
    int x;
    int y;

    if (!window || !window->hwnd)
        return;

    GetWindowRect(window->hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    monitor = MonitorFromWindow(window->hwnd, MONITOR_DEFAULTTONEAREST);
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(monitor, &mi))
        return;

    x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    if (x < mi.rcWork.left)
        x = mi.rcWork.left;
    if (y < mi.rcWork.top)
        y = mi.rcWork.top;

    SetWindowPos(window->hwnd, NULL, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void wtk_window_set_title(WtkWidget *window, const char *title)
{
    WCHAR *wtitle;
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;
    wtitle = wtk__u8_to_u16(title ? title : "");
    SetWindowTextW(window->hwnd, wtitle);
    free(wtitle);
}

void wtk_window_set_min_size(WtkWidget *window, int width, int height)
{
    if (!window || window->type != WTK_TYPE_WINDOW)
        return;
    window->min_w = wtk__logical(window->app, width > 0 ? width : 0);
    window->min_h = wtk__logical(window->app, height > 0 ? height : 0);
}

void wtk_window_set_resizable(WtkWidget *window, int resizable)
{
    LONG_PTR style;
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;

    window->resizable = !!resizable;
    style = GetWindowLongPtrW(window->hwnd, GWL_STYLE);
    if (resizable) {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    } else {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    SetWindowLongPtrW(window->hwnd, GWL_STYLE, style);
    SetWindowPos(window->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

/* ==========================================================================
 * 模态窗口 + 指针/捕获辅助
 *
 * 以前这些是让应用层自己去调 EnableWindow / SetCapture / GetCursorPos，
 * 既不统一也容易漏掉恢复。放在框架里由窗口生命周期统一管理：
 * 模态窗口被销毁时（包括点 X）会自动恢复其它窗口，不会把界面锁死。
 * ========================================================================== */

void wtk_window_set_modal(WtkWidget *window, int modal)
{
    int i;

    if (modal) {
        if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
            return;
        if (g_modal_window)
            return; /* 已经有一个模态窗口了 */
        g_modal_window = window;
        g_modal_disabled_count = 0;
        /* 禁用同一 App 里其它所有已显示的窗口 */
        for (i = 0; i < window->app->window_count; i++) {
            WtkWidget *other = window->app->windows[i];
            if (!other || other == window || !other->hwnd || !other->shown)
                continue;
            if (g_modal_disabled_count >= 32)
                break;
            EnableWindow(other->hwnd, FALSE);
            g_modal_disabled[g_modal_disabled_count++] = other->hwnd;
        }
        /* 模态窗口自身置顶到属主之前 */
        SetWindowPos(window->hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return;
    }

    if (g_modal_window != window)
        return;
    wtk__modal_release();
}

/* 内部：释放模态（窗口销毁时也会调） */
void wtk__modal_release(void)
{
    int i;
    for (i = 0; i < g_modal_disabled_count; i++) {
        if (g_modal_disabled[i] && IsWindow(g_modal_disabled[i]))
            EnableWindow(g_modal_disabled[i], TRUE);
    }
    g_modal_disabled_count = 0;
    if (g_modal_window) {
        HWND self = g_modal_window->hwnd;
        g_modal_window = NULL;
        /* 焦点还给主窗口 */
        if (self && IsWindow(self)) {
            HWND owner = GetWindow(self, GW_OWNER);
            if (owner && IsWindow(owner)) {
                SetForegroundWindow(owner);
                SetActiveWindow(owner);
            }
        }
    }
}

int wtk_window_is_modal(WtkWidget *window)
{
    return window && window == g_modal_window;
}

/* 鼠标捕获：拖拽分栏之类需要鼠标离开控件后仍收得到位置 */
void wtk_widget_set_capture(WtkWidget *widget, int on)
{
    if (!widget || !widget->hwnd)
        return;
    if (on) {
        SetCapture(widget->hwnd);
    } else if (GetCapture() == widget->hwnd) {
        ReleaseCapture();
    }
}

int wtk_widget_has_capture(WtkWidget *widget)
{
    return widget && widget->hwnd && GetCapture() == widget->hwnd;
}

/* 左键是否仍按着（读本线程消息队列状态，不是全键盘轮询） */
int wtk_mouse_left_down(void)
{
    return (GetKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
}

/* 光标的客户区坐标（相对任意控件） */
int wtk_pointer_pos(WtkWidget *widget, int *x, int *y)
{
    POINT p;
    if (!widget || !widget->hwnd)
        return 0;
    if (!GetCursorPos(&p))
        return 0;
    if (!ScreenToClient(widget->hwnd, &p))
        return 0;
    if (x)
        *x = p.x;
    if (y)
        *y = p.y;
    return 1;
}

/* 窗口/控件客户区尺寸 */
int wtk_widget_client_size(WtkWidget *widget, int *width, int *height)
{
    RECT rc;
    if (!widget || !widget->hwnd)
        return 0;
    if (!GetClientRect(widget->hwnd, &rc))
        return 0;
    if (width)
        *width = rc.right - rc.left;
    if (height)
        *height = rc.bottom - rc.top;
    return 1;
}

void wtk_window_bring_to_front(WtkWidget *widget)
{
    if (!widget || !widget->hwnd)
        return;
    SetForegroundWindow(widget->hwnd);
    SetActiveWindow(widget->hwnd);
}

int wtk_window_get_title(WtkWidget *window, char *buf, size_t cap)
{
    WCHAR wt[512];
    int n;
    if (!window || !window->hwnd || !buf || cap == 0)
        return 0;
    wt[0] = 0;
    GetWindowTextW(window->hwnd, wt, 511);
    n = WideCharToMultiByte(CP_UTF8, 0, wt, -1, buf, (int)cap, NULL, NULL);
    if (n <= 0) {
        buf[0] = 0;
        return 0;
    }
    return n - 1;
}

void wtk_window_close(WtkWidget *window)
{
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;
    PostMessageW(window->hwnd, WM_CLOSE, 0, 0);
}

void wtk_quit(WtkApp *app)
{
    if (!app)
        return;
    PostQuitMessage(0);
}

/* ------------------------------------------------------------------------- */
/* Menus                                                                     */
/* ------------------------------------------------------------------------- */

WtkMenu *wtk_menubar(WtkWidget *window)
{
    WtkMenu *bar;
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return NULL;
    if (window->menu)
        return window->menu;

    bar = (WtkMenu *)wtk__alloc_zero(sizeof(*bar));
    bar->app = window->app;
    bar->window = window->hwnd;
    bar->is_bar = 1;
    bar->handle = CreateMenu();
    if (!bar->handle) {
        free(bar);
        return NULL;
    }

    window->menu = bar;
    SetMenu(window->hwnd, bar->handle);
    DrawMenuBar(window->hwnd);
    return bar;
}

WtkMenu *wtk_menu_submenu(WtkMenu *parent, const char *label)
{
    WtkMenu *menu;
    WCHAR *wlabel;
    UINT id;
    if (!parent || !parent->handle)
        return NULL;

    menu = (WtkMenu *)wtk__alloc_zero(sizeof(*menu));
    menu->app = parent->app;
    menu->window = parent->window;
    menu->parent = parent;
    id = parent->app->next_menu_id++;
    if (parent->app->next_menu_id < 1000)
        parent->app->next_menu_id = 1000;
    menu->item_id = id;
    menu->handle = CreatePopupMenu();
    if (!menu->handle) {
        free(menu);
        return NULL;
    }

    wlabel = wtk__u8_to_u16(label ? label : "");
    if (!AppendMenuW(parent->handle, MF_POPUP | MF_STRING,
                     (UINT_PTR)menu->handle, wlabel)) {
        free(wlabel);
        DestroyMenu(menu->handle);
        free(menu);
        return NULL;
    }
    free(wlabel);

    if (!parent->first_child) {
        parent->first_child = menu;
    } else {
        WtkMenu *it = parent->first_child;
        while (it->next_sibling)
            it = it->next_sibling;
        it->next_sibling = menu;
    }
    if (parent->window)
        DrawMenuBar(parent->window);
    return menu;
}

static char *menu_dup_text(const char *text)
{
    size_t len;
    char *copy;
    if (!text)
        return NULL;
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static WCHAR *menu_action_display(const WtkMenuAction *action)
{
    const char *text = action && action->text ? action->text : "";
    const char *shortcut = action && action->shortcut ? action->shortcut : "";
    WCHAR *wtext = wtk__u8_to_u16(text);
    WCHAR *wshortcut = NULL;
    WCHAR *display;
    size_t text_len;
    size_t shortcut_len = 0;

    if (!wtext)
        return NULL;
    if (shortcut[0])
        wshortcut = wtk__u8_to_u16(shortcut);
    text_len = wcslen(wtext);
    if (wshortcut)
        shortcut_len = wcslen(wshortcut) + 1;
    display = (WCHAR *)malloc((text_len + shortcut_len + 1) * sizeof(WCHAR));
    if (!display) {
        free(wtext);
        free(wshortcut);
        return NULL;
    }
    wcscpy(display, wtext);
    if (wshortcut) {
        wcscat(display, L"\t");
        wcscat(display, wshortcut);
    }
    free(wtext);
    free(wshortcut);
    return display;
}

static int menu_action_refresh(WtkMenuAction *action)
{
    MENUITEMINFOW item;
    WCHAR *display;
    int ok;
    if (!action || !action->menu || !action->menu->handle)
        return 0;
    display = menu_action_display(action);
    if (!display)
        return 0;
    memset(&item, 0, sizeof(item));
    item.cbSize = sizeof(item);
    item.fMask = MIIM_STRING;
    item.dwTypeData = display;
    ok = SetMenuItemInfoW(action->menu->handle, action->id, FALSE, &item);
    free(display);
    if (ok && action->menu->window)
        DrawMenuBar(action->menu->window);
    return ok;
}

static void menu_action_apply_flags(WtkMenuAction *action)
{
    MENUITEMINFOW item;
    UINT flags = MF_BYCOMMAND;
    if (!action || !action->menu || !action->menu->handle)
        return;
    if (action->checkable) {
        flags |= action->checked ? MF_CHECKED : MF_UNCHECKED;
        CheckMenuItem(action->menu->handle, action->id, flags);
    }
    EnableMenuItem(action->menu->handle, action->id,
                   MF_BYCOMMAND |
                       (action->enabled ? MF_ENABLED : MF_GRAYED));
    if (action->radio) {
        memset(&item, 0, sizeof(item));
        item.cbSize = sizeof(item);
        item.fMask = MIIM_FTYPE;
        item.fType = MFT_RADIOCHECK;
        SetMenuItemInfoW(action->menu->handle, action->id, FALSE, &item);
    }
}

static void menu_radio_select(WtkMenuAction *action);

static WtkMenuAction *menu_add_action(WtkMenu *menu, const char *text,
                                      int checkable, int radio, int checked,
                                      WtkMenuActionFn callback,
                                      WtkMenuFn legacy_callback,
                                      void *userdata)
{
    WtkMenuAction *action;
    WCHAR *wtext;
    UINT id;
    UINT flags;

    if (!menu || !menu->handle || (!callback && !legacy_callback))
        return NULL;
    action = (WtkMenuAction *)wtk__alloc_zero(sizeof(*action));
    if (!action)
        return NULL;
    action->menu = menu;
    action->legacy_callback = legacy_callback;
    action->callback = callback;
    action->userdata = userdata;
    action->checkable = !!checkable;
    action->radio = !!radio;
    action->checked = checkable && checked;
    action->enabled = 1;
    action->text = menu_dup_text(text ? text : "");
    if (!action->text) {
        free(action);
        return NULL;
    }
    wtext = menu_action_display(action);

    id = menu->app->next_menu_id++;
    if (menu->app->next_menu_id < 1000)
        menu->app->next_menu_id = 1000;
    action->id = id;

    if (menu->action_count >= menu->action_cap) {
        int cap = menu->action_cap ? menu->action_cap * 2 : 8;
        WtkMenuAction **grown = (WtkMenuAction **)wtk__realloc_array(
            menu->actions, (size_t)cap * sizeof(*grown));
        if (!grown) {
            free(action->text);
            free(action);
            free(wtext);
            return NULL;
        }
        menu->actions = grown;
        menu->action_cap = cap;
    }
    menu->actions[menu->action_count++] = action;

    flags = MF_STRING | (action->checked ? MF_CHECKED : 0);
    if (!wtext ||
        !AppendMenuW(menu->handle, flags, (UINT_PTR)id, wtext)) {
        menu->action_count--;
        free(wtext);
        free(action->text);
        free(action);
        return NULL;
    }
    free(wtext);
    if (action->radio && action->checked) {
        menu_radio_select(action);
        menu_action_apply_flags(action);
    } else {
        menu_action_apply_flags(action);
    }
    if (menu->window)
        DrawMenuBar(menu->window);
    return action;
}

WtkMenuAction *wtk_menu_add_action(WtkMenu *menu, const char *text,
                                   WtkMenuActionFn callback, void *userdata)
{
    return menu_add_action(menu, text, 0, 0, 0, callback, NULL, userdata);
}

WtkMenuAction *wtk_menu_add_check_action(WtkMenu *menu, const char *text,
                                         int checked,
                                         WtkMenuActionFn callback,
                                         void *userdata)
{
    return menu_add_action(menu, text, 1, 0, checked, callback, NULL,
                           userdata);
}

WtkMenuAction *wtk_menu_add_radio_action(WtkMenu *menu, const char *text,
                                         int checked,
                                         WtkMenuActionFn callback,
                                         void *userdata)
{
    return menu_add_action(menu, text, 1, 1, checked, callback, NULL,
                           userdata);
}

void wtk_menu_add_command(WtkMenu *menu, const char *label, WtkMenuFn callback,
                          void *userdata)
{
    (void)menu_add_action(menu, label, 0, 0, 0, NULL, callback, userdata);
}

const char *wtk_menu_action_get_text(const WtkMenuAction *action)
{
    return action && action->text ? action->text : "";
}

int wtk_menu_action_set_text(WtkMenuAction *action, const char *text)
{
    char *copy;
    if (!action)
        return 0;
    copy = menu_dup_text(text);
    if (!copy)
        return 0;
    free(action->text);
    action->text = copy;
    return menu_action_refresh(action);
}

int wtk_menu_action_set_shortcut_hint(WtkMenuAction *action, const char *hint)
{
    char *copy;
    if (!action)
        return 0;
    copy = hint && hint[0] ? menu_dup_text(hint) : NULL;
    free(action->shortcut);
    action->shortcut = copy;
    return menu_action_refresh(action);
}

void wtk_menu_action_set_enabled(WtkMenuAction *action, int enabled)
{
    if (!action)
        return;
    action->enabled = !!enabled;
    menu_action_apply_flags(action);
    if (action->menu->window)
        DrawMenuBar(action->menu->window);
}

int wtk_menu_action_is_enabled(const WtkMenuAction *action)
{
    return action && action->enabled;
}

void wtk_menu_action_set_checkable(WtkMenuAction *action, int checkable)
{
    if (!action)
        return;
    action->checkable = !!checkable;
    if (!action->checkable) {
        action->checked = 0;
        action->radio = 0;
        CheckMenuItem(action->menu->handle, action->id,
                      MF_BYCOMMAND | MF_UNCHECKED);
    }
    menu_action_apply_flags(action);
    if (action->menu->window)
        DrawMenuBar(action->menu->window);
}

int wtk_menu_action_is_checkable(const WtkMenuAction *action)
{
    return action && action->checkable;
}

static void menu_radio_select(WtkMenuAction *action)
{
    WtkMenu *menu;
    int i;
    if (!action || !action->menu)
        return;
    menu = action->menu;
    for (i = 0; i < menu->action_count; ++i) {
        WtkMenuAction *other = menu->actions[i];
        if (!other || other == action || !other->radio)
            continue;
        other->checked = 0;
        CheckMenuItem(menu->handle, other->id,
                      MF_BYCOMMAND | MF_UNCHECKED);
    }
    action->checked = 1;
    CheckMenuItem(menu->handle, action->id, MF_BYCOMMAND | MF_CHECKED);
}

void wtk_menu_action_set_checked(WtkMenuAction *action, int checked)
{
    if (!action || !action->checkable)
        return;
    if (action->radio && checked)
        menu_radio_select(action);
    else {
        action->checked = !!checked;
        CheckMenuItem(action->menu->handle, action->id,
                      MF_BYCOMMAND |
                          (action->checked ? MF_CHECKED : MF_UNCHECKED));
    }
    if (action->menu->window)
        DrawMenuBar(action->menu->window);
}

int wtk_menu_action_is_checked(const WtkMenuAction *action)
{
    return action && action->checked;
}

static void menu_dispatch_action(WtkWidget *window, WtkMenuAction *action)
{
    if (!window || !action || !window->app)
        return;
    if (action->checkable) {
        if (action->radio)
            menu_radio_select(action);
        else
            wtk_menu_action_set_checked(action, !action->checked);
    }
    wtk__begin_dispatch(window->app);
    if (action->callback)
        action->callback(window, action, action->userdata);
    else if (action->legacy_callback)
        action->legacy_callback(window, action->userdata);
    wtk__end_dispatch(window->app);
}

void wtk_menu_action_trigger(WtkMenuAction *action)
{
    WtkWidget *window;
    if (!action || !action->menu)
        return;
    window = wtk__window_from_hwnd(action->menu->app, action->menu->window);
    if (window)
        menu_dispatch_action(window, action);
}

WtkMenuAction *wtk_menu_get_action(WtkMenu *menu, int index)
{
    if (!menu || index < 0 || index >= menu->action_count)
        return NULL;
    return menu->actions[index];
}

void wtk_menu_set_enabled(WtkMenu *menu, int enabled)
{
    if (!menu || !menu->parent || !menu->parent->handle || !menu->item_id)
        return;
    EnableMenuItem(menu->parent->handle, menu->item_id,
                   MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
    if (menu->window)
        DrawMenuBar(menu->window);
}

void wtk_menu_set_item_enabled(WtkMenu *menu, int index, int enabled)
{
    if (!menu || index < 0 || index >= menu->action_count ||
        !menu->actions[index])
        return;
    wtk_menu_action_set_enabled(menu->actions[index], enabled);
}

static WtkMenu *menu_find_by_id(WtkMenu *menu, UINT id,
                                WtkMenuAction **action_out)
{
    int i;
    WtkMenu *child;
    if (!menu)
        return NULL;
    for (i = 0; i < menu->action_count; ++i) {
        WtkMenuAction *action = menu->actions[i];
        if (action && action->id == id) {
            if (action_out)
                *action_out = action;
            return menu;
        }
    }
    for (child = menu->first_child; child; child = child->next_sibling) {
        WtkMenu *found = menu_find_by_id(child, id, action_out);
        if (found)
            return found;
    }
    return NULL;
}

void wtk_menu_add_separator(WtkMenu *menu)
{
    if (!menu || !menu->handle)
        return;
    AppendMenuW(menu->handle, MF_SEPARATOR, 0, NULL);
    if (menu->window)
        DrawMenuBar(menu->window);
}

void wtk__handle_menu_command(WtkWidget *window, UINT id)
{
    WtkMenuAction *action = NULL;
    if (!window || !window->menu)
        return;
    if (!menu_find_by_id(window->menu, id, &action) || !action)
        return;
    menu_dispatch_action(window, action);
}

void wtk__free_menu_tree(WtkMenu *menu)
{
    while (menu) {
        WtkMenu *next = menu->next_sibling;
        int i;
        if (menu->first_child)
            wtk__free_menu_tree(menu->first_child);
        for (i = 0; i < menu->action_count; ++i) {
            WtkMenuAction *action = menu->actions[i];
            if (action) {
                free(action->text);
                free(action->shortcut);
                free(action);
            }
        }
        free(menu->actions);
        free(menu);
        menu = next;
    }
}

/* ------------------------------------------------------------------------- */
/* Focus traversal and event loop                                            */
/* ------------------------------------------------------------------------- */

int wtk__radio_index(WtkWidget *radio)
{
    int index = 0;
    int i;
    if (!radio || radio->type != WTK_TYPE_RADIO || !radio->parent)
        return -1;
    for (i = 0; i < radio->parent->child_count; ++i) {
        WtkWidget *child = radio->parent->children[i];
        if (child->type != WTK_TYPE_RADIO)
            continue;
        if (child == radio)
            return index;
        index++;
    }
    return -1;
}

int wtk__radio_selected(WtkWidget *radio)
{
    int index;
    int group_value;
    if (!radio || radio->type != WTK_TYPE_RADIO || !radio->parent)
        return 0;
    if (radio->parent->type != WTK_TYPE_RADIOGROUP)
        return radio->checked;
    index = wtk__radio_index(radio);
    group_value = radio->parent->value;
    if (group_value < 0) {
        int i;
        for (i = 0; i < radio->parent->child_count; ++i) {
            WtkWidget *child = radio->parent->children[i];
            if (child->type == WTK_TYPE_RADIO)
                return child == radio;
        }
        return 0;
    }
    return index == group_value;
}

HWND wtk__focus_target(WtkWidget *widget)
{
    if (!widget || !widget->enabled || !widget->visible)
        return NULL;
    if (wtk__is_field(widget))
        return widget->input;
    if (widget->type == WTK_TYPE_RADIOGROUP) {
        int i;
        WtkWidget *selected = NULL;
        for (i = 0; i < widget->child_count; ++i) {
            WtkWidget *child = widget->children[i];
            if (child->type == WTK_TYPE_RADIO &&
                wtk__radio_selected(child)) {
                selected = child;
                break;
            }
        }
        if (!selected) {
            for (i = 0; i < widget->child_count; ++i) {
                if (widget->children[i]->type == WTK_TYPE_RADIO) {
                    selected = widget->children[i];
                    break;
                }
            }
        }
        return selected ? wtk__focus_target(selected) : NULL;
    }
    return widget->focusable ? widget->hwnd : NULL;
}

typedef struct FocusList {
    HWND *items;
    int count;
    int cap;
} FocusList;

static void focus_list_add(FocusList *list, HWND hwnd)
{
    if (!hwnd)
        return;
    if (list->count >= list->cap) {
        int new_cap = list->cap ? list->cap * 2 : 16;
        list->items = (HWND *)wtk__realloc_array(
            list->items, (size_t)new_cap * sizeof(HWND));
        list->cap = new_cap;
    }
    list->items[list->count++] = hwnd;
}

static int first_radio_of_group(WtkWidget *radio)
{
    int i;
    if (!radio || !radio->parent || radio->parent->type != WTK_TYPE_RADIOGROUP)
        return 0;
    for (i = 0; i < radio->parent->child_count; ++i) {
        if (radio->parent->children[i]->type == WTK_TYPE_RADIO)
            return radio->parent->children[i] == radio;
    }
    return 0;
}

static void collect_focus(WtkWidget *widget, FocusList *list)
{
    int i;
    if (!widget || !widget->visible || !widget->enabled)
        return;

    if (widget->type == WTK_TYPE_RADIO && widget->parent &&
        widget->parent->type == WTK_TYPE_RADIOGROUP) {
        int group_value = widget->parent->value;
        if (group_value >= 0) {
            if (wtk__radio_index(widget) != group_value)
                return;
        } else if (!first_radio_of_group(widget)) {
            return;
        }
    }

    if (widget->focusable) {
        HWND target = wtk__focus_target(widget);
        if (target)
            focus_list_add(list, target);
    }

    for (i = 0; i < widget->child_count; ++i)
        collect_focus(widget->children[i], list);
}

void wtk__focus_next(WtkWidget *window, int reverse)
{
    FocusList list;
    HWND current;
    int index = -1;
    int i;
    int next;

    if (!window || !window->hwnd)
        return;

    memset(&list, 0, sizeof(list));
    collect_focus(window, &list);
    if (list.count == 0) {
        free(list.items);
        return;
    }

    current = GetFocus();
    for (i = 0; i < list.count; ++i) {
        if (list.items[i] == current) {
            index = i;
            break;
        }
    }

    if (index < 0)
        next = reverse ? list.count - 1 : 0;
    else if (reverse)
        next = (index + list.count - 1) % list.count;
    else
        next = (index + 1) % list.count;

    SetFocus(list.items[next]);
    free(list.items);
}

static int window_has_live_hwnd(WtkWidget *window)
{
    return window && window->hwnd && IsWindow(window->hwnd);
}

/* ------------------------------------------------------------------------- */
/* 全局快捷键                                                                */
/* ------------------------------------------------------------------------- */

static int shortcut_match(const WtkShortcut *s, int vk, unsigned mods)
{
    if (!s || s->vk != vk)
        return 0;
    return (s->mods & (WTK_MOD_CONTROL | WTK_MOD_SHIFT | WTK_MOD_ALT)) ==
           (mods & (WTK_MOD_CONTROL | WTK_MOD_SHIFT | WTK_MOD_ALT));
}

/* 内部：命中就执行并返回 1（消息不再下发） */
static int wtk__handle_shortcut(WtkApp *app, int vk, unsigned mods)
{
    int i;
    if (!app)
        return 0;
    for (i = 0; i < app->shortcut_count; i++) {
        if (shortcut_match(&app->shortcuts[i], vk, mods)) {
            WtkMenuFn cb = app->shortcuts[i].callback;
            void *ud = app->shortcuts[i].userdata;
            WtkWidget *win = NULL;
            int j;
            for (j = 0; j < app->window_count; j++) {
                WtkWidget *w = app->windows[j];
                if (w && w->shown && w->hwnd) {
                    win = w;
                    break;
                }
            }
            if (cb)
                cb(win, ud);
            return 1;
        }
    }
    return 0;
}

int wtk_app_add_shortcut(WtkApp *app, int vk, unsigned mods, WtkMenuFn callback,
                         void *userdata)
{
    WtkShortcut *s;
    if (!app || !callback || vk == 0)
        return 0;
    if (app->shortcut_count >= app->shortcut_cap) {
        int cap = app->shortcut_cap ? app->shortcut_cap * 2 : 8;
        WtkShortcut *grown = (WtkShortcut *)realloc(app->shortcuts,
                                                   (size_t)cap * sizeof(WtkShortcut));
        if (!grown)
            return 0;
        app->shortcuts = grown;
        app->shortcut_cap = cap;
    }
    s = &app->shortcuts[app->shortcut_count++];
    s->vk = vk;
    s->mods = mods;
    s->callback = callback;
    s->userdata = userdata;
    return 1;
}

int wtk_run(WtkApp *app)
{
    MSG msg;
    int shown_any = 0;
    int i;
    int result = 0;

    if (!app || app->running)
        return -1;

    app->running = 1;

    for (i = 0; i < app->window_count; ++i) {
        WtkWidget *window = app->windows[i];
        if (!window_has_live_hwnd(window) || !window->visible)
            continue;
        if (!window->shown) {
            ShowWindow(window->hwnd, SW_SHOW);
            window->shown = 1;
        }
        wtk__layout_window(window);
        UpdateWindow(window->hwnd);
        shown_any = 1;
    }

    if (!shown_any) {
        app->running = 0;
        return 0;
    }

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        int is_tab = (msg.message == WM_KEYDOWN) && msg.wParam == VK_TAB;
        if (msg.message == WM_KEYDOWN) {
            unsigned mods = 0;
            if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_CONTROL) & 0x8000))
                mods |= WTK_MOD_CONTROL;
            if ((GetKeyState(VK_SHIFT) & 0x8000) || (GetAsyncKeyState(VK_SHIFT) & 0x8000))
                mods |= WTK_MOD_SHIFT;
            if ((GetKeyState(VK_MENU) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000))
                mods |= WTK_MOD_ALT;
            if (wtk__handle_shortcut(app, (int)msg.wParam, mods))
                continue; /* 已由快捷键处理，不再下发给控件 */
        }
        if (is_tab && !(GetKeyState(VK_CONTROL) & 0x8000) &&
            !(GetKeyState(VK_MENU) & 0x8000)) {
            WtkWidget *window = wtk__window_from_hwnd(app, msg.hwnd);
            if (window) {
                int reverse = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                wtk__focus_next(window, reverse);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    app->running = 0;
    wtk__sweep_graveyard(app);
    if (msg.message == WM_QUIT)
        result = (int)msg.wParam;
    return result;
}
