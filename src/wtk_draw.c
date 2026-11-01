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

static void d2d_flush(void)
{
    d2d_end();
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

static void d2d_round_rect(RECT rect, int radius, int filled,
                           WtkColor fill_color, int border_width,
                           WtkColor border_color)
{
    D2D1_ROUNDED_RECT rr;
    ID2D1Brush *brush;
    if (radius < 0)
        radius = 0;
    rr.rect = d2d_rect(&rect);
    rr.radiusX = (float)radius;
    rr.radiusY = (float)radius;
    if (filled) {
        brush = d2d_brush(fill_color);
        if (brush) {
            g_d2d_render_target->lpVtbl->FillRoundedRectangle(
                g_d2d_render_target, &rr, brush);
            ID2D1Brush_Release(brush);
        }
    }
    if (border_width > 0) {
        brush = d2d_brush(border_color);
        if (brush) {
            g_d2d_render_target->lpVtbl->DrawRoundedRectangle(
                g_d2d_render_target, &rr, brush, (float)border_width, NULL);
            ID2D1Brush_Release(brush);
        }
    }
}

static void d2d_ellipse(RECT rect, int filled, WtkColor fill_color,
                        int border_width, WtkColor border_color)
{
    D2D1_ELLIPSE e;
    ID2D1Brush *brush;
    e.point.x = ((float)rect.left + (float)rect.right) / 2.0f;
    e.point.y = ((float)rect.top + (float)rect.bottom) / 2.0f;
    e.radiusX = ((float)rect.right - (float)rect.left) / 2.0f;
    e.radiusY = ((float)rect.bottom - (float)rect.top) / 2.0f;
    if (filled) {
        brush = d2d_brush(fill_color);
        if (brush) {
            g_d2d_render_target->lpVtbl->FillEllipse(g_d2d_render_target, &e,
                                                      brush);
            ID2D1Brush_Release(brush);
        }
    }
    if (border_width > 0) {
        brush = d2d_brush(border_color);
        if (brush) {
            g_d2d_render_target->lpVtbl->DrawEllipse(
                g_d2d_render_target, &e, brush, (float)border_width, NULL);
            ID2D1Brush_Release(brush);
        }
    }
}

static void d2d_polyline(const POINT *points, int count, WtkColor color,
                         int width)
{
    ID2D1Brush *brush;
    int i;
    if (!points || count < 2)
        return;
    brush = d2d_brush(color);
    if (!brush)
        return;
    for (i = 1; i < count; ++i) {
        D2D1_POINT_2F a;
        D2D1_POINT_2F b;
        a.x = (float)points[i - 1].x;
        a.y = (float)points[i - 1].y;
        b.x = (float)points[i].x;
        b.y = (float)points[i].y;
        g_d2d_render_target->lpVtbl->DrawLine(
            g_d2d_render_target, a, b, brush, (float)width, NULL);
    }
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

static void draw_round_rect(HDC dc, RECT rect, int radius, int filled,
                            WtkColor fill_color, int border_width,
                            WtkColor border_color)
{
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    int min_size;

    if (radius < 0)
        radius = 0;
    min_size = rect.right - rect.left;
    if (rect.bottom - rect.top < min_size)
        min_size = rect.bottom - rect.top;
    if (min_size <= 0)
        return;
    if (radius > min_size / 2)
        radius = min_size / 2;

    if (border_width > 0) {
        int inset = border_width / 2;
        InflateRect(&rect, -inset, -inset);
    }

    if (g_d2d_active) {
        d2d_round_rect(rect, radius, filled, fill_color, border_width,
                       border_color);
        return;
    }

    brush = filled ? CreateSolidBrush(wtk__colorref(fill_color))
                   : (HBRUSH)GetStockObject(NULL_BRUSH);
    pen = border_width > 0
              ? CreatePen(PS_SOLID, border_width, wtk__colorref(border_color))
              : (HPEN)GetStockObject(NULL_PEN);
    old_brush = SelectObject(dc, brush);
    old_pen = SelectObject(dc, pen);

    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              radius * 2, radius * 2);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    if (filled)
        DeleteObject(brush);
    if (border_width > 0)
        DeleteObject(pen);
}

static void draw_ellipse(HDC dc, RECT rect, int filled, WtkColor fill_color,
                         int border_width, WtkColor border_color)
{
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;

    if (g_d2d_active) {
        d2d_ellipse(rect, filled, fill_color, border_width, border_color);
        return;
    }

    brush = filled ? CreateSolidBrush(wtk__colorref(fill_color))
                          : (HBRUSH)GetStockObject(NULL_BRUSH);
    pen = border_width > 0
              ? CreatePen(PS_SOLID, border_width,
                          wtk__colorref(border_color))
              : (HPEN)GetStockObject(NULL_PEN);
    old_brush = SelectObject(dc, brush);
    old_pen = SelectObject(dc, pen);

    Ellipse(dc, rect.left, rect.top, rect.right, rect.bottom);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    if (filled)
        DeleteObject(brush);
    if (border_width > 0)
        DeleteObject(pen);
}

static UINT text_align_flags(WtkAlign align)
{
    switch (align) {
    case WTK_ALIGN_CENTER:
        return DT_CENTER;
    case WTK_ALIGN_RIGHT:
        return DT_RIGHT;
    default:
        return DT_LEFT;
    }
}

static void draw_polyline(HDC dc, const POINT *points, int count,
                          WtkColor color, int width)
{
    HPEN pen;
    HGDIOBJ old;
    if (count < 2)
        return;
    if (g_d2d_active) {
        d2d_polyline(points, count, color, width);
        return;
    }
    pen = CreatePen(PS_SOLID, width, wtk__colorref(color));
    old = SelectObject(dc, pen);
    Polyline(dc, points, count);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void draw_text_single(HDC dc, HFONT font, WtkColor color,
                             const WCHAR *text, RECT rect, WtkAlign align,
                             int ellipsis)
{
    HFONT old_font;
    UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
                 text_align_flags(align);
    if (ellipsis)
        flags |= DT_END_ELLIPSIS;

    d2d_flush();
    old_font = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, wtk__colorref(color));
    DrawTextW(dc, text ? text : L"", -1, &rect, flags);
    SelectObject(dc, old_font);
}

static void draw_text_block(HDC dc, HFONT font, WtkColor color,
                            const WCHAR *text, RECT rect, WtkAlign align)
{
    HFONT old_font;
    UINT flags = DT_WORDBREAK | DT_NOPREFIX | text_align_flags(align);
    d2d_flush();
    old_font = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, wtk__colorref(color));
    DrawTextW(dc, text ? text : L"", -1, &rect, flags);
    SelectObject(dc, old_font);
}

