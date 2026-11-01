/* WinTk Direct2D example: build with `mingw32-make d2d`. */
#include "wintk.h"
#include <d2d1.h>
#include <stdio.h>

typedef struct D2DState {
    WtkWidget *window;
    int frames;
} D2DState;

static ID2D1Brush *make_brush(ID2D1RenderTarget *target, D2D1_COLOR_F color)
{
    ID2D1SolidColorBrush *solid = NULL;
    ID2D1Brush *brush = NULL;
    if (FAILED(target->lpVtbl->CreateSolidColorBrush(target, &color, NULL,
                                                      &solid)))
        return NULL;
    if (FAILED(ID2D1SolidColorBrush_QueryInterface(
            solid, &IID_ID2D1Brush, (void **)&brush))) {
        ID2D1SolidColorBrush_Release(solid);
        return NULL;
    }
    ID2D1SolidColorBrush_Release(solid);
    return brush;
}

static void fill_ellipse(ID2D1RenderTarget *target, float x, float y,
                         float rx, float ry, float r, float g, float b,
                         float a)
{
    D2D1_COLOR_F color;
    D2D1_ELLIPSE ellipse;
    ID2D1Brush *brush;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    ellipse.point.x = x;
    ellipse.point.y = y;
    ellipse.radiusX = rx;
    ellipse.radiusY = ry;
    brush = make_brush(target, color);
    if (!brush)
        return;
    target->lpVtbl->FillEllipse(target, &ellipse, brush);
    ID2D1Brush_Release(brush);
}

static void d2d_canvas_draw(WtkWidget *canvas, ID2D1RenderTarget *target,
                            int width, int height, void *userdata)
{
    D2DState *state = (D2DState *)userdata;
    D2D1_COLOR_F color;
    D2D1_ROUNDED_RECT card;
    ID2D1Brush *brush;
    int i;

    (void)canvas;
    color.r = 0.10f;
    color.g = 0.16f;
    color.b = 0.28f;
    color.a = 1.0f;
    brush = make_brush(target, color);
    if (brush) {
        card.rect.left = 18.0f;
        card.rect.top = 18.0f;
        card.rect.right = (float)width - 18.0f;
        card.rect.bottom = (float)height - 18.0f;
        card.radiusX = 18.0f;
        card.radiusY = 18.0f;
        target->lpVtbl->FillRoundedRectangle(target, &card, brush);
        ID2D1Brush_Release(brush);
    }

    for (i = 0; i < 7; ++i) {
        float t = (float)i / 6.0f;
        fill_ellipse(target, 70.0f + t * (float)(width - 140),
                     70.0f + (float)(i % 2) * 60.0f,
                     46.0f, 46.0f,
                     0.15f + 0.65f * t, 0.35f, 0.95f - 0.55f * t,
                     0.72f);
    }

    state->frames++;
}

static void on_exit(WtkWidget *widget, WtkEvent *event, void *data)
{
    D2DState *state = (D2DState *)data;
    (void)widget;
    (void)event;
    wtk_quit(wtk_widget_app(state->window));
}

static int run_d2d(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *window;
    WtkWidget *card;
    WtkWidget *title;
    WtkWidget *hint;
    WtkWidget *canvas;
    WtkWidget *status;
    WtkWidget *exit_button;
    const WtkTheme *theme;
    D2DState state;

    if (!app)
        return 1;
    if (!wtk_app_set_renderer(app, WTK_RENDERER_D2D)) {
        wtk_message_box(NULL, "Direct2D 不可用",
                        "无法创建 Direct2D 渲染目标，程序将使用 GDI。",
                        WTK_ICON_WARNING);
    }
    theme = wtk_app_get_theme(app);
    window = wtk_window(app, "WinTk Direct2D 示例", 900, 700);
    if (!window) {
        wtk_app_free(app);
        return 1;
    }
    wtk_window_set_min_size(window, 680, 560);

    card = wtk_frame(window);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 16);
    wtk_widget_set_padding(card, 24, 20, 24, 20);
    wtk_pack(card, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 28, 28);

    title = wtk_label(card, "Direct2D 画布");
    wtk_widget_set_font(title, NULL, 23, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);

    hint = wtk_label(card,
                     "核心控件的圆角/描边走 D2D，文字仍由 GDI ClearType 绘制；"
                     "Canvas 使用原生 ID2D1RenderTarget。");
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_widget_set_font(hint, NULL, 9, WTK_FONT_NORMAL);
    wtk_label_set_wrap_width(hint, 720);
    wtk_pack(hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    canvas = wtk_canvas(card, 700, 300);
    wtk_widget_set_background(canvas, WTK_RGB(16, 24, 40));
    wtk_pack(canvas, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 10);

    status = wtk_label(card, "Render: Direct2D 1.1 · Canvas: ID2D1RenderTarget");
    wtk_widget_set_foreground(status, theme->muted);
    wtk_widget_set_font(status, NULL, 9, WTK_FONT_NORMAL);
    wtk_pack(status, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    exit_button = wtk_button(card, "退出");
    wtk_button_set_style(exit_button, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(exit_button, 92, 34);
    wtk_pack(exit_button, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 0, 4);

    state.window = window;
    state.frames = 0;
    wtk_canvas_set_draw_d2d(canvas, d2d_canvas_draw, &state);
    wtk_bind(exit_button, WTK_EVENT_CLICK, on_exit, &state);

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
    return run_d2d();
}
