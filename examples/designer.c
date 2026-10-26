/*
 * WinTk Designer - a small visual page editor built with WinTk itself.
 *
 * Drag a control from the palette onto the page, move/resize it on the canvas,
 * edit its core properties, then export a standalone C source file.
 */
#include "wintk.h"
#include "designer_model.h"

#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define DESIGNER_WINDOW_W 1440
#define DESIGNER_WINDOW_H 900
#define DESIGNER_MIN_W 1200
#define DESIGNER_MIN_H 840
#define DESIGNER_GRID 8
#define DESIGNER_CODE_CAP 65536

#define HANDLE_LEFT 1
#define HANDLE_RIGHT 2
#define HANDLE_TOP 4
#define HANDLE_BOTTOM 8

#define ACTION_NONE 0
#define ACTION_MOVE 1
#define ACTION_RESIZE 2

typedef struct Designer Designer;

typedef struct PreviewFont {
    int pt;
    int bold;
    HFONT font;
} PreviewFont;

typedef struct DrawContext {
    Designer *designer;
    HDC dc;
    const WtkTheme *theme;
    double scale;
    PreviewFont fonts[24];
    int font_count;
} DrawContext;

struct Designer {
    WtkApp *app;
    WtkWidget *window;
    WtkWidget *palette;
    WtkWidget *canvas;
    WtkWidget *code_view;
    WtkWidget *status_label;
    WtkWidget *page_caption;

    WtkWidget *type_value;
    WtkWidget *name_entry;
    WtkWidget *text_entry;
    WtkWidget *x_entry;
    WtkWidget *y_entry;
    WtkWidget *w_entry;
    WtkWidget *h_entry;
    WtkWidget *font_entry;
    WtkWidget *bold_check;
    WtkWidget *checked_check;
    WtkWidget *primary_check;
    WtkWidget *apply_button;
    WtkWidget *delete_button;

    WtkWidget *clear_button;
    WtkWidget *preview_button;
    WtkWidget *export_button;

    DesignDoc doc;
    int selected;

    WCHAR font_family[LF_FACESIZE];

    double view_scale;
    int view_ox;
    int view_oy;

    int palette_hover;
    int palette_pressed;
    int palette_dragging;
    DesignKind palette_kind;
    POINT palette_start;

    int drop_active;
    DesignKind drop_kind;
    int drop_x;
    int drop_y;

    int action;
    int action_handle;
    int action_index;
    int action_start_x;
    int action_start_y;
    int action_changed;
    DesignItem action_orig;

