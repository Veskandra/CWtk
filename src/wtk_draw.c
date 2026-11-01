#define INITGUID 1
#include "wtk_internal.h"

/* ------------------------------------------------------------------------- */
/* Direct2D bridge                                                           */
/* ------------------------------------------------------------------------- */

static ID2D1DCRenderTarget *g_d2d_target;
static ID2D1RenderTarget *g_d2d_render_target;
static WtkApp *g_d2d_app;
static int g_d2d_active;

static D2D1_COLOR_F d2d_color(WtkColor color)
{
    D2D1_COLOR_F out;
    out.r = (float)((color >> 16) & 0xFF) / 255.0f;
    out.g = (float)((color >> 8) & 0xFF) / 255.0f;
    out.b = (float)(color & 0xFF) / 255.0f;
    out.a = 1.0f;
    return out;
}

static D2D1_RECT_F d2d_rect(const RECT *rect)
{
    D2D1_RECT_F out;
    out.left = (float)rect->left;
    out.top = (float)rect->top;
    out.right = (float)rect->right;
    out.bottom = (float)rect->bottom;
    return out;
}

static ID2D1Brush *d2d_brush(WtkColor color)
{
    ID2D1SolidColorBrush *solid = NULL;
    ID2D1Brush *brush = NULL;
    D2D1_COLOR_F c;
    if (!g_d2d_render_target)
        return NULL;
    c = d2d_color(color);
    if (FAILED(g_d2d_render_target->lpVtbl->CreateSolidColorBrush(
            g_d2d_render_target, &c, NULL, &solid)))
        return NULL;
    if (FAILED(ID2D1SolidColorBrush_QueryInterface(
            solid, &IID_ID2D1Brush, (void **)&brush)) ||
        !brush) {
        ID2D1SolidColorBrush_Release(solid);
        return NULL;
    }
    ID2D1SolidColorBrush_Release(solid);
    return brush;
}

static void d2d_release_target(WtkApp *app)
{
    if (g_d2d_render_target) {
        ID2D1RenderTarget_Release(g_d2d_render_target);
        g_d2d_render_target = NULL;
    }
    if (g_d2d_target) {
        ID2D1DCRenderTarget_Release(g_d2d_target);
        g_d2d_target = NULL;
        if (app)
            app->d2d_target = NULL;
    }
}

static void d2d_end(void)
{
    if (!g_d2d_active)
        return;
    g_d2d_active = 0;
    if (g_d2d_render_target) {
        HRESULT hr = g_d2d_render_target->lpVtbl->EndDraw(
            g_d2d_render_target, NULL, NULL);
        if (FAILED(hr)) {
            WtkApp *app = g_d2d_app;
            d2d_release_target(app);
        } else {
            ID2D1RenderTarget_Release(g_d2d_render_target);
            g_d2d_render_target = NULL;
        }
    }
    g_d2d_app = NULL;
}