static WtkColor backdrop_color(WtkWidget *widget)
{
    if (widget->parent)
        return wtk__container_bg(widget->parent);
    return widget->app->theme.window_bg;
}

static WtkColor label_text_color(WtkWidget *widget)
{
    if (widget->fg_set)
        return widget->fg;
    if (!widget->enabled)
        return widget->app->theme.muted;
    return widget->app->theme.text;
}

static void draw_focus_rect(HDC dc, RECT rect)
{
    d2d_flush();
    InflateRect(&rect, -2, -2);
    DrawFocusRect(dc, &rect);
}

static int button_radius(WtkWidget *widget)
{
    int radius = widget->border_radius;
    int maximum = (widget->h - 2) / 2;
    if (radius < 0)
        radius = 0;
    if (maximum < 0)
        maximum = 0;
    if (radius > maximum)
        radius = maximum;
    return radius;
}

/* ------------------------------------------------------------------------- */
/* Individual widgets                                                        */
/* ------------------------------------------------------------------------- */

void wtk__paint_window_background(WtkWidget *window, HDC dc,
                                  const RECT *bounds)
{
    WtkColor color;
    if (window->app->style_painter &&
        window->app->style_painter->paint) {
        WtkPaintState state;
        wtk__build_paint_state(window, &state);
        window->app->style_painter->paint(
            window, dc, bounds, &state, window->app->style_painter_data);
        return;
    }
    color = window->bg_set ? window->bg : window->app->theme.window_bg;
    if (window->app->renderer == WTK_RENDERER_D2D &&
        d2d_begin(window->app, dc, window->w, window->h)) {
        d2d_fill_rect(*bounds, color);
        d2d_end();
        return;
    }
    fill_rect(dc, *bounds, color);
}

