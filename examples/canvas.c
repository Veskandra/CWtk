/* WinTk Canvas example: build with `mingw32-make canvas`. */
#include "wintk.h"

#include <stdio.h>
#include <wchar.h>

#define MAX_POINTS 64

typedef struct CanvasState {
    WtkWidget *window;
    WtkWidget *canvas;
    double xs[MAX_POINTS];
    double ys[MAX_POINTS];
    int count;
    int tracking;
} CanvasState;

static void utf8_to_wide(const char *text, WCHAR *out, int capacity)
{
    if (!out || capacity <= 0)
        return;
    out[0] = L'\0';
    if (!text)
        return;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, capacity);
}

static COLORREF color_ref(WtkColor color)
{
    return RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

static void canvas_draw(WtkWidget *canvas, HDC dc, int width, int height,
                        void *userdata)
{
    CanvasState *state = (CanvasState *)userdata;
    WtkApp *app = wtk_widget_app(canvas);
    const WtkTheme *theme = wtk_app_get_theme(app);
    int dpi = wtk_app_dpi(app);
    int grid = wtk_scale(app, 24);
    int radius = wtk_scale(app, 6);
    int x;
    int y;
    HPEN old_pen;
    HBRUSH old_brush;
    HFONT old_font;
    HFONT font;
    WCHAR hint[128];
    WCHAR count_text[64];
    POINT line[MAX_POINTS];

    SetBkMode(dc, TRANSPARENT);

    /* A light grid makes the canvas dimensions visible. */
    old_pen = (HPEN)SelectObject(dc, CreatePen(PS_SOLID, 1,
                                              RGB(226, 232, 240)));
    for (x = 0; x < width; x += grid) {
        MoveToEx(dc, x, 0, NULL);
        LineTo(dc, x, height);
    }
    for (y = 0; y < height; y += grid) {
        MoveToEx(dc, 0, y, NULL);
        LineTo(dc, width, y);
    }
    DeleteObject(SelectObject(dc, old_pen));

    /* Connect the captured points. */
    for (x = 0; x < state->count; ++x) {
        line[x].x = (LONG)(state->xs[x] * width);
        line[x].y = (LONG)(state->ys[x] * height);
    }
    if (state->count > 1) {
        old_pen = (HPEN)SelectObject(dc, CreatePen(PS_SOLID,
                                                   wtk_scale(app, 2),
                                                   color_ref(theme->accent)));
        Polyline(dc, line, state->count);
        DeleteObject(SelectObject(dc, old_pen));
    }

    old_brush = (HBRUSH)SelectObject(dc, CreateSolidBrush(
                                              color_ref(theme->accent)));
    old_pen = (HPEN)SelectObject(dc, CreatePen(PS_SOLID, 1,
                                              color_ref(theme->accent)));
    for (x = 0; x < state->count; ++x) {
        Ellipse(dc, line[x].x - radius, line[x].y - radius,
                line[x].x + radius, line[x].y + radius);
    }
    DeleteObject(SelectObject(dc, old_pen));
    DeleteObject(SelectObject(dc, old_brush));

    font = CreateFontW(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    old_font = (HFONT)SelectObject(dc, font ? font : GetStockObject(
                                                     DEFAULT_GUI_FONT));
    SetTextColor(dc, color_ref(theme->text));
    utf8_to_wide("单击或拖动添加点；点击“清空”重置。", hint, 128);
    TextOutW(dc, wtk_scale(app, 18), wtk_scale(app, 14), hint,
             (int)wcslen(hint));
    swprintf(count_text, 64, L"点数：%d", state->count);
    SetTextColor(dc, color_ref(theme->muted));
    TextOutW(dc, wtk_scale(app, 18), wtk_scale(app, 38), count_text,
             (int)wcslen(count_text));
    SelectObject(dc, old_font);
    if (font)
        DeleteObject(font);
}

static void add_point(CanvasState *state, int x, int y)
{
    RECT client;
    int width;
    int height;
    if (!state || state->count >= MAX_POINTS)
        return;
    GetClientRect(wtk_widget_hwnd(state->canvas), &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
        return;
    state->xs[state->count] = (double)x / (double)width;
    state->ys[state->count] = (double)y / (double)height;
    state->count++;
}

static void on_canvas_down(WtkWidget *widget, WtkEvent *event, void *data)
{
    CanvasState *state = (CanvasState *)data;
    (void)widget;
    state->tracking = 1;
    add_point(state, event->x, event->y);
    wtk_canvas_redraw(state->canvas);
}

static void on_canvas_move(WtkWidget *widget, WtkEvent *event, void *data)
{
    CanvasState *state = (CanvasState *)data;
    (void)widget;
    if (!state->tracking)
        return;
    add_point(state, event->x, event->y);
    wtk_canvas_redraw(state->canvas);
}

static void on_canvas_up(WtkWidget *widget, WtkEvent *event, void *data)
{
    CanvasState *state = (CanvasState *)data;
    (void)widget;
    (void)event;
    state->tracking = 0;
}

static void on_clear(WtkWidget *widget, WtkEvent *event, void *data)
{
    CanvasState *state = (CanvasState *)data;
    (void)widget;
    (void)event;
    state->count = 0;
    state->tracking = 0;
    wtk_canvas_redraw(state->canvas);
}

static int run_canvas(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *card;
    WtkWidget *header;
    WtkWidget *title;
    WtkWidget *hint;
    WtkWidget *clear;
    WtkWidget *canvas;
    const WtkTheme *theme;
    CanvasState state;

    if (!app)
        return 1;
    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk Canvas 示例", 900, 650);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 680, 520);

    card = wtk_frame(window);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 16);
    wtk_widget_set_padding(card, 24, 20, 24, 20);
    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 28, 28);

    header = wtk_frame(card);
    wtk_widget_set_background(header, theme->surface);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    title = wtk_label(header, "交互式画布");
    wtk_widget_set_font(title, NULL, 22, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    hint = wtk_label(header,
                     "Canvas 的 draw 回调接收真实客户区像素尺寸，鼠标坐标也是设备像素。");
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_widget_set_font(hint, NULL, 9, WTK_FONT_NORMAL);
    wtk_pack(hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    clear = wtk_button(header, "清空");
    wtk_button_set_style(clear, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(clear, 88, 34);
    wtk_pack(clear, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 0, 8);

    canvas = wtk_canvas(card, 600, 380);
    wtk_widget_set_background(canvas, WTK_WHITE);
    wtk_pack(canvas, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);

    state.window = window;
    state.canvas = canvas;
    state.count = 0;
    state.tracking = 0;
    wtk_canvas_set_draw(canvas, canvas_draw, &state);
    wtk_bind(canvas, WTK_EVENT_MOUSE_DOWN, on_canvas_down, &state);
    wtk_bind(canvas, WTK_EVENT_MOUSE_MOVE, on_canvas_move, &state);
    wtk_bind(canvas, WTK_EVENT_MOUSE_UP, on_canvas_up, &state);
    wtk_bind(clear, WTK_EVENT_CLICK, on_clear, &state);

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
    return run_canvas();
}