static int d2d_begin(WtkApp *app, HDC dc, int width, int height)
{
    RECT rc;
    D2D1_MATRIX_3X2_F identity;
    identity.m11 = 1.0f;
    identity.m12 = 0.0f;
    identity.m21 = 0.0f;
    identity.m22 = 1.0f;
    identity.dx = 0.0f;
    identity.dy = 0.0f;
    if (!app || !dc || width <= 0 || height <= 0 || g_d2d_active)
        return 0;
    if (!wtk__d2d_ensure(app))
        return 0;
    if (!app->d2d_target)
        return 0;
    g_d2d_target = app->d2d_target;
    if (FAILED(ID2D1DCRenderTarget_QueryInterface(
            g_d2d_target, &IID_ID2D1RenderTarget,
            (void **)&g_d2d_render_target)) ||
        !g_d2d_render_target) {
        g_d2d_target = NULL;
        return 0;
    }
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    if (FAILED(g_d2d_target->lpVtbl->BindDC(g_d2d_target, dc, &rc))) {
        d2d_release_target(app);
        return 0;
    }
    g_d2d_render_target->lpVtbl->SetDpi(g_d2d_render_target, 96.0f, 96.0f);
    g_d2d_render_target->lpVtbl->SetTransform(g_d2d_render_target, &identity);
    g_d2d_render_target->lpVtbl->SetAntialiasMode(
        g_d2d_render_target, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_d2d_render_target->lpVtbl->BeginDraw(g_d2d_render_target);
    g_d2d_app = app;
    g_d2d_active = 1;
    return 1;
}

static void d2d_fill_rect(RECT rect, WtkColor color)
{
    ID2D1Brush *brush = d2d_brush(color);
    D2D1_RECT_F r = d2d_rect(&rect);
    if (!brush)
        return;
    g_d2d_render_target->lpVtbl->FillRectangle(g_d2d_render_target, &r,
                                                brush);
    ID2D1Brush_Release(brush);
}

/* ------------------------------------------------------------------------- */
/* Low-level GDI helpers                                                     */
/* ------------------------------------------------------------------------- */

void wtk__fill_rect_color(HDC dc, const RECT *rect, WtkColor color)
{
    HBRUSH brush;
    if (g_d2d_active) {
        d2d_fill_rect(*rect, color);
        return;
    }
    brush = CreateSolidBrush(wtk__colorref(color));
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void fill_rect(HDC dc, RECT rect, WtkColor color)
{
    wtk__fill_rect_color(dc, &rect, color);
}

static WtkColor backdrop_color(WtkWidget *widget)
{
    if (widget->parent)
        return wtk__container_bg(widget->parent);
    return widget->app->theme.window_bg;
}

static void paint_canvas(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkColor bg = widget->bg_set ? widget->bg : widget->app->theme.surface;
    if (widget->d2d_draw_callback &&
        d2d_begin(widget->app, dc, widget->w, widget->h)) {
        d2d_fill_rect(*bounds, bg);
        widget->d2d_draw_callback(widget, g_d2d_render_target, widget->w,
                                  widget->h, widget->d2d_draw_data);
        d2d_end();
    } else {
        fill_rect(dc, *bounds, bg);
        if (widget->draw_callback && widget->w > 0 && widget->h > 0) {
            widget->draw_callback(widget, dc, widget->w, widget->h,
                                  widget->draw_data);
        }
    }
    if (widget->focused && widget->focus_ring) {
        RECT rc = *bounds;
        HPEN pen = CreatePen(PS_SOLID, wtk__scale_widget(widget, 1),
                             wtk__colorref(widget->app->theme.accent));
        HGDIOBJ old_pen = SelectObject(dc, pen);
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }
}


void wtk__paint_window_background(WtkWidget *window, HDC dc,
                                  const RECT *bounds)
{
    WtkColor color;
    if (!window || !bounds)
        return;
    if (window->app->style_painter && window->app->style_painter->paint) {
        WtkPaintState state;
        wtk__build_paint_state(window, &state);
        window->app->style_painter->paint(
            window, dc, bounds, &state, window->app->style_painter_data);
        return;
    }
    color = window->bg_set ? window->bg : window->app->theme.window_bg;
    fill_rect(dc, *bounds, color);
}

static void paint_plain_container(WtkWidget *widget, HDC dc,
                                  const RECT *bounds)
{
    WtkColor bg = widget->bg_set ? widget->bg : backdrop_color(widget);
    fill_rect(dc, *bounds, bg);
    if (widget->border_set && widget->border_width > 0) {
        HBRUSH brush = CreateSolidBrush(wtk__colorref(widget->border));
        FrameRect(dc, bounds, brush);
        DeleteObject(brush);
    }
}

void wtk__paint_widget(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    if (!widget || !bounds)
        return;
    if (widget->type == WTK_TYPE_CANVAS) {
        paint_canvas(widget, dc, bounds);
        return;
    }
    if (widget->type == WTK_TYPE_WINDOW) {
        wtk__paint_window_background(widget, dc, bounds);
        return;
    }
    if (widget->app->style_painter && widget->app->style_painter->paint) {
        WtkPaintState state;
        wtk__build_paint_state(widget, &state);
        widget->app->style_painter->paint(
            widget, dc, bounds, &state, widget->app->style_painter_data);
        return;
    }
    if (widget->type == WTK_TYPE_FRAME ||
        widget->type == WTK_TYPE_RADIOGROUP) {
        paint_plain_container(widget, dc, bounds);
        return;
    }
    fill_rect(dc, *bounds, backdrop_color(widget));
}