static void paint_container(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkColor backdrop = backdrop_color(widget);
    RECT rc = *bounds;
    WtkColor fill = widget->bg_set ? widget->bg : backdrop;

    fill_rect(dc, rc, backdrop);

    if (widget->border_set && (widget->border_width > 0 ||
                               widget->border_radius > 0)) {
        draw_round_rect(dc, rc, widget->border_radius, widget->bg_set, fill,
                        widget->border_width, widget->border);
    } else if (widget->bg_set) {
        fill_rect(dc, rc, widget->bg);
    }
}

static void paint_label(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkColor bg = wtk__widget_bg(widget);
    WtkColor fg = label_text_color(widget);
    RECT content = *bounds;
    int has_newline = widget->text && wcschr(widget->text, L'\n') != NULL;

    fill_rect(dc, content, bg);
    content.left += widget->pad_l;
    content.top += widget->pad_t;
    content.right -= widget->pad_r;
    content.bottom -= widget->pad_b;
    if (content.right <= content.left || content.bottom <= content.top)
        return;

    if (!has_newline && widget->wrap_width <= 0) {
        draw_text_single(dc, widget->font, fg, widget->text, content,
                         widget->align, 1);
    } else {
        draw_text_block(dc, widget->font, fg, widget->text, content,
                        widget->align);
    }
    if (widget->focused)
        draw_focus_rect(dc, content);
}

static void button_colors(WtkWidget *widget, WtkColor *fill, WtkColor *text,
                          WtkColor *border, int *border_width)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    WtkColor base;
    WtkColor hover;
    WtkColor pressed;
    WtkColor fg;
    WtkColor edge;
    int edge_width = 0;

    switch (widget->button_style) {
    case WTK_BUTTON_PRIMARY:
        base = theme->accent;
        hover = theme->accent_hover;
        pressed = theme->accent_pressed;
        fg = theme->on_accent;
        edge = base;
        break;
    case WTK_BUTTON_GHOST:
        base = backdrop_color(widget);
        hover = theme->surface_alt;
        pressed = wtk__shade(theme->surface_alt, -8);
        fg = theme->accent;
        edge = hover;
        break;
    case WTK_BUTTON_DANGER:
        base = theme->danger;
        hover = wtk__shade(base, 12);
        pressed = wtk__shade(base, -16);
        fg = theme->on_accent;
        edge = base;
        break;
    case WTK_BUTTON_SECONDARY:
    default:
        base = theme->surface;
        hover = theme->surface_alt;
        pressed = wtk__shade(theme->surface_alt, -8);
        fg = theme->text;
        edge = theme->border;
        edge_width = wtk__scale_widget(widget, 1);
        break;
    }

    if (widget->bg_set) {
        base = widget->bg;
        hover = wtk__shade(base, 8);
        pressed = wtk__shade(base, -12);
        edge = wtk__shade(base, -10);
        if (widget->button_style != WTK_BUTTON_GHOST &&
            widget->button_style != WTK_BUTTON_SECONDARY)
            edge_width = 0;
    }
    if (widget->fg_set)
        fg = widget->fg;
    if (widget->border_set) {
        edge = widget->border;
        edge_width = widget->border_width;
    }

    if (!widget->enabled) {
        base = theme->surface_alt;
        hover = base;
        pressed = base;
        fg = theme->muted;
        edge = theme->border;
        edge_width = wtk__scale_widget(widget, 1);
    }

    if (widget->pressed)
        base = pressed;
    else if (widget->hover)
        base = hover;

    *fill = base;
    *text = fg;
    *border = edge;
    *border_width = edge_width;
}