    char code[DESIGNER_CODE_CAP];
};

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static int clampi(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int iround(double value)
{
    return (int)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

static int snap_grid(int value)
{
    if (value >= 0)
        return ((value + DESIGNER_GRID / 2) / DESIGNER_GRID) * DESIGNER_GRID;
    return -(((-value + DESIGNER_GRID / 2) / DESIGNER_GRID) * DESIGNER_GRID);
}

static COLORREF color_ref(WtkColor color)
{
    return RGB((BYTE)((color >> 16) & 0xFF), (BYTE)((color >> 8) & 0xFF),
               (BYTE)(color & 0xFF));
}

static void fill_color(HDC dc, const RECT *rect, WtkColor color)
{
    HBRUSH brush = CreateSolidBrush(color_ref(color));
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void round_rect_color(HDC dc, RECT rect, int radius, WtkColor fill,
                             WtkColor border, int border_width)
{
    HBRUSH brush = CreateSolidBrush(color_ref(fill));
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;

    if (border_width <= 0) {
        pen = CreatePen(PS_NULL, 0, color_ref(border));
        border_width = 0;
    } else {
        pen = CreatePen(PS_SOLID, border_width, color_ref(border));
    }
    old_brush = SelectObject(dc, brush);
    old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom,
              radius * 2, radius * 2);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void line_color(HDC dc, int x1, int y1, int x2, int y2, WtkColor color,
                       int width)
{
    HPEN pen = CreatePen(PS_SOLID, width > 0 ? width : 1, color_ref(color));
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void dashed_rect(HDC dc, RECT rect, WtkColor color)
{
    HPEN pen = CreatePen(PS_DASH, 1, color_ref(color));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    Rectangle(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void utf8_to_wide(const char *text, WCHAR *out, int capacity)
{
    int result;
    if (!out || capacity <= 0)
        return;
    out[0] = 0;
    if (!text)
        return;
    result = MultiByteToWideChar(CP_UTF8, 0, text, -1, out, capacity);
    if (result <= 0)
        out[0] = 0;
}

static void wide_to_utf8(const WCHAR *text, char *out, int capacity)
{
    int result;
    if (!out || capacity <= 0)
        return;
    out[0] = 0;
    if (!text)
        return;
    result = WideCharToMultiByte(CP_UTF8, 0, text, -1, out, capacity, NULL,
                                 NULL);
    if (result <= 0)
        out[0] = 0;
}

static void designer_status(Designer *designer, const char *text)
{
    if (designer && designer->status_label)
        wtk_label_set_text(designer->status_label, text ? text : "");
}

/* ------------------------------------------------------------------------- */
/* Preview drawing                                                           */
/* ------------------------------------------------------------------------- */

static void designer_clamp_item(Designer *designer, DesignItem *item)
{
    int min_w = 8;
    int min_h = 8;
    int max_w;
    int max_h;

    if (item->kind == DK_SEPARATOR) {
        min_h = 1;
        min_w = 8;
    } else if (item->kind == DK_PROGRESS) {
        min_h = 6;
    }
    max_w = designer->doc.page_w;
    max_h = designer->doc.page_h;
    item->w = clampi(item->w, min_w, max_w);
    item->h = clampi(item->h, min_h, max_h);
    item->x = clampi(item->x, 0, designer->doc.page_w - item->w);
    item->y = clampi(item->y, 0, designer->doc.page_h - item->h);
}

static int designer_view(Designer *designer, RECT *page_rect, double *scale_out)
{
    RECT client;
    double dpi_scale;
    double margin;
    double fit_x;
    double fit_y;
    double fit;
    int page_w;
    int page_h;

    if (!designer || !designer->canvas)
        return 0;
    GetClientRect(wtk_widget_hwnd(designer->canvas), &client);
    if (client.right - client.left < 20 || client.bottom - client.top < 20)
        return 0;

    dpi_scale = (double)wtk_app_dpi(designer->app) / 96.0;
    margin = 22.0 * dpi_scale;
    if (margin < 10.0)
        margin = 10.0;
    fit_x = ((double)(client.right - client.left) - margin * 2.0) /
            ((double)designer->doc.page_w * dpi_scale);
    fit_y = ((double)(client.bottom - client.top) - margin * 2.0) /
            ((double)designer->doc.page_h * dpi_scale);
    fit = fit_x < fit_y ? fit_x : fit_y;
    if (fit > 1.0)
        fit = 1.0;
    if (fit < 0.20)
        fit = 0.20;

    designer->view_scale = dpi_scale * fit;
    page_w = iround((double)designer->doc.page_w * designer->view_scale);
    page_h = iround((double)designer->doc.page_h * designer->view_scale);
    if (page_w < 1)
        page_w = 1;
    if (page_h < 1)
        page_h = 1;
    designer->view_ox = (client.right - client.left - page_w) / 2;
    designer->view_oy = (client.bottom - client.top - page_h) / 2;
    if (designer->view_ox < 0)
        designer->view_ox = 0;
    if (designer->view_oy < 0)
        designer->view_oy = 0;

    if (page_rect) {
        page_rect->left = designer->view_ox;
        page_rect->top = designer->view_oy;
        page_rect->right = designer->view_ox + page_w;
        page_rect->bottom = designer->view_oy + page_h;
    }
    if (scale_out)
        *scale_out = designer->view_scale;
    return 1;
}

static void designer_page_point(Designer *designer, int mx, int my, int *x,
                                int *y)
{
    *x = iround(((double)mx - designer->view_ox) / designer->view_scale);
    *y = iround(((double)my - designer->view_oy) / designer->view_scale);
}

static int designer_event_page(Designer *designer, int mx, int my, int *x,
                               int *y)
{
    RECT page;
    double scale;
    if (!designer_view(designer, &page, &scale))
        return 0;
    if (mx < page.left || mx > page.right || my < page.top ||
        my > page.bottom)
        return 0;
    designer_page_point(designer, mx, my, x, y);
    return 1;
}

static HFONT preview_font(DrawContext *ctx, int pt, int bold)
{
    int i;
    int effective_dpi;
    LOGFONTW lf;
    HFONT font;

    for (i = 0; i < ctx->font_count; ++i) {
        if (ctx->fonts[i].pt == pt && ctx->fonts[i].bold == bold)
            return ctx->fonts[i].font;
    }
    if (ctx->font_count >= (int)(sizeof(ctx->fonts) / sizeof(ctx->fonts[0])))
        return NULL;

    effective_dpi = iround(96.0 * ctx->scale);
    if (effective_dpi < 40)
        effective_dpi = 40;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(pt, effective_dpi, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, ctx->designer->font_family);
    font = CreateFontIndirectW(&lf);
    if (!font)
        return NULL;
    ctx->fonts[ctx->font_count].pt = pt;
    ctx->fonts[ctx->font_count].bold = bold;
    ctx->fonts[ctx->font_count].font = font;
    ctx->font_count++;
    return font;
}

static void draw_text_utf8(DrawContext *ctx, const char *text, RECT rect,
                           WtkColor color, int pt, int bold, UINT format)
{
    WCHAR wide[1024];
    HFONT font;
    HGDIOBJ old_font;

    utf8_to_wide(text, wide, 1024);
    if (!wide[0])
        return;
    font = preview_font(ctx, pt, bold);
    if (!font)
        return;
    old_font = SelectObject(ctx->dc, font);
    SetBkMode(ctx->dc, TRANSPARENT);
    SetTextColor(ctx->dc, color_ref(color));
    DrawTextW(ctx->dc, wide, -1, &rect, format | DT_NOPREFIX);
    SelectObject(ctx->dc, old_font);
}

static RECT item_rect(Designer *designer, const DesignItem *item)
{
    RECT rect;
    rect.left = designer->view_ox +
                iround((double)item->x * designer->view_scale);
    rect.top = designer->view_oy +
               iround((double)item->y * designer->view_scale);
    rect.right = designer->view_ox +
                 iround((double)(item->x + item->w) * designer->view_scale);
    rect.bottom = designer->view_oy +
                  iround((double)(item->y + item->h) * designer->view_scale);
    return rect;
}

static int sc(Designer *designer, int logical)
{
    return iround((double)logical * designer->view_scale);
}

static void draw_preview_label(DrawContext *ctx, const DesignItem *item,
                               RECT rect)
{
    RECT text = rect;
    text.left += sc(ctx->designer, 2);
    text.right -= sc(ctx->designer, 2);
    draw_text_utf8(ctx, item->text, text, ctx->theme->text, item->font_size,
                   item->bold,
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
}

static void draw_preview_button(DrawContext *ctx, const DesignItem *item,
                                RECT rect)
{
    WtkColor fill = item->primary ? ctx->theme->accent : ctx->theme->surface;
    WtkColor border =
        item->primary ? ctx->theme->accent : ctx->theme->border;
    WtkColor text = item->primary ? ctx->theme->on_accent : ctx->theme->text;
    int radius = sc(ctx->designer, 8);
    RECT label = rect;
    label.left += sc(ctx->designer, 10);
    label.right -= sc(ctx->designer, 10);

    round_rect_color(ctx->dc, rect, radius, fill, border, 1);
    draw_text_utf8(ctx, item->text, label, text, item->font_size, item->bold,
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER |
                       DT_END_ELLIPSIS);
}

static void draw_field(DrawContext *ctx, RECT rect)
{
    round_rect_color(ctx->dc, rect, sc(ctx->designer, 8),
                     ctx->theme->surface, ctx->theme->border, 1);
}

static void draw_preview_entry(DrawContext *ctx, const DesignItem *item,
                               RECT rect)
{
    RECT text = rect;
    text.left += sc(ctx->designer, 10);
    text.right -= sc(ctx->designer, 10);
    text.top += sc(ctx->designer, 5);
    text.bottom -= sc(ctx->designer, 5);

    draw_field(ctx, rect);
    if (item->text[0]) {
        draw_text_utf8(ctx, item->text, text, ctx->theme->text,
                       item->font_size, item->bold,
                       DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                           DT_END_ELLIPSIS);
    } else {
        draw_text_utf8(ctx, designer_kind_placeholder(DK_ENTRY), text,
                       ctx->theme->muted, item->font_size, item->bold,
                       DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                           DT_END_ELLIPSIS);
    }
}

static void draw_preview_text(DrawContext *ctx, const DesignItem *item,
                              RECT rect)
{
    RECT text = rect;
    RECT rail;

    draw_field(ctx, rect);
    rail = rect;
    rail.left = rail.right - sc(ctx->designer, 9);
    rail.top += 1;
    rail.bottom -= 1;
    fill_color(ctx->dc, &rail, ctx->theme->surface_alt);
    text.left += sc(ctx->designer, 10);
    text.right -= sc(ctx->designer, 16);
    text.top += sc(ctx->designer, 8);
    text.bottom -= sc(ctx->designer, 8);
    if (item->text[0]) {
        draw_text_utf8(ctx, item->text, text, ctx->theme->text,
                       item->font_size, item->bold,
                       DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL);
    } else {
        draw_text_utf8(ctx, designer_kind_placeholder(DK_TEXT), text,
                       ctx->theme->muted, item->font_size, item->bold,
                       DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL);
    }
}

static void draw_preview_checkbox(DrawContext *ctx, const DesignItem *item,
                                  RECT rect)
{
    int max_box = (rect.bottom - rect.top) - sc(ctx->designer, 2);
    int box_size;
    int box_top;
    if (max_box < 10)
        max_box = 10;
    box_size = clampi(sc(ctx->designer, 18), 10, max_box);
    box_top = rect.top + ((rect.bottom - rect.top) - box_size) / 2;
    RECT box;
    RECT text = rect;
    WtkColor border = item->checked ? ctx->theme->accent : ctx->theme->border;
    WtkColor fill = item->checked ? ctx->theme->accent : ctx->theme->surface;

    box.left = rect.left;
    box.top = box_top;
    box.right = box.left + box_size;
    box.bottom = box.top + box_size;
    round_rect_color(ctx->dc, box, sc(ctx->designer, 4), fill, border, 1);
    if (item->checked) {
        int inset = box_size / 4;
        line_color(ctx->dc, box.left + inset, box.top + box_size / 2,
                   box.left + box_size / 2, box.bottom - inset,
                   ctx->theme->on_accent, 2);
        line_color(ctx->dc, box.left + box_size / 2, box.bottom - inset,
                   box.right - inset, box.top + inset, ctx->theme->on_accent,
                   2);
    }
    text.left = box.right + sc(ctx->designer, 8);
    draw_text_utf8(ctx, item->text, text, ctx->theme->text, item->font_size,
                   item->bold, DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                                   DT_END_ELLIPSIS);
}

static void draw_preview_listbox(DrawContext *ctx, const DesignItem *item,
                                 RECT rect)
{
    static const char *const items[] = { "项目一", "项目二", "项目三" };
    int count = (int)(sizeof(items) / sizeof(items[0]));
    int row_h = sc(ctx->designer, 28);
    int i;
    RECT inner = rect;
    int saved;

    draw_field(ctx, rect);
    inner.left += 2;
    inner.right -= 2;
    inner.top += 2;
    inner.bottom -= 2;
    saved = SaveDC(ctx->dc);
    IntersectClipRect(ctx->dc, inner.left, inner.top, inner.right,
                      inner.bottom);
    for (i = 0; i < count; ++i) {
        RECT row;
        row.left = inner.left;
        row.right = inner.right;
        row.top = inner.top + i * row_h;
        row.bottom = row.top + row_h;
        if (row.top >= inner.bottom)
            break;
        if (i == 0) {
            fill_color(ctx->dc, &row, ctx->theme->accent);
            row.left += sc(ctx->designer, 8);
            draw_text_utf8(ctx, items[i], row, ctx->theme->on_accent,
                           item->font_size, item->bold,
                           DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                               DT_END_ELLIPSIS);
        } else {
            row.left += sc(ctx->designer, 8);
            draw_text_utf8(ctx, items[i], row, ctx->theme->text,
                           item->font_size, item->bold,
                           DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                               DT_END_ELLIPSIS);
        }
    }
    RestoreDC(ctx->dc, saved);
}

static void draw_preview_slider(DrawContext *ctx, const DesignItem *item,
                                RECT rect)
{
    int track_h = clampi(sc(ctx->designer, 6), 3, rect.bottom - rect.top);
    int cy = (rect.top + rect.bottom) / 2;
    int thumb = clampi(sc(ctx->designer, 16), 8, rect.bottom - rect.top);
    int half_x = rect.left + (rect.right - rect.left) / 2;
    RECT track;
    RECT fill;
    HBRUSH brush;
    HGDIOBJ old;
    HPEN pen;

    track.left = rect.left;
    track.right = rect.right;
    track.top = cy - track_h / 2;
    track.bottom = cy + track_h / 2;
    fill = track;
    fill.right = half_x;
    round_rect_color(ctx->dc, track, track_h / 2, ctx->theme->surface_alt,
                     ctx->theme->surface_alt, 0);
    round_rect_color(ctx->dc, fill, track_h / 2, ctx->theme->accent,
                     ctx->theme->accent, 0);

    brush = CreateSolidBrush(color_ref(ctx->theme->surface));
    pen = CreatePen(PS_SOLID, 1, color_ref(ctx->theme->accent));
    old = SelectObject(ctx->dc, brush);
    {
        HGDIOBJ old_pen = SelectObject(ctx->dc, pen);
        Ellipse(ctx->dc, half_x - thumb / 2, cy - thumb / 2,
                half_x + thumb / 2 + 1, cy + thumb / 2 + 1);
        SelectObject(ctx->dc, old_pen);
    }
    SelectObject(ctx->dc, old);
    DeleteObject(brush);
    DeleteObject(pen);
    (void)item;
}

static void draw_preview_progress(DrawContext *ctx, const DesignItem *item,
                                  RECT rect)
{
    RECT fill = rect;
    int radius = clampi((rect.bottom - rect.top) / 2, 2,
                        sc(ctx->designer, 8));
    fill.right = rect.left + (rect.right - rect.left) / 2;

    round_rect_color(ctx->dc, rect, radius, ctx->theme->surface_alt,
                     ctx->theme->surface_alt, 0);
    round_rect_color(ctx->dc, fill, radius, ctx->theme->accent,
                     ctx->theme->accent, 0);
    (void)item;
}

static void draw_preview_separator(DrawContext *ctx, RECT rect)
{
    int y = (rect.top + rect.bottom) / 2;
    line_color(ctx->dc, rect.left, y, rect.right, y, ctx->theme->border, 1);
}

static void draw_preview_frame(DrawContext *ctx, RECT rect)
{
    round_rect_color(ctx->dc, rect, sc(ctx->designer, 8),
                     ctx->theme->surface_alt, ctx->theme->border, 1);
}

static void draw_preview_canvas(DrawContext *ctx, RECT rect)
{
    RECT inner = rect;
    int step = clampi(sc(ctx->designer, 18), 12, 48);
    int x;
    int y;
    int saved;

    round_rect_color(ctx->dc, rect, sc(ctx->designer, 6),
                     WTK_RGB(255, 255, 255), ctx->theme->border, 1);
    inner.left += 1;
    inner.right -= 1;
    inner.top += 1;
    inner.bottom -= 1;
    saved = SaveDC(ctx->dc);
    IntersectClipRect(ctx->dc, inner.left, inner.top, inner.right,
                      inner.bottom);
    for (x = inner.left + step; x < inner.right; x += step)
        line_color(ctx->dc, x, inner.top, x - step, inner.bottom,
                   WTK_RGB(238, 242, 249), 1);
    for (y = inner.top + step; y < inner.bottom; y += step)
        line_color(ctx->dc, inner.left, y, inner.right, y,
                   WTK_RGB(238, 242, 249), 1);
    RestoreDC(ctx->dc, saved);
    {
        RECT text = rect;
        draw_text_utf8(ctx, "画布", text, ctx->theme->muted, 9, 0,
                       DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
}

static void draw_preview_item(DrawContext *ctx, const DesignItem *item)
{
    RECT rect = item_rect(ctx->designer, item);
    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return;

    switch (item->kind) {
    case DK_LABEL:
        draw_preview_label(ctx, item, rect);
        break;
    case DK_BUTTON:
        draw_preview_button(ctx, item, rect);
        break;
    case DK_ENTRY:
        draw_preview_entry(ctx, item, rect);
        break;
    case DK_TEXT:
        draw_preview_text(ctx, item, rect);
        break;
    case DK_CHECKBOX:
        draw_preview_checkbox(ctx, item, rect);
        break;
    case DK_LISTBOX:
        draw_preview_listbox(ctx, item, rect);
        break;
    case DK_SLIDER:
        draw_preview_slider(ctx, item, rect);
        break;
    case DK_PROGRESS:
        draw_preview_progress(ctx, item, rect);
        break;
    case DK_SEPARATOR:
        draw_preview_separator(ctx, rect);
        break;
    case DK_FRAME:
        draw_preview_frame(ctx, rect);
        break;
    case DK_CANVAS:
        draw_preview_canvas(ctx, rect);
        break;
    default:
        break;
    }
}

static POINT handle_point(RECT rect, int handle)
{
    POINT point;
    point.x = (rect.left + rect.right) / 2;
    point.y = (rect.top + rect.bottom) / 2;
    if (handle & HANDLE_LEFT)
        point.x = rect.left;
    else if (handle & HANDLE_RIGHT)
        point.x = rect.right;
    if (handle & HANDLE_TOP)
        point.y = rect.top;
    else if (handle & HANDLE_BOTTOM)
        point.y = rect.bottom;
    return point;
}

static void draw_selection(DrawContext *ctx, const DesignItem *item)
{
    static const int handles[] = {
        HANDLE_LEFT | HANDLE_TOP,    HANDLE_TOP,
        HANDLE_RIGHT | HANDLE_TOP,   HANDLE_RIGHT,
        HANDLE_RIGHT | HANDLE_BOTTOM, HANDLE_BOTTOM,
        HANDLE_LEFT | HANDLE_BOTTOM, HANDLE_LEFT
    };
    RECT rect = item_rect(ctx->designer, item);
    int i;
    int size = 6;

    dashed_rect(ctx->dc, rect, ctx->theme->accent);
    for (i = 0; i < (int)(sizeof(handles) / sizeof(handles[0])); ++i) {
        POINT point = handle_point(rect, handles[i]);
        RECT handle;
        handle.left = point.x - size / 2;
        handle.top = point.y - size / 2;
        handle.right = handle.left + size;
        handle.bottom = handle.top + size;
        fill_color(ctx->dc, &handle, ctx->theme->surface);
        {
            HPEN pen = CreatePen(PS_SOLID, 1, color_ref(ctx->theme->accent));
            HGDIOBJ old = SelectObject(ctx->dc, pen);
            HGDIOBJ old_brush = SelectObject(ctx->dc, GetStockObject(NULL_BRUSH));
            Rectangle(ctx->dc, handle.left, handle.top, handle.right,
                      handle.bottom);
            SelectObject(ctx->dc, old_brush);
            SelectObject(ctx->dc, old);
            DeleteObject(pen);
        }
    }
}

static void draw_drop_ghost(DrawContext *ctx)
{
    int index;
    RECT rect;
    char label[96];
    DesignItem preview;

    if (!ctx->designer->drop_active)
        return;
    index = (int)ctx->designer->drop_kind;
    if (index < 0 || index >= DK_COUNT)
        return;
    memset(&preview, 0, sizeof(preview));
    preview.kind = ctx->designer->drop_kind;
    preview.x = ctx->designer->drop_x;
    preview.y = ctx->designer->drop_y;
    preview.w = designer_palette[index].width;
    preview.h = designer_palette[index].height;
    rect = item_rect(ctx->designer, &preview);
    {
        RECT fill = rect;
        fill_color(ctx->dc, &fill, WTK_RGB(234, 242, 255));
    }
    dashed_rect(ctx->dc, rect, ctx->theme->accent);
    snprintf(label, sizeof(label), "%s  %d × %d", designer_kind_name(preview.kind),
             preview.w, preview.h);
    draw_text_utf8(ctx, label, rect, ctx->theme->accent, 9, 1,
                   DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

static void canvas_draw(WtkWidget *canvas, HDC dc, int width, int height,
                        void *userdata)
{
    Designer *designer = (Designer *)userdata;
    DrawContext ctx;
    RECT page;
    double scale;
    int i;
    int grid;
    WtkColor workspace = WTK_RGB(230, 235, 244);
    RECT full;

    (void)canvas;
    (void)width;
    (void)height;
    if (!designer)
        return;
    memset(&ctx, 0, sizeof(ctx));
    ctx.designer = designer;
    ctx.dc = dc;
    ctx.theme = wtk_app_get_theme(designer->app);
    if (!designer_view(designer, &page, &scale))
        return;
    ctx.scale = scale;

    full.left = 0;
    full.top = 0;
    full.right = width;
    full.bottom = height;
    fill_color(dc, &full, workspace);

    {
        RECT shadow = page;
        int offset = clampi(iround(5.0 * scale), 3, 8);
        shadow.left += offset;
        shadow.top += offset;
        shadow.right += offset;
        shadow.bottom += offset;
        fill_color(dc, &shadow, WTK_RGB(207, 216, 229));
    }
    fill_color(dc, &page, WTK_RGB(255, 255, 255));
    {
        HPEN pen = CreatePen(PS_SOLID, 1, color_ref(ctx.theme->border));
        HGDIOBJ old = SelectObject(dc, pen);
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, page.left, page.top, page.right, page.bottom);
        SelectObject(dc, old_brush);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    grid = iround(20.0 * scale);
    if (grid >= 8) {
        int x;
        int y;
        for (y = page.top + grid; y < page.bottom; y += grid) {
            for (x = page.left + grid; x < page.right; x += grid) {
                SetPixelV(dc, x, y, color_ref(WTK_RGB(226, 232, 242)));
            }
        }
    }

    {
        int saved = SaveDC(dc);
        IntersectClipRect(dc, page.left, page.top, page.right, page.bottom);
        for (i = 0; i < designer->doc.count; ++i)
            draw_preview_item(&ctx, &designer->doc.items[i]);
        RestoreDC(dc, saved);
    }

    if (designer->doc.count == 0) {
        RECT hint = page;
        hint.top = page.top + (page.bottom - page.top) / 2 -
                   iround(30.0 * scale);
        hint.bottom = hint.top + iround(60.0 * scale);
        draw_text_utf8(&ctx, "从左侧拖入控件", hint, ctx.theme->muted, 12, 1,
                       DT_SINGLELINE | DT_CENTER | DT_TOP);
        hint.top += iround(24.0 * scale);
        hint.bottom = hint.top + iround(24.0 * scale);
        draw_text_utf8(&ctx, "也可以单击调色板中的控件", hint, ctx.theme->muted,
                       9, 0, DT_SINGLELINE | DT_CENTER | DT_TOP);
    }

    draw_drop_ghost(&ctx);

    if (designer->selected >= 0 &&
        designer->selected < designer->doc.count) {
        draw_selection(&ctx, &designer->doc.items[designer->selected]);
    }

    for (i = 0; i < ctx.font_count; ++i)
        DeleteObject(ctx.fonts[i].font);
}

/* ------------------------------------------------------------------------- */
/* Document helpers                                                          */
/* ------------------------------------------------------------------------- */

static void designer_redraw(Designer *designer)
{
    if (designer && designer->canvas)
        wtk_canvas_redraw(designer->canvas);
}

static void designer_regenerate_code(Designer *designer)
{
    if (!designer)
        return;
    if (!designer_generate_c(&designer->doc, designer->code,
                             sizeof(designer->code))) {
        snprintf(designer->code, sizeof(designer->code),
                 "/* Code generation failed: buffer is too small. */\n");
    }
    if (designer->code_view)
        wtk_text_set_text(designer->code_view, designer->code);
}

static int designer_parse_int(const char *text, int fallback)
{
    char *end = NULL;
    long value;
    if (!text || !text[0])
        return fallback;
    value = strtol(text, &end, 10);
    if (end == text)
        return fallback;
    if (value > 1000000)
        value = 1000000;
    if (value < -1000000)
        value = -1000000;
    return (int)value;
}

static void entry_set_int(WtkWidget *entry, int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    wtk_entry_set_text(entry, text);
}

static void designer_sync_properties(Designer *designer)
{
    int has_item;
    int has_text;
    int has_font;

    if (!designer)
        return;
    has_item = designer->selected >= 0 &&
               designer->selected < designer->doc.count;
    if (!has_item) {
        wtk_label_set_text(designer->type_value, "页面");
        if (designer->name_entry) {
            wtk_entry_set_text(designer->name_entry, "");
            wtk_widget_set_enabled(designer->name_entry, 0);
        }
        if (designer->text_entry) {
            wtk_entry_set_text(designer->text_entry, designer->doc.title);
            wtk_widget_set_enabled(designer->text_entry, 1);
        }
        if (designer->x_entry) {
            wtk_entry_set_text(designer->x_entry, "0");
            wtk_widget_set_enabled(designer->x_entry, 0);
            wtk_entry_set_text(designer->y_entry, "0");
            wtk_widget_set_enabled(designer->y_entry, 0);
            entry_set_int(designer->w_entry, designer->doc.page_w);
            entry_set_int(designer->h_entry, designer->doc.page_h);
            wtk_widget_set_enabled(designer->w_entry, 1);
            wtk_widget_set_enabled(designer->h_entry, 1);
        }
        if (designer->font_entry) {
            wtk_entry_set_text(designer->font_entry, "");
            wtk_widget_set_enabled(designer->font_entry, 0);
        }
        if (designer->bold_check) {
            wtk_checkbox_set(designer->bold_check, 0);
            wtk_widget_set_enabled(designer->bold_check, 0);
            wtk_checkbox_set(designer->checked_check, 0);
            wtk_widget_set_enabled(designer->checked_check, 0);
            wtk_checkbox_set(designer->primary_check, 0);
            wtk_widget_set_enabled(designer->primary_check, 0);
        }
        if (designer->delete_button)
            wtk_widget_set_enabled(designer->delete_button, 0);
        return;
    }

    {
        DesignItem *item = &designer->doc.items[designer->selected];
        char title[128];
        char number[32];
        has_text = designer_kind_has_text(item->kind);
        has_font = designer_kind_has_font(item->kind);
        snprintf(title, sizeof(title), "%s · %s",
                 designer_kind_name(item->kind), item->name);
        wtk_label_set_text(designer->type_value, title);

        if (designer->name_entry) {
            wtk_entry_set_text(designer->name_entry, item->name);
            wtk_widget_set_enabled(designer->name_entry, 1);
        }
        if (designer->text_entry) {
            if (has_text)
                wtk_entry_set_text(designer->text_entry, item->text);
            else
                wtk_entry_set_text(designer->text_entry, "");
            wtk_widget_set_enabled(designer->text_entry, has_text);
        }
        if (designer->x_entry) {
            entry_set_int(designer->x_entry, item->x);
            entry_set_int(designer->y_entry, item->y);
            entry_set_int(designer->w_entry, item->w);
            entry_set_int(designer->h_entry, item->h);
            wtk_widget_set_enabled(designer->x_entry, 1);
            wtk_widget_set_enabled(designer->y_entry, 1);
            wtk_widget_set_enabled(designer->w_entry, 1);
            wtk_widget_set_enabled(designer->h_entry, 1);
        }
        if (designer->font_entry) {
            if (has_font) {
                snprintf(number, sizeof(number), "%d", item->font_size);
                wtk_entry_set_text(designer->font_entry, number);
            } else {
                wtk_entry_set_text(designer->font_entry, "");
            }
            wtk_widget_set_enabled(designer->font_entry, has_font);
        }
        if (designer->bold_check) {
            wtk_checkbox_set(designer->bold_check, item->bold);
            wtk_widget_set_enabled(designer->bold_check, has_font);
            wtk_checkbox_set(designer->checked_check, item->checked);
            wtk_widget_set_enabled(designer->checked_check,
                                   item->kind == DK_CHECKBOX);
            wtk_checkbox_set(designer->primary_check, item->primary);
            wtk_widget_set_enabled(designer->primary_check,
                                   item->kind == DK_BUTTON);
        }
        if (designer->delete_button)
            wtk_widget_set_enabled(designer->delete_button, 1);
    }
}

static void designer_update_geometry_fields(Designer *designer)
{
    DesignItem *item;
    char title[128];
    if (!designer || designer->selected < 0 ||
        designer->selected >= designer->doc.count || !designer->x_entry)
        return;
    item = &designer->doc.items[designer->selected];
    entry_set_int(designer->x_entry, item->x);
    entry_set_int(designer->y_entry, item->y);
    entry_set_int(designer->w_entry, item->w);
    entry_set_int(designer->h_entry, item->h);
    snprintf(title, sizeof(title), "%s · %s",
             designer_kind_name(item->kind), item->name);
    wtk_label_set_text(designer->type_value, title);
}

static void designer_select(Designer *designer, int index)
{
    if (!designer)
        return;
    if (index < -1 || index >= designer->doc.count)
        index = -1;
    designer->selected = index;
    designer_sync_properties(designer);
    designer_redraw(designer);
    if (index >= 0) {
        char status[160];
        DesignItem *item = &designer->doc.items[index];
        snprintf(status, sizeof(status), "已选择 %s · %s",
                 designer_kind_name(item->kind), item->name);
        designer_status(designer, status);
    } else {
        designer_status(designer, "就绪 · 拖拽左侧控件到页面");
    }
}

static void designer_add_kind(Designer *designer, DesignKind kind, int x,
                              int y)
{
    int index;
    int default_w;
    int default_h;

    if (!designer || kind < 0 || kind >= DK_COUNT)
        return;
    if (designer->doc.count >= DESIGN_MAX_ITEMS) {
        designer_status(designer, "控件数量已达到上限");
        return;
    }
    default_w = designer_palette[kind].width;
    default_h = designer_palette[kind].height;
    x = clampi(x, 0, designer->doc.page_w - default_w);
    y = clampi(y, 0, designer->doc.page_h - default_h);
    index = designer_add_item(&designer->doc, kind, x, y);
    if (index < 0)
        return;
    designer_clamp_item(designer, &designer->doc.items[index]);
    designer_regenerate_code(designer);
    designer_select(designer, index);
    designer_status(designer, "已添加控件，可拖动调整位置");
}

static void designer_delete_selected(Designer *designer)
{
    char status[128];
    if (!designer || designer->selected < 0 ||
        designer->selected >= designer->doc.count)
        return;
    snprintf(status, sizeof(status), "已删除 %s",
             designer_kind_name(designer->doc.items[designer->selected].kind));
    designer_remove_item(&designer->doc, designer->selected);
    if (designer->selected >= designer->doc.count)
        designer->selected = designer->doc.count - 1;
    designer_regenerate_code(designer);
    designer_sync_properties(designer);
    designer_redraw(designer);
    designer_status(designer, status);
}

static void designer_update_page_caption(Designer *designer)
{
    char caption[256];
    if (!designer || !designer->page_caption)
        return;
    snprintf(caption, sizeof(caption), "%s · %d × %d",
             designer->doc.title, designer->doc.page_w,
             designer->doc.page_h);
    wtk_label_set_text(designer->page_caption, caption);
}

static void designer_apply_properties(Designer *designer)
{
    int has_item;
    int has_text;
    int has_font;

    if (!designer)
        return;
    has_item = designer->selected >= 0 &&
               designer->selected < designer->doc.count;
    if (!has_item) {
        const char *title = wtk_entry_get_text(designer->text_entry);
        int new_w = clampi(designer_parse_int(
                               wtk_entry_get_text(designer->w_entry),
                               designer->doc.page_w),
                           320, 1600);
        int new_h = clampi(designer_parse_int(
                               wtk_entry_get_text(designer->h_entry),
                               designer->doc.page_h),
                           240, 1200);
        int i;
        if (!title || !title[0])
            title = "未命名页面";
        snprintf(designer->doc.title, sizeof(designer->doc.title), "%s",
                 title);
        designer->doc.page_w = new_w;
        designer->doc.page_h = new_h;
        for (i = 0; i < designer->doc.count; ++i)
            designer_clamp_item(designer, &designer->doc.items[i]);
        designer_sync_properties(designer);
        designer_update_page_caption(designer);
        designer_regenerate_code(designer);
        designer_redraw(designer);
        designer_status(designer, "已更新页面属性");
        return;
    }

    {
        DesignItem *item = &designer->doc.items[designer->selected];
        char requested[DESIGN_NAME_CAP];
        char unique[DESIGN_NAME_CAP];
        int x;
        int y;
        int w;
        int h;
        int min_w = 8;
        int min_h = 8;

        has_text = designer_kind_has_text(item->kind);
        has_font = designer_kind_has_font(item->kind);
        if (designer->name_entry) {
            snprintf(requested, sizeof(requested), "%s",
                     wtk_entry_get_text(designer->name_entry));
            designer_normalize_name(&designer->doc, designer->selected,
                                    requested, unique, sizeof(unique));
            snprintf(item->name, sizeof(item->name), "%s", unique);
        }
        if (has_text && designer->text_entry) {
            snprintf(item->text, sizeof(item->text), "%s",
                     wtk_entry_get_text(designer->text_entry));
        }
        if (item->kind == DK_SEPARATOR) {
            min_w = 8;
            min_h = 1;
        } else if (item->kind == DK_PROGRESS) {
            min_h = 6;
        }
        x = designer_parse_int(wtk_entry_get_text(designer->x_entry), item->x);
        y = designer_parse_int(wtk_entry_get_text(designer->y_entry), item->y);
        w = designer_parse_int(wtk_entry_get_text(designer->w_entry), item->w);
        h = designer_parse_int(wtk_entry_get_text(designer->h_entry), item->h);
        item->w = clampi(w, min_w, designer->doc.page_w);
        item->h = clampi(h, min_h, designer->doc.page_h);
        item->x = clampi(x, 0, designer->doc.page_w - item->w);
        item->y = clampi(y, 0, designer->doc.page_h - item->h);
        if (has_font) {
            int font_size = designer_parse_int(
                wtk_entry_get_text(designer->font_entry), item->font_size);
            item->font_size = clampi(font_size, 7, 72);
            item->bold = wtk_checkbox_get(designer->bold_check) ? 1 : 0;
        }
        if (item->kind == DK_CHECKBOX)
            item->checked = wtk_checkbox_get(designer->checked_check) ? 1 : 0;
        if (item->kind == DK_BUTTON)
            item->primary = wtk_checkbox_get(designer->primary_check) ? 1 : 0;

        designer_sync_properties(designer);
        designer_regenerate_code(designer);
        designer_redraw(designer);
        designer_status(designer, "已应用控件属性");
    }
}

static void on_apply_properties(WtkWidget *widget, WtkEvent *event, void *data)
{
    (void)widget;
    (void)event;
    designer_apply_properties((Designer *)data);
}

static void on_delete_click(WtkWidget *widget, WtkEvent *event, void *data)
{
    (void)widget;
    (void)event;
    designer_delete_selected((Designer *)data);
}

/* ------------------------------------------------------------------------- */
/* Canvas interaction                                                        */
/* ------------------------------------------------------------------------- */

static int handle_hit(Designer *designer, int mx, int my)
{
    static const int handles[] = {
        HANDLE_LEFT | HANDLE_TOP,    HANDLE_TOP,
        HANDLE_RIGHT | HANDLE_TOP,   HANDLE_RIGHT,
        HANDLE_RIGHT | HANDLE_BOTTOM, HANDLE_BOTTOM,
        HANDLE_LEFT | HANDLE_BOTTOM, HANDLE_LEFT
    };
    RECT rect;
    int i;
    if (!designer || designer->selected < 0 ||
        designer->selected >= designer->doc.count)
        return 0;
    rect = item_rect(designer, &designer->doc.items[designer->selected]);
    for (i = 0; i < (int)(sizeof(handles) / sizeof(handles[0])); ++i) {
        POINT point = handle_point(rect, handles[i]);
        if (abs(mx - point.x) <= 7 && abs(my - point.y) <= 7)
            return handles[i];
    }
    return 0;
}

static int item_hit(Designer *designer, int x, int y)
{
    int i;
    for (i = designer->doc.count - 1; i >= 0; --i) {
        DesignItem *item = &designer->doc.items[i];
        if (x >= item->x && x <= item->x + item->w && y >= item->y &&
            y <= item->y + item->h)
            return i;
    }
    return -1;
}

static void on_canvas_mouse_down(WtkWidget *widget, WtkEvent *event, void *data)
{
    Designer *designer = (Designer *)data;
    int handle;
    int page_x;
    int page_y;
    int hit;

    (void)widget;
    handle = handle_hit(designer, event->x, event->y);
    if (handle) {
        designer->action = ACTION_RESIZE;
        designer->action_handle = handle;
        designer->action_index = designer->selected;
        designer->action_orig = designer->doc.items[designer->selected];
        designer->action_start_x = event->x;
        designer->action_start_y = event->y;
        designer->action_changed = 0;
        SetCapture(wtk_widget_hwnd(designer->canvas));
        return;
    }
    if (!designer_event_page(designer, event->x, event->y, &page_x,
                             &page_y)) {
        designer_select(designer, -1);
        return;
    }
    hit = item_hit(designer, page_x, page_y);
    if (hit < 0) {
        designer_select(designer, -1);
        return;
    }
    designer_select(designer, hit);
    designer->action = ACTION_MOVE;
    designer->action_index = hit;
    designer->action_orig = designer->doc.items[hit];
    designer->action_start_x = event->x;
    designer->action_start_y = event->y;
    designer->action_changed = 0;
    SetCapture(wtk_widget_hwnd(designer->canvas));
}

static void on_canvas_mouse_move(WtkWidget *widget, WtkEvent *event, void *data)
{
    Designer *designer = (Designer *)data;
    DesignItem *item;
    int page_x;
    int page_y;
    int dx;
    int dy;
    int new_x;
    int new_y;
    int new_w;
    int new_h;
    int min_w = 8;
    int min_h = 8;

    (void)widget;
    if (!designer || designer->action == ACTION_NONE ||
        designer->action_index < 0 ||
        designer->action_index >= designer->doc.count)
        return;

    designer_page_point(designer, event->x, event->y, &page_x, &page_y);
    item = &designer->doc.items[designer->action_index];
    if (item->kind == DK_SEPARATOR) {
        min_w = 8;
        min_h = 1;
    } else if (item->kind == DK_PROGRESS) {
        min_h = 6;
    }

    if (designer->action == ACTION_MOVE) {
        dx = event->x - designer->action_start_x;
        dy = event->y - designer->action_start_y;
        new_x = snap_grid(designer->action_orig.x +
                          iround(dx / designer->view_scale));
        new_y = snap_grid(designer->action_orig.y +
                          iround(dy / designer->view_scale));
        new_x = clampi(new_x, 0,
                       designer->doc.page_w - designer->action_orig.w);
        new_y = clampi(new_y, 0,
                       designer->doc.page_h - designer->action_orig.h);
        if (new_x != item->x || new_y != item->y) {
            item->x = new_x;
            item->y = new_y;
            designer->action_changed = 1;
            designer_update_geometry_fields(designer);
            designer_redraw(designer);
        }
        return;
    }

    new_x = designer->action_orig.x;
    new_y = designer->action_orig.y;
    new_w = designer->action_orig.w;
    new_h = designer->action_orig.h;
    dx = iround((event->x - designer->action_start_x) /
                designer->view_scale);
    dy = iround((event->y - designer->action_start_y) /
                designer->view_scale);
    if (designer->action_handle & HANDLE_LEFT) {
        int left = snap_grid(designer->action_orig.x + dx);
        left = clampi(left, 0,
                      designer->action_orig.x + designer->action_orig.w -
                          min_w);
        new_x = left;
        new_w = designer->action_orig.x + designer->action_orig.w - left;
    } else if (designer->action_handle & HANDLE_RIGHT) {
        int right = snap_grid(designer->action_orig.x +
                              designer->action_orig.w + dx);
        right = clampi(right, designer->action_orig.x + min_w,
                       designer->doc.page_w);
        new_w = right - designer->action_orig.x;
    }
    if (designer->action_handle & HANDLE_TOP) {
        int top = snap_grid(designer->action_orig.y + dy);
        top = clampi(top, 0,
                     designer->action_orig.y + designer->action_orig.h -
                         min_h);
        new_y = top;
        new_h = designer->action_orig.y + designer->action_orig.h - top;
    } else if (designer->action_handle & HANDLE_BOTTOM) {
        int bottom = snap_grid(designer->action_orig.y +
                               designer->action_orig.h + dy);
        bottom = clampi(bottom, designer->action_orig.y + min_h,
                        designer->doc.page_h);
        new_h = bottom - designer->action_orig.y;
    }
    if (new_x != item->x || new_y != item->y || new_w != item->w ||
        new_h != item->h) {
        item->x = new_x;
        item->y = new_y;
        item->w = new_w;
        item->h = new_h;
        designer->action_changed = 1;
        designer_update_geometry_fields(designer);
        designer_redraw(designer);
    }
}

static void on_canvas_mouse_up(WtkWidget *widget, WtkEvent *event, void *data)
{
    Designer *designer = (Designer *)data;
    int changed;
    (void)widget;
    (void)event;
    if (!designer || designer->action == ACTION_NONE)
        return;
    changed = designer->action_changed;
    designer->action = ACTION_NONE;
    designer->action_handle = 0;
    designer->action_changed = 0;
    if (GetCapture() == wtk_widget_hwnd(designer->canvas))
        ReleaseCapture();
    designer_sync_properties(designer);
    if (changed) {
        designer_regenerate_code(designer);
        designer_redraw(designer);
        designer_status(designer, "已更新控件位置和尺寸");
    } else if (designer->selected >= 0 &&
               designer->selected < designer->doc.count) {
        char status[160];
        DesignItem *item = &designer->doc.items[designer->selected];
        snprintf(status, sizeof(status), "已选择 %s · %s",
                 designer_kind_name(item->kind), item->name);
        designer_status(designer, status);
    }
}

static void on_canvas_key_down(WtkWidget *widget, WtkEvent *event, void *data)
{
    Designer *designer = (Designer *)data;
    DesignItem *item;
    int step;
    int changed = 0;

    (void)widget;
    if (!designer)
        return;
    if (event->key == VK_ESCAPE) {
        designer_select(designer, -1);
        return;
    }
    if (event->key == VK_DELETE || event->key == VK_BACK) {
        designer_delete_selected(designer);
        return;
    }
    if (designer->selected < 0 ||
        designer->selected >= designer->doc.count)
        return;
    item = &designer->doc.items[designer->selected];
    step = (GetKeyState(VK_SHIFT) & 0x8000) ? DESIGNER_GRID : 1;
    switch (event->key) {
    case VK_LEFT:
        item->x -= step;
        changed = 1;
        break;
    case VK_RIGHT:
        item->x += step;
        changed = 1;
        break;
    case VK_UP:
        item->y -= step;
        changed = 1;
        break;
    case VK_DOWN:
        item->y += step;
        changed = 1;
        break;
    default:
        break;
    }
    if (changed) {
        designer_clamp_item(designer, item);
        designer_update_geometry_fields(designer);
        designer_regenerate_code(designer);
        designer_redraw(designer);
    }
}

/* ------------------------------------------------------------------------- */
/* Palette                                                                   */
/* ------------------------------------------------------------------------- */

static void palette_metrics(Designer *designer, int height, int *pad_out,
                            int *row_h_out)
{
    int dpi = wtk_app_dpi(designer->app);
    int pad = iround(4.0 * dpi / 96.0);
    int row_h = (height - pad * 2) / DK_COUNT;
    int min_h = iround(22.0 * dpi / 96.0);
    int max_h = iround(38.0 * dpi / 96.0);

    if (row_h > max_h)
        row_h = max_h;
    if (row_h < min_h)
        row_h = min_h;
    if (row_h * DK_COUNT > height - pad * 2)
        row_h = (height - pad * 2) / DK_COUNT;
    if (row_h < 1)
        row_h = 1;
    *pad_out = pad;
    *row_h_out = row_h;
}

static int palette_index_at(Designer *designer, int height, int x, int y)
{
    int pad;
    int row_h;
    int index;
    (void)x;
    palette_metrics(designer, height, &pad, &row_h);
    if (y < pad)
        return -1;
    index = (y - pad) / row_h;
    if (index < 0 || index >= DK_COUNT)
        return -1;
    return index;
}

static void palette_icon(DrawContext *ctx, DesignKind kind, RECT box,
                         int active)
{
    WtkColor color = active ? ctx->theme->accent : ctx->theme->muted;
    WtkColor surface = ctx->theme->surface;
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_pen;
    int w = box.right - box.left;
    int h = box.bottom - box.top;

    brush = CreateSolidBrush(color_ref(surface));
    pen = CreatePen(PS_SOLID, 1, color_ref(color));
    old_brush = SelectObject(ctx->dc, brush);
    old_pen = SelectObject(ctx->dc, pen);

    switch (kind) {
    case DK_LABEL: {
        int i;
        for (i = 0; i < 3; ++i) {
            int y = box.top + h / 4 + i * (h / 4);
            MoveToEx(ctx->dc, box.left + 2, y, NULL);
            LineTo(ctx->dc, box.right - (i == 1 ? 4 : 8), y);
        }
        break;
    }
    case DK_BUTTON:
        RoundRect(ctx->dc, box.left, box.top + h / 4, box.right,
                  box.bottom - h / 4, 4, 4);
        break;
    case DK_ENTRY:
        RoundRect(ctx->dc, box.left, box.top + h / 4, box.right,
                  box.bottom - h / 4, 4, 4);
        MoveToEx(ctx->dc, box.left + 5, box.top + h / 3, NULL);
        LineTo(ctx->dc, box.left + 5, box.bottom - h / 3);
        break;
    case DK_TEXT:
        Rectangle(ctx->dc, box.left, box.top + h / 5, box.right,
                  box.bottom - h / 5);
        MoveToEx(ctx->dc, box.left + 4, box.top + h / 2, NULL);
        LineTo(ctx->dc, box.right - 4, box.top + h / 2);
        break;
    case DK_CHECKBOX:
        Rectangle(ctx->dc, box.left, box.top + 2,
                  box.left + w / 2, box.bottom - 2);
        MoveToEx(ctx->dc, box.left + w / 2 + 4, box.top + h / 2, NULL);
        LineTo(ctx->dc, box.right, box.top + h / 2);
        break;
    case DK_LISTBOX:
        Rectangle(ctx->dc, box.left, box.top + 2, box.right,
                  box.bottom - 2);
        MoveToEx(ctx->dc, box.left + 3, box.top + h / 3, NULL);
        LineTo(ctx->dc, box.right - 3, box.top + h / 3);
        MoveToEx(ctx->dc, box.left + 3, box.top + 2 * h / 3, NULL);
        LineTo(ctx->dc, box.right - 3, box.top + 2 * h / 3);
        break;
    case DK_SLIDER:
        MoveToEx(ctx->dc, box.left, box.top + h / 2, NULL);
        LineTo(ctx->dc, box.right, box.top + h / 2);
        Ellipse(ctx->dc, box.left + w / 2 - 4, box.top + h / 2 - 4,
                box.left + w / 2 + 4, box.top + h / 2 + 4);
        break;
    case DK_PROGRESS:
        Rectangle(ctx->dc, box.left, box.top + h / 2 - 3,
                  box.right, box.top + h / 2 + 3);
        MoveToEx(ctx->dc, box.left, box.top + h / 2, NULL);
        LineTo(ctx->dc, box.left + w / 2, box.top + h / 2);
        break;
    case DK_SEPARATOR:
        MoveToEx(ctx->dc, box.left, box.top + h / 2, NULL);
        LineTo(ctx->dc, box.right, box.top + h / 2);
        break;
    case DK_FRAME:
        RoundRect(ctx->dc, box.left, box.top + 2, box.right,
                  box.bottom - 2, 5, 5);
        MoveToEx(ctx->dc, box.left + 4, box.top + 2, NULL);
        LineTo(ctx->dc, box.left + 4, box.top + 7);
        break;
    case DK_CANVAS:
        Rectangle(ctx->dc, box.left + 1, box.top + 2, box.right - 1,
                  box.bottom - 2);
        MoveToEx(ctx->dc, box.left + 3, box.bottom - 4, NULL);
        LineTo(ctx->dc, box.right - 3, box.top + 3);
        break;
    default:
        break;
    }

    SelectObject(ctx->dc, old_brush);
    SelectObject(ctx->dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void palette_draw(WtkWidget *widget, HDC dc, int width, int height,
                         void *userdata)
{
    Designer *designer = (Designer *)userdata;
    DrawContext ctx;
    int pad;
    int row_h;
    int i;
    int dpi;
    WtkColor surface;
    WtkColor hover;
    WtkColor pressed;
    RECT full;

    (void)widget;
    if (!designer)
        return;
    memset(&ctx, 0, sizeof(ctx));
    ctx.designer = designer;
    ctx.dc = dc;
    ctx.theme = wtk_app_get_theme(designer->app);
    dpi = wtk_app_dpi(designer->app);
    ctx.scale = (double)dpi / 96.0;

    surface = ctx.theme->surface;
    hover = WTK_RGB(239, 244, 252);
    pressed = WTK_RGB(224, 235, 251);
    full.left = 0;
    full.top = 0;
    full.right = width;
    full.bottom = height;
    fill_color(dc, &full, surface);

    palette_metrics(designer, height, &pad, &row_h);
    for (i = 0; i < DK_COUNT; ++i) {
        RECT row;
        RECT icon;
        RECT label;
        int icon_size = row_h - iround(8.0 * ctx.scale);
        if (icon_size < 12)
            icon_size = 12;
        if (icon_size > iround(28.0 * ctx.scale))
            icon_size = iround(28.0 * ctx.scale);
        row.left = iround(2.0 * ctx.scale);
        row.top = pad + i * row_h + 1;
        row.right = width - iround(2.0 * ctx.scale);
        row.bottom = pad + (i + 1) * row_h - 1;
        if (designer->palette_pressed && i == designer->palette_hover) {
            round_rect_color(dc, row, iround(7.0 * ctx.scale), pressed,
                             pressed, 0);
        } else if (i == designer->palette_hover) {
            round_rect_color(dc, row, iround(7.0 * ctx.scale), hover, hover,
                             0);
        }
        icon.left = row.left + iround(8.0 * ctx.scale);
        icon.top = row.top + (row_h - icon_size) / 2;
        icon.right = icon.left + icon_size;
        icon.bottom = icon.top + icon_size;
        palette_icon(&ctx, (DesignKind)i, icon,
                     i == designer->palette_hover);
        label.left = icon.right + iround(10.0 * ctx.scale);
        label.top = row.top;
        label.right = row.right - iround(4.0 * ctx.scale);
        label.bottom = row.bottom;
        draw_text_utf8(&ctx, designer_palette[i].label, label,
                       ctx.theme->text, 10, 0,
                       DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                           DT_END_ELLIPSIS);
    }

    for (i = 0; i < ctx.font_count; ++i)
        DeleteObject(ctx.fonts[i].font);
}

static int palette_cursor_page(Designer *designer, int *x, int *y)
{
    POINT point;
    RECT client;
    if (!designer || !designer->canvas)
        return 0;
    GetCursorPos(&point);
    ScreenToClient(wtk_widget_hwnd(designer->canvas), &point);
    GetClientRect(wtk_widget_hwnd(designer->canvas), &client);
    if (point.x < client.left || point.x >= client.right ||
        point.y < client.top || point.y >= client.bottom)
        return 0;
    return designer_event_page(designer, point.x, point.y, x, y);
}

static void on_palette_mouse_down(WtkWidget *widget, WtkEvent *event,
                                  void *data)
{
    Designer *designer = (Designer *)data;
    int index;
    int height;
    RECT client;

    (void)widget;
    if (!designer)
        return;
    GetClientRect(wtk_widget_hwnd(designer->palette), &client);
    height = client.bottom - client.top;
    index = palette_index_at(designer, height, event->x, event->y);
    if (index < 0)
        return;
    designer->palette_hover = index;
    designer->palette_pressed = 1;
    designer->palette_dragging = 0;
    designer->palette_kind = (DesignKind)index;
    GetCursorPos(&designer->palette_start);
    SetCapture(wtk_widget_hwnd(designer->palette));
    wtk_canvas_redraw(designer->palette);
}

static void on_palette_mouse_move(WtkWidget *widget, WtkEvent *event,
                                  void *data)
{
    Designer *designer = (Designer *)data;
    int height;
    int index;
    RECT client;

    (void)widget;
    if (!designer)
        return;
    GetClientRect(wtk_widget_hwnd(designer->palette), &client);
    height = client.bottom - client.top;
    if (designer->palette_pressed) {
        POINT point;
        int dx;
        int dy;
        GetCursorPos(&point);
        dx = point.x - designer->palette_start.x;
        dy = point.y - designer->palette_start.y;
        if (!designer->palette_dragging && (abs(dx) > 4 || abs(dy) > 4))
            designer->palette_dragging = 1;
        if (designer->palette_dragging) {
            int drop_x;
            int drop_y;
            int old_active = designer->drop_active;
            int old_kind = (int)designer->drop_kind;
            int old_x = designer->drop_x;
            int old_y = designer->drop_y;
            int kind = (int)designer->palette_kind;
            if (palette_cursor_page(designer, &drop_x, &drop_y)) {
                int w = designer_palette[kind].width;
                int h = designer_palette[kind].height;
                drop_x = snap_grid(drop_x);
                drop_y = snap_grid(drop_y);
                drop_x = clampi(drop_x, 0, designer->doc.page_w - w);
                drop_y = clampi(drop_y, 0, designer->doc.page_h - h);
                designer->drop_active = 1;
                designer->drop_kind = (DesignKind)kind;
                designer->drop_x = drop_x;
                designer->drop_y = drop_y;
            } else {
                designer->drop_active = 0;
            }
            if (old_active != designer->drop_active ||
                old_kind != (int)designer->drop_kind ||
                old_x != designer->drop_x || old_y != designer->drop_y)
                wtk_canvas_redraw(designer->canvas);
        }
        wtk_canvas_redraw(designer->palette);
        return;
    }
    index = palette_index_at(designer, height, event->x, event->y);
    if (index != designer->palette_hover) {
        designer->palette_hover = index;
        wtk_canvas_redraw(designer->palette);
    }
}

static void on_palette_mouse_up(WtkWidget *widget, WtkEvent *event, void *data)
{
    Designer *designer = (Designer *)data;
    int index;
    int default_w;
    int default_h;
    int x;
    int y;

    (void)widget;
    (void)event;
    if (!designer || !designer->palette_pressed)
        return;
    index = (int)designer->palette_kind;
    default_w = designer_palette[index].width;
    default_h = designer_palette[index].height;
    if (designer->palette_dragging && designer->drop_active) {
        x = designer->drop_x;
        y = designer->drop_y;
        designer_add_kind(designer, designer->palette_kind, x, y);
    } else if (!designer->palette_dragging) {
        int offset = (designer->doc.count % 5) * 12;
        x = (designer->doc.page_w - default_w) / 2 + offset;
        y = (designer->doc.page_h - default_h) / 2 + offset;
        designer_add_kind(designer, designer->palette_kind, x, y);
    }
    designer->palette_pressed = 0;
    designer->palette_dragging = 0;
    designer->drop_active = 0;
    if (GetCapture() == wtk_widget_hwnd(designer->palette))
        ReleaseCapture();
    wtk_canvas_redraw(designer->palette);
    wtk_canvas_redraw(designer->canvas);
}

static void on_palette_mouse_leave(WtkWidget *widget, WtkEvent *event,
                                   void *data)
{
    Designer *designer = (Designer *)data;
    (void)widget;
    (void)event;
    if (!designer || designer->palette_pressed)
        return;
    if (designer->palette_hover != -1) {
        designer->palette_hover = -1;
        wtk_canvas_redraw(designer->palette);
    }
}

/* ------------------------------------------------------------------------- */
/* Live preview, clear and export                                            */
/* ------------------------------------------------------------------------- */

static WtkWidget *create_preview_item(WtkWidget *parent,
                                      const DesignItem *item)
{
    WtkWidget *widget = NULL;

    switch (item->kind) {
    case DK_LABEL:
        widget = wtk_label(parent, item->text);
        break;
    case DK_BUTTON:
        widget = wtk_button(parent, item->text);
        if (item->primary)
            wtk_button_set_style(widget, WTK_BUTTON_PRIMARY);
        break;
    case DK_ENTRY:
        widget = wtk_entry(parent);
        if (item->text[0])
            wtk_entry_set_text(widget, item->text);
        wtk_entry_set_placeholder(widget,
                                  designer_kind_placeholder(DK_ENTRY));
        break;
    case DK_TEXT:
        widget = wtk_text(parent);
        if (item->text[0])
            wtk_text_set_text(widget, item->text);
        wtk_text_set_placeholder(widget,
                                 designer_kind_placeholder(DK_TEXT));
        break;
    case DK_CHECKBOX:
        widget = wtk_checkbox(parent, item->text, item->checked);
        break;
    case DK_LISTBOX:
        widget = wtk_listbox(parent);
        wtk_listbox_add(widget, "项目一");
        wtk_listbox_add(widget, "项目二");
        wtk_listbox_add(widget, "项目三");
        break;
    case DK_SLIDER:
        widget = wtk_slider(parent, 0, 100, 50);
        break;
    case DK_PROGRESS:
        widget = wtk_progress(parent);
        wtk_progress_set(widget, 0.5);
        break;
    case DK_SEPARATOR:
        widget = wtk_separator(parent);
        break;
    case DK_FRAME:
        widget = wtk_frame(parent);
        wtk_widget_set_background(widget, WTK_RGB(237, 241, 247));
        wtk_widget_set_border(widget, WTK_RGB(220, 226, 236), 1, 8);
        break;
    case DK_CANVAS:
        widget = wtk_canvas(parent, item->w, item->h);
        wtk_widget_set_background(widget, WTK_RGB(255, 255, 255));
        break;
    default:
        break;
    }
    if (!widget)
        return NULL;
    wtk_place(widget, item->x, item->y, item->w, item->h);
    if (designer_kind_has_font(item->kind)) {
        wtk_widget_set_font(widget, NULL, item->font_size,
                            item->bold ? WTK_FONT_BOLD : 0);
    }
    return widget;
}

static void designer_open_preview(Designer *designer)
{
    char title[200];
    WtkWidget *window;
    WtkWidget *page;
    int i;

    if (!designer)
        return;
    snprintf(title, sizeof(title), "%s - 预览", designer->doc.title);
    window = wtk_window(designer->app, title, designer->doc.page_w + 48,
                        designer->doc.page_h + 120);
    if (!window) {
        wtk_message_box(designer->window, "预览失败",
                        "无法创建预览窗口。", WTK_ICON_ERROR);
        return;
    }
    page = wtk_frame(window);
    wtk_widget_set_background(page, WTK_RGB(255, 255, 255));
    wtk_widget_set_padding(page, 0, 0, 0, 0);
    wtk_pack(page, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    for (i = 0; i < designer->doc.count; ++i) {
        if (!create_preview_item(page, &designer->doc.items[i]))
            break;
    }
    wtk_window_center(window);
    wtk_window_show(window);
    designer_status(designer, "已打开实时预览，关闭预览窗口即可返回编辑");
}

static void designer_clear_document(Designer *designer)
{
    if (!designer)
        return;
    if (designer->doc.count > 0 &&
        !wtk_ask_yes_no(designer->window, "清空页面",
                        "确定要删除页面中的所有控件吗？")) {
        return;
    }
    designer_document_clear(&designer->doc);
    designer->selected = -1;
    designer_sync_properties(designer);
    designer_update_page_caption(designer);
    designer_regenerate_code(designer);
    designer_redraw(designer);
    designer_status(designer, "页面已清空");
}

static void designer_export_code(Designer *designer)
{
    WCHAR path[MAX_PATH];
    OPENFILENAMEW ofn;
    HANDLE file;
    DWORD written = 0;
    DWORD size;
    char utf8_path[MAX_PATH * 3];
    char message[MAX_PATH * 3 + 120];

    if (!designer)
        return;
    designer_regenerate_code(designer);
    wcscpy(path, L"generated_ui.c");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = wtk_widget_hwnd(designer->window);
    ofn.lpstrFilter =
        L"C 源文件 (*.c)\0*.c\0所有文件 (*.*)\0*.*\0\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"c";
    ofn.lpstrTitle = L"导出生成的 C 代码";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return;

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        wtk_message_box(designer->window, "导出失败",
                        "无法创建目标文件，请换一个位置后重试。",
                        WTK_ICON_ERROR);
        return;
    }
    size = (DWORD)strlen(designer->code);
    if (!WriteFile(file, designer->code, size, &written, NULL) ||
        written != size) {
        CloseHandle(file);
        wtk_message_box(designer->window, "导出失败",
                        "写入文件时发生错误。", WTK_ICON_ERROR);
        return;
    }
    CloseHandle(file);
    wide_to_utf8(path, utf8_path, sizeof(utf8_path));
    snprintf(message, sizeof(message),
             "C 代码已导出：\n%s\n\n编译时请保留 UTF-8 源码参数。",
             utf8_path);
    wtk_message_box(designer->window, "导出成功", message, WTK_ICON_INFO);
    designer_status(designer, "已导出 C 代码");
}

static void on_clear_click(WtkWidget *widget, WtkEvent *event, void *data)
{
    (void)widget;
    (void)event;
    designer_clear_document((Designer *)data);
}

static void on_preview_click(WtkWidget *widget, WtkEvent *event, void *data)
{
    (void)widget;
    (void)event;
    designer_open_preview((Designer *)data);
}

static void on_export_click(WtkWidget *widget, WtkEvent *event, void *data)
{
    (void)widget;
    (void)event;
    designer_export_code((Designer *)data);
}

/* ------------------------------------------------------------------------- */
/* UI construction                                                           */
/* ------------------------------------------------------------------------- */

static WtkWidget *make_card(Designer *designer, WtkWidget *parent)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *card = wtk_frame(parent);
    wtk_widget_set_background(card, theme->surface);
    wtk_widget_set_border(card, theme->border, 1, 12);
    wtk_widget_set_padding(card, 14, 14, 14, 14);
    return card;
}

static WtkWidget *make_heading(WtkWidget *parent, const char *text, int size)
{
    WtkWidget *label = wtk_label(parent, text);
    wtk_widget_set_font(label, NULL, size, WTK_FONT_BOLD);
    return label;
}

static WtkWidget *make_prop_entry(Designer *designer, WtkWidget *parent,
                                  const char *label_text, const char *value)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *row = wtk_frame(parent);
    WtkWidget *label = wtk_label(row, label_text);
    WtkWidget *entry = wtk_entry(row);

    wtk_widget_set_background(row, theme->surface);
    wtk_widget_set_font(label, NULL, 9, 0);
    wtk_widget_set_foreground(label, theme->muted);
    wtk_widget_set_size(label, 54, 0);
    wtk_pack(label, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 0);

    wtk_widget_set_size(entry, 0, 28);
    wtk_widget_set_font(entry, NULL, 9, 0);
    wtk_entry_set_text(entry, value ? value : "");
    wtk_pack(entry, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    wtk_bind(entry, WTK_EVENT_SUBMIT, on_apply_properties, designer);
    wtk_pack(row, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 3);
    return entry;
}

static WtkWidget *build_header(Designer *designer, WtkWidget *window)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *header = wtk_frame(window);
    WtkWidget *actions = wtk_frame(header);
    WtkWidget *titles = wtk_frame(header);
    WtkWidget *title = wtk_label(titles, "WinTk Designer");
    WtkWidget *subtitle =
        wtk_label(titles, "拖拽控件 · 所见即所得 · 自动生成 C 代码");

    wtk_widget_set_background(header, theme->window_bg);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 24, 14);

    wtk_widget_set_background(actions, theme->window_bg);
    designer->clear_button = wtk_button(actions, "清空");
    wtk_button_set_style(designer->clear_button, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(designer->clear_button, 0, 38);
    wtk_pack(designer->clear_button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 4, 0);
    wtk_bind(designer->clear_button, WTK_EVENT_CLICK, on_clear_click,
             designer);

    designer->preview_button = wtk_button(actions, "预览");
    wtk_button_set_style(designer->preview_button, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(designer->preview_button, 0, 38);
    wtk_pack(designer->preview_button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 4,
             0);
    wtk_bind(designer->preview_button, WTK_EVENT_CLICK, on_preview_click,
             designer);

    designer->export_button = wtk_button(actions, "导出 C...");
    wtk_button_set_style(designer->export_button, WTK_BUTTON_PRIMARY);
    wtk_widget_set_size(designer->export_button, 0, 38);
    wtk_pack(designer->export_button, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 4, 0);
    wtk_bind(designer->export_button, WTK_EVENT_CLICK, on_export_click,
             designer);
    wtk_pack(actions, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);

    wtk_widget_set_background(titles, theme->window_bg);
    wtk_widget_set_font(title, NULL, 20, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);
    wtk_widget_set_font(subtitle, NULL, 10, 0);
    wtk_widget_set_foreground(subtitle, theme->muted);
    wtk_pack(subtitle, WTK_SIDE_TOP, WTK_FILL_X, 0, 2, 0);
    wtk_pack(titles, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    return header;
}

static WtkWidget *build_code_panel(Designer *designer, WtkWidget *window)
{
    WtkWidget *card = wtk_frame(window);
    WtkWidget *header = wtk_frame(card);
    WtkWidget *title = wtk_label(header, "生成的 C 代码");
    WtkWidget *hint = wtk_label(header, "自动生成 · 只读");

    wtk_widget_set_background(card, WTK_RGB(16, 24, 40));
    wtk_widget_set_border(card, WTK_RGB(40, 55, 79), 1, 14);
    wtk_widget_set_padding(card, 12, 10, 12, 12);
    wtk_widget_set_size(card, 0, 218);
    wtk_pack(card, WTK_SIDE_BOTTOM, WTK_FILL_X, 0, 24, 10);

    wtk_widget_set_background(header, WTK_RGB(16, 24, 40));
    wtk_widget_set_font(hint, NULL, 9, 0);
    wtk_widget_set_foreground(hint, WTK_RGB(148, 170, 201));
    wtk_pack(hint, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);
    wtk_widget_set_font(title, NULL, 11, WTK_FONT_BOLD);
    wtk_widget_set_foreground(title, WTK_RGB(235, 242, 252));
    wtk_pack(title, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    designer->code_view = wtk_text(card);
    wtk_widget_set_background(designer->code_view, WTK_RGB(11, 18, 32));
    wtk_widget_set_foreground(designer->code_view, WTK_RGB(221, 231, 247));
    wtk_widget_set_border(designer->code_view, WTK_RGB(37, 50, 74), 1, 8);
    wtk_widget_set_font(designer->code_view, "Consolas", 9, 0);
    wtk_text_set_readonly(designer->code_view, 1);
    wtk_pack(designer->code_view, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    designer->status_label = hint;
    designer_status(designer, "自动生成 · 只读");
    return card;
}

static WtkWidget *build_palette(Designer *designer, WtkWidget *workspace)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *card = make_card(designer, workspace);
    WtkWidget *title = make_heading(card, "控件库", 14);
    WtkWidget *hint = wtk_label(card, "拖到画布，或单击添加");

    wtk_widget_set_size(card, 196, 0);
    wtk_pack(card, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 12);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);
    wtk_widget_set_font(hint, NULL, 9, 0);
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_pack(hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    designer->palette = wtk_canvas(card, 160, 420);
    wtk_widget_set_cursor(designer->palette, WTK_CURSOR_HAND);
    wtk_canvas_set_draw(designer->palette, palette_draw, designer);
    wtk_bind(designer->palette, WTK_EVENT_MOUSE_DOWN,
             on_palette_mouse_down, designer);
    wtk_bind(designer->palette, WTK_EVENT_MOUSE_MOVE,
             on_palette_mouse_move, designer);
    wtk_bind(designer->palette, WTK_EVENT_MOUSE_UP, on_palette_mouse_up,
             designer);
    wtk_bind(designer->palette, WTK_EVENT_MOUSE_LEAVE,
             on_palette_mouse_leave, designer);
    wtk_pack(designer->palette, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    return card;
}

static WtkWidget *build_inspector(Designer *designer, WtkWidget *workspace)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *card = make_card(designer, workspace);
    WtkWidget *title = make_heading(card, "属性", 14);
    WtkWidget *hint = wtk_label(card, "选中控件后可编辑名称、文本与几何尺寸");
    WtkWidget *type_row;
    WtkWidget *type_label;
    WtkWidget *flags;
    WtkWidget *buttons;
    WtkWidget *apply;
    WtkWidget *delete_button;
    WtkWidget *bottom_hint;

    wtk_widget_set_size(card, 316, 0);
    wtk_pack(card, WTK_SIDE_RIGHT, WTK_FILL_Y, 0, 12, 12);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 0);
    wtk_widget_set_font(hint, NULL, 9, 0);
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_pack(hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    type_row = wtk_frame(card);
    wtk_widget_set_background(type_row, theme->surface);
    type_label = wtk_label(type_row, "类型");
    wtk_widget_set_font(type_label, NULL, 9, 0);
    wtk_widget_set_foreground(type_label, theme->muted);
    wtk_widget_set_size(type_label, 54, 0);
    wtk_pack(type_label, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 0);
    designer->type_value = wtk_label(type_row, "页面");
    wtk_widget_set_font(designer->type_value, NULL, 9, WTK_FONT_BOLD);
    wtk_widget_set_foreground(designer->type_value, theme->accent);
    wtk_pack(designer->type_value, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    wtk_pack(type_row, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 4);

    designer->name_entry = make_prop_entry(designer, card, "名称", "");
    designer->text_entry = make_prop_entry(designer, card, "文本", "");
    designer->x_entry = make_prop_entry(designer, card, "X", "0");
    designer->y_entry = make_prop_entry(designer, card, "Y", "0");
    designer->w_entry = make_prop_entry(designer, card, "宽", "760");
    designer->h_entry = make_prop_entry(designer, card, "高", "520");
    designer->font_entry = make_prop_entry(designer, card, "字号", "12");

    flags = wtk_frame(card);
    wtk_widget_set_background(flags, theme->surface);
    designer->bold_check = wtk_checkbox(flags, "粗体", 0);
    designer->checked_check = wtk_checkbox(flags, "选中", 0);
    designer->primary_check = wtk_checkbox(flags, "主按钮", 0);
    wtk_widget_set_font(designer->bold_check, NULL, 9, 0);
    wtk_widget_set_font(designer->checked_check, NULL, 9, 0);
    wtk_widget_set_font(designer->primary_check, NULL, 9, 0);
    wtk_pack(designer->bold_check, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 2, 0);
    wtk_pack(designer->checked_check, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 2,
             0);
    wtk_pack(designer->primary_check, WTK_SIDE_LEFT, WTK_FILL_NONE, 0, 2,
             0);
    wtk_pack(flags, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    buttons = wtk_frame(card);
    wtk_widget_set_background(buttons, theme->surface);
    delete_button = wtk_button(buttons, "删除");
    wtk_button_set_style(delete_button, WTK_BUTTON_SECONDARY);
    wtk_widget_set_size(delete_button, 0, 34);
    wtk_pack(delete_button, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 4, 0);
    wtk_bind(delete_button, WTK_EVENT_CLICK, on_delete_click, designer);
    designer->delete_button = delete_button;

    apply = wtk_button(buttons, "应用属性");
    wtk_button_set_style(apply, WTK_BUTTON_PRIMARY);
    wtk_widget_set_size(apply, 0, 34);
    wtk_pack(apply, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    wtk_bind(apply, WTK_EVENT_CLICK, on_apply_properties, designer);
    designer->apply_button = apply;
    wtk_pack(buttons, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 6);

    bottom_hint = wtk_label(card,
                            "逻辑像素坐标 · 拖动边角缩放 · Delete 删除");
    wtk_widget_set_font(bottom_hint, NULL, 8, 0);
    wtk_widget_set_foreground(bottom_hint, theme->muted);
    wtk_pack(bottom_hint, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 5);
    return card;
}

static WtkWidget *build_canvas_card(Designer *designer, WtkWidget *workspace)
{
    const WtkTheme *theme = wtk_app_get_theme(designer->app);
    WtkWidget *card = make_card(designer, workspace);
    WtkWidget *header = wtk_frame(card);
    WtkWidget *title = make_heading(header, "页面画布", 13);
    WtkWidget *hint = wtk_label(header, "");

    wtk_pack(card, WTK_SIDE_LEFT, WTK_FILL_BOTH, 1, 0, 0);
    wtk_widget_set_background(header, theme->surface);
    wtk_widget_set_font(hint, NULL, 9, 0);
    wtk_widget_set_foreground(hint, theme->muted);
    wtk_pack(hint, WTK_SIDE_RIGHT, WTK_FILL_NONE, 0, 0, 0);
    designer->page_caption = hint;
    wtk_pack(title, WTK_SIDE_LEFT, WTK_FILL_X, 1, 0, 0);
    wtk_pack(header, WTK_SIDE_TOP, WTK_FILL_X, 0, 0, 8);

    designer->canvas = wtk_canvas(card, 640, 440);
    wtk_widget_set_background(designer->canvas, WTK_RGB(230, 235, 244));
    wtk_canvas_set_draw(designer->canvas, canvas_draw, designer);
    wtk_bind(designer->canvas, WTK_EVENT_MOUSE_DOWN, on_canvas_mouse_down,
             designer);
    wtk_bind(designer->canvas, WTK_EVENT_MOUSE_MOVE, on_canvas_mouse_move,
             designer);
    wtk_bind(designer->canvas, WTK_EVENT_MOUSE_UP, on_canvas_mouse_up,
             designer);
    wtk_bind(designer->canvas, WTK_EVENT_KEY_DOWN, on_canvas_key_down,
             designer);
    wtk_pack(designer->canvas, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
    return card;
}

static WtkWidget *build_workspace(Designer *designer, WtkWidget *window)
{
    WtkWidget *workspace = wtk_frame(window);
    wtk_widget_set_background(workspace,
                              wtk_app_get_theme(designer->app)->window_bg);
    wtk_pack(workspace, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 24, 0);
    build_palette(designer, workspace);
    build_inspector(designer, workspace);
    build_canvas_card(designer, workspace);
    return workspace;
}

static int run_designer(void)
{
    Designer *designer;
    WtkApp *app;
    WtkWidget *window;
    const char *font_family;

    app = wtk_app_new();
    if (!app)
        return 1;
    designer = (Designer *)calloc(1, sizeof(*designer));
    if (!designer) {
        wtk_app_free(app);
        return 1;
    }
    designer->app = app;
    designer->selected = -1;
    designer->palette_hover = -1;
    designer_document_default(&designer->doc);

    font_family = wtk_app_ui_font_family(app);
    utf8_to_wide(font_family, designer->font_family, LF_FACESIZE);
    if (!designer->font_family[0])
        wcscpy(designer->font_family, L"Segoe UI");

    window = wtk_window(app, "WinTk Designer - 轻量级 C UI 页面编辑器",
                        DESIGNER_WINDOW_W, DESIGNER_WINDOW_H);
    if (!window) {
        free(designer);
        wtk_app_free(app);
        return 1;
    }
    designer->window = window;
    wtk_window_set_min_size(window, DESIGNER_MIN_W, DESIGNER_MIN_H);
    wtk_widget_set_background(window,
                              wtk_app_get_theme(app)->window_bg);

    build_header(designer, window);
    build_code_panel(designer, window);
    build_workspace(designer, window);

    designer_update_page_caption(designer);
    designer_sync_properties(designer);
    designer_regenerate_code(designer);
    designer_status(designer, "就绪 · 拖拽左侧控件到页面");

    wtk_window_center(window);
    wtk_run(app);
    wtk_app_free(app);
    free(designer);
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show_command)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;
    return run_designer();
}
