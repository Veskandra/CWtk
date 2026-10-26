#include "wtk_internal.h"

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

    hwnd = CreateWindowExW(0, WTK_CLASS_WINDOW, wtitle,
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
    if (!parent || !parent->handle)
        return NULL;

    menu = (WtkMenu *)wtk__alloc_zero(sizeof(*menu));
    menu->app = parent->app;
    menu->window = parent->window;
    menu->parent = parent;
    menu->handle = CreatePopupMenu();
    if (!menu->handle) {
        free(menu);
        return NULL;
    }

    wlabel = wtk__u8_to_u16(label ? label : "");
    if (!AppendMenuW(parent->handle, MF_POPUP, (UINT_PTR)menu->handle, wlabel)) {
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

static WtkMenu *menu_find_by_id(WtkMenu *menu, UINT id,
                                WtkMenuCommand **command)
{
    int i;
    WtkMenu *child;
    if (!menu)
        return NULL;
    for (i = 0; i < menu->command_count; ++i) {
        if (menu->commands[i].id == id) {
            if (command)
                *command = &menu->commands[i];
            return menu;
        }
    }
    for (child = menu->first_child; child; child = child->next_sibling) {
        WtkMenu *found = menu_find_by_id(child, id, command);
        if (found)
            return found;
    }
    return NULL;
}

void wtk_menu_add_command(WtkMenu *menu, const char *label, WtkMenuFn callback,
                          void *userdata)
{
    WCHAR *wlabel;
    UINT id;
    if (!menu || !menu->handle)
        return;

    id = menu->app->next_menu_id++;
    if (menu->app->next_menu_id < 1000)
        menu->app->next_menu_id = 1000;

    if (menu->command_count >= menu->command_cap) {
        int new_cap = menu->command_cap ? menu->command_cap * 2 : 8;
        menu->commands = (WtkMenuCommand *)wtk__realloc_array(
            menu->commands, (size_t)new_cap * sizeof(WtkMenuCommand));
        menu->command_cap = new_cap;
    }
    menu->commands[menu->command_count].id = id;
    menu->commands[menu->command_count].callback = callback;
    menu->commands[menu->command_count].userdata = userdata;
    menu->command_count++;

    wlabel = wtk__u8_to_u16(label ? label : "");
    if (!AppendMenuW(menu->handle, MF_STRING, (UINT_PTR)id, wlabel)) {
        menu->command_count--;
    }
    free(wlabel);

    if (menu->window)
        DrawMenuBar(menu->window);
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
    WtkMenuCommand *command = NULL;
    if (!window || !window->menu)
        return;
    if (!menu_find_by_id(window->menu, id, &command) || !command)
        return;
    wtk__begin_dispatch(window->app);
    if (command->callback)
        command->callback(window, command->userdata);
    wtk__end_dispatch(window->app);
}

void wtk__free_menu_tree(WtkMenu *menu)
{
    while (menu) {
        WtkMenu *next = menu->next_sibling;
        if (menu->first_child)
            wtk__free_menu_tree(menu->first_child);
        free(menu->commands);
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