static void paint_button(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkColor backdrop = backdrop_color(widget);
    WtkColor fill;
    WtkColor text;
    WtkColor border;
    int border_width;
    RECT rc = *bounds;
    RECT content;
    int radius;

    button_colors(widget, &fill, &text, &border, &border_width);
    fill_rect(dc, rc, backdrop);
    radius = button_radius(widget);
    draw_round_rect(dc, rc, radius, 1, fill, border_width, border);

    content = rc;
    content.left += widget->pad_l;
    content.right -= widget->pad_r;
    content.top += widget->pad_t;
    content.bottom -= widget->pad_b;
    draw_text_single(dc, widget->font, text,
                     widget->text ? widget->text : L"", content,
                     widget->align, 1);
    if (widget->focused)
        draw_focus_rect(dc, rc);
}

static void paint_checkbox(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    RECT rc = *bounds;
    RECT box;
    RECT text_rect;
    int box_size = wtk__scale_widget(widget, 18);
    int gap = wtk__scale_widget(widget, 9);
    int box_left;
    int box_top;
    WtkColor fg = label_text_color(widget);
    WtkColor fill = theme->surface;
    WtkColor border = theme->border;
    int border_width = wtk__scale_widget(widget, 1);

    fill_rect(dc, rc, backdrop_color(widget));

    box_left = rc.left + widget->pad_l;
    box_top = rc.top + (rc.bottom - rc.top - box_size) / 2;
    box.left = box_left;
    box.top = box_top;
    box.right = box_left + box_size;
    box.bottom = box_top + box_size;

    if (widget->checked) {
        fill = theme->accent;
        border = theme->accent;
    } else if (widget->hover || widget->focused) {
        border = theme->accent;
    }
    if (!widget->enabled) {
        fill = theme->surface_alt;
        border = theme->border;
    }

    draw_round_rect(dc, box, wtk__scale_widget(widget, 4), 1, fill,
                    border_width, border);

    if (widget->checked) {
        POINT pts[3];
        pts[0].x = box.left + box_size * 26 / 100;
        pts[0].y = box.top + box_size * 52 / 100;
        pts[1].x = box.left + box_size * 43 / 100;
        pts[1].y = box.top + box_size * 70 / 100;
        pts[2].x = box.left + box_size * 75 / 100;
        pts[2].y = box.top + box_size * 30 / 100;
        draw_polyline(dc, pts, 3,
                      widget->enabled ? theme->on_accent : theme->muted,
                      wtk__scale_widget(widget, 2));
    }

    text_rect = rc;
    text_rect.left = box.right + gap;
    text_rect.right -= widget->pad_r;
    text_rect.top += widget->pad_t;
    text_rect.bottom -= widget->pad_b;
    draw_text_single(dc, widget->font, fg,
                     widget->text ? widget->text : L"", text_rect,
                     WTK_ALIGN_LEFT, 1);
    if (widget->focused)
        draw_focus_rect(dc, rc);
}

static void paint_radio(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    RECT rc = *bounds;
    RECT circle;
    RECT inner;
    RECT text_rect;
    int size = wtk__scale_widget(widget, 18);
    int gap = wtk__scale_widget(widget, 9);
    int left = rc.left + widget->pad_l;
    int top = rc.top + (rc.bottom - rc.top - size) / 2;
    int selected = wtk__radio_selected(widget);
    WtkColor fg = label_text_color(widget);
    WtkColor border = theme->border;
    int border_width = wtk__scale_widget(widget, 1);

    fill_rect(dc, rc, backdrop_color(widget));

    circle.left = left;
    circle.top = top;
    circle.right = left + size;
    circle.bottom = top + size;

    if (!widget->enabled) {
        border = theme->border;
    } else if (selected || widget->hover || widget->focused) {
        border = theme->accent;
    }
    if (selected && widget->enabled)
        border_width = wtk__scale_widget(widget, 2);

    draw_ellipse(dc, circle, 1,
                 widget->enabled ? theme->surface : theme->surface_alt,
                 border_width, border);

    if (selected) {
        int inset = size * 30 / 100;
        inner.left = circle.left + inset;
        inner.top = circle.top + inset;
        inner.right = circle.right - inset;
        inner.bottom = circle.bottom - inset;
        draw_ellipse(dc, inner, 1,
                     widget->enabled ? theme->accent : theme->muted, 0,
                     theme->accent);
    }

    text_rect = rc;
    text_rect.left = circle.right + gap;
    text_rect.right -= widget->pad_r;
    text_rect.top += widget->pad_t;
    text_rect.bottom -= widget->pad_b;
    draw_text_single(dc, widget->font, fg,
                     widget->text ? widget->text : L"", text_rect,
                     WTK_ALIGN_LEFT, 1);
    if (widget->focused)
        draw_focus_rect(dc, rc);
}

static void paint_field(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    RECT rc = *bounds;
    WtkColor field_bg = wtk__field_bg(widget);
    WtkColor border = theme->border;
    int border_width = wtk__scale_widget(widget, 1);
    int radius = widget->border_radius;
    int maximum;

    fill_rect(dc, rc, backdrop_color(widget));

    if (!widget->enabled) {
        field_bg = theme->surface_alt;
        border = theme->border;
    } else if (widget->focused) {
        border = theme->accent;
        border_width = wtk__scale_widget(widget, 2);
    } else if (widget->border_set) {
        border = widget->border;
        border_width = widget->border_width;
    }

    maximum = (rc.bottom - rc.top) / 2;
    if (radius > maximum)
        radius = maximum;

    draw_round_rect(dc, rc, radius, 1, field_bg, border_width, border);
}

static void paint_slider(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    RECT rc = *bounds;
    int left = rc.left + wtk__scale_widget(widget, 12);
    int right = rc.right - wtk__scale_widget(widget, 12);
    int center_y = (rc.top + rc.bottom) / 2;
    int track_height = wtk__scale_widget(widget, 6);
    int thumb_radius = wtk__scale_widget(widget, 8);
    int value_range = widget->value_max - widget->value_min;
    double fraction = 0.0;
    int thumb_x;
    WtkColor accent = widget->enabled ? theme->accent : theme->muted;
    RECT track;
    RECT fill_track;
    RECT thumb;

    fill_rect(dc, rc, backdrop_color(widget));
    if (right <= left)
        return;
    if (value_range > 0)
        fraction = (double)(widget->value - widget->value_min) / value_range;
    if (fraction < 0.0)
        fraction = 0.0;
    if (fraction > 1.0)
        fraction = 1.0;
    thumb_x = left + (int)((right - left) * fraction + 0.5);
    if (widget->hover)
        thumb_radius += wtk__scale_widget(widget, 1);

    track.left = left;
    track.right = right;
    track.top = center_y - track_height / 2;
    track.bottom = center_y + (track_height + 1) / 2;
    draw_round_rect(dc, track, track_height / 2, 1, theme->surface_alt, 0,
                    theme->surface_alt);

    fill_track = track;
    fill_track.right = thumb_x;
    if (fill_track.right > fill_track.left)
        draw_round_rect(dc, fill_track, track_height / 2, 1, accent, 0,
                        accent);

    thumb.left = thumb_x - thumb_radius;
    thumb.top = center_y - thumb_radius;
    thumb.right = thumb_x + thumb_radius + 1;
    thumb.bottom = center_y + thumb_radius + 1;
    draw_ellipse(dc, thumb, 1,
                 widget->enabled ? theme->surface : theme->surface_alt,
                 wtk__scale_widget(widget, 2), accent);
    if (widget->focused)
        draw_focus_rect(dc, rc);
}

static void paint_progress(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    WtkApp *app = widget->app;
    WtkTheme *theme = &app->theme;
    RECT rc = *bounds;
    int track_h;
    RECT track;
    RECT fill_track;
    double fraction = widget->progress;

    if (fraction < 0.0)
        fraction = 0.0;
    if (fraction > 1.0)
        fraction = 1.0;
    fill_rect(dc, rc, backdrop_color(widget));

    if (widget->show_percent) {
        track_h = wtk__scale_widget(widget, 10);
        if (track_h > (rc.bottom - rc.top) / 2)
            track_h = (rc.bottom - rc.top) / 2;
        track.left = rc.left;
        track.right = rc.right;
        track.top = rc.top;
        track.bottom = rc.top + track_h;
    } else {
        track = rc;
        track_h = rc.bottom - rc.top;
    }

    draw_round_rect(dc, track, track_h / 2, 1, theme->surface_alt, 0,
                    theme->surface_alt);

    fill_track = track;
    fill_track.right = track.left +
                       (int)((track.right - track.left) * fraction + 0.5);
    if (fill_track.right > fill_track.left) {
        int radius = track_h / 2;
        draw_round_rect(dc, fill_track, radius, 1,
                        widget->enabled ? theme->accent : theme->muted, 0,
                        theme->accent);
    }

    if (widget->show_percent) {
        WCHAR number[16];
        RECT text_rect = rc;
        int percent = (int)(fraction * 100.0 + 0.5);
        text_rect.top = track.bottom + wtk__scale_widget(widget, 2);
        swprintf(number, sizeof(number) / sizeof(number[0]), L"%d%%", percent);
        draw_text_single(dc, widget->font, label_text_color(widget), number,
                         text_rect, WTK_ALIGN_CENTER, 0);
    }
}

static void paint_separator(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    RECT rc = *bounds;
    int center_y = (rc.top + rc.bottom) / 2;
    WtkColor color = widget->border_set ? widget->border
                                        : widget->app->theme.border;
    int thickness = widget->border_width > 0
                        ? widget->border_width
                        : wtk__scale_widget(widget, 1);
    fill_rect(dc, rc, backdrop_color(widget));
    rc.left += widget->pad_l;
    rc.right -= widget->pad_r;
    rc.top = center_y;
    rc.bottom = center_y + (thickness > 0 ? thickness : 1);
    fill_rect(dc, rc, color);
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

void wtk__paint_widget(WtkWidget *widget, HDC dc, const RECT *bounds)
{
    if (!widget)
        return;
    if (widget->type != WTK_TYPE_CANVAS &&
        widget->app->style_painter &&
        widget->app->style_painter->paint) {
        WtkPaintState state;
        wtk__build_paint_state(widget, &state);
        widget->app->style_painter->paint(
            widget, dc, bounds, &state,
            widget->app->style_painter_data);
        return;
    }
    if (widget->type != WTK_TYPE_WINDOW && widget->type != WTK_TYPE_CANVAS &&
        widget->app->renderer == WTK_RENDERER_D2D)
        d2d_begin(widget->app, dc, widget->w, widget->h);
    switch (widget->type) {
    case WTK_TYPE_WINDOW:
        wtk__paint_window_background(widget, dc, bounds);
        break;
    case WTK_TYPE_FRAME:
    case WTK_TYPE_RADIOGROUP:
        paint_container(widget, dc, bounds);
        break;
    case WTK_TYPE_LABEL:
        paint_label(widget, dc, bounds);
        break;
    case WTK_TYPE_BUTTON:
        paint_button(widget, dc, bounds);
        break;
    case WTK_TYPE_ENTRY:
    case WTK_TYPE_TEXT:
    case WTK_TYPE_LISTBOX:
        paint_field(widget, dc, bounds);
        break;
    case WTK_TYPE_CHECKBOX:
        paint_checkbox(widget, dc, bounds);
        break;
    case WTK_TYPE_RADIO:
        paint_radio(widget, dc, bounds);
        break;
    case WTK_TYPE_SLIDER:
        paint_slider(widget, dc, bounds);
        break;
    case WTK_TYPE_PROGRESS:
        paint_progress(widget, dc, bounds);
        break;
    case WTK_TYPE_SEPARATOR:
        paint_separator(widget, dc, bounds);
        break;
    case WTK_TYPE_CANVAS:
        paint_canvas(widget, dc, bounds);
        break;
    default:
        fill_rect(dc, *bounds, backdrop_color(widget));
        break;
    }
    d2d_end();
}
