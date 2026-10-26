#include "wtk_internal.h"

/* ------------------------------------------------------------------------- */
/* Measuring                                                                 */
/* ------------------------------------------------------------------------- */

static HDC measuring_dc(HFONT *old_font, WtkWidget *widget)
{
    HDC dc = GetDC(NULL);
    if (!dc)
        return NULL;
    if (old_font)
        *old_font = (HFONT)SelectObject(dc, widget->font);
    return dc;
}

static void release_measuring_dc(HDC dc, HFONT old_font)
{
    if (!dc)
        return;
    if (old_font)
        SelectObject(dc, old_font);
    ReleaseDC(NULL, dc);
}

static int font_line_height(HDC dc)
{
    TEXTMETRICW tm;
    if (!dc || !GetTextMetricsW(dc, &tm))
        return 16;
    return tm.tmHeight + tm.tmExternalLeading;
}

static void measure_line(HDC dc, const WCHAR *text, int *width, int *height)
{
    SIZE size;
    if (!text)
        text = L"";
    if (!GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size)) {
        size.cx = 0;
        size.cy = font_line_height(dc);
    }
    if (width)
        *width = size.cx;
    if (height)
        *height = size.cy;
}

/* Measures a label's text. A positive wrap_width forces word wrapping. Without
 * wrapping, explicit newlines are honored and the widest line is returned. */
static void measure_text_block(WtkWidget *widget, const WCHAR *text,
                               int wrap_width, int *out_width, int *out_height)
{
    HDC dc;
    HFONT old_font = NULL;
    int line_height;
    int w = 0;
    int h = 0;

    *out_width = 0;
    *out_height = 0;
    if (!text)
        text = L"";

    dc = measuring_dc(&old_font, widget);
    if (!dc) {
        *out_width = 0;
        *out_height = 16;
        return;
    }
    line_height = font_line_height(dc);

    if (wrap_width > 0) {
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = wrap_width;
        rc.bottom = 0;
        DrawTextW(dc, text, -1, &rc,
                  DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
        w = rc.right;
        if (w > wrap_width)
            w = wrap_width;
        if (w <= 0)
            w = wrap_width;
        h = rc.bottom;
        if (h <= 0)
            h = line_height;
    } else {
        const WCHAR *line = text;
        const WCHAR *cursor = text;
        int lines = 0;
        int max_w = 0;
        for (;;) {
            if (*cursor == L'\0' || *cursor == L'\n') {
                int line_w = 0;
                int count = (int)(cursor - line);
                SIZE size;
                if (count > 0) {
                    if (GetTextExtentPoint32W(dc, line, count, &size))
                        line_w = size.cx;
                }
                if (line_w > max_w)
                    max_w = line_w;
                lines++;
                if (*cursor == L'\0')
                    break;
                line = cursor + 1;
            }
            cursor++;
        }
        if (lines < 1)
            lines = 1;
        w = max_w;
        h = lines * line_height;
    }

    release_measuring_dc(dc, old_font);
    *out_width = w;
    *out_height = h;
}

static void add_padding(WtkWidget *widget, int *width, int *height)
{
    *width += widget->pad_l + widget->pad_r;
    *height += widget->pad_t + widget->pad_b;
}

static int widget_border_inset(WtkWidget *widget)
{
    if (!widget->border_set || widget->border_width <= 0)
        return 0;
    return widget->border_width;
}

static void content_insets(WtkWidget *widget, int *left, int *top, int *right,
                           int *bottom)
{
    int border = widget_border_inset(widget);
    *left = widget->pad_l + border;
    *top = widget->pad_t + border;
    *right = widget->pad_r + border;
    *bottom = widget->pad_b + border;
}

static int visible_managed(WtkWidget *widget)
{
    return widget && widget->visible && widget->layout_managed;
}

static void distribute_extra(int *sizes, const int *weights, int count,
                             int extra)
{
    int total_weight = 0;
    long long accumulated = 0;
    long long previous;
    int i;

    if (!sizes || !weights || count <= 0 || extra <= 0)
        return;
    for (i = 0; i < count; ++i)
        total_weight += weights[i];
    if (total_weight <= 0)
        return;

    for (i = 0; i < count; ++i) {
        previous = accumulated;
        accumulated += (long long)extra * weights[i];
        if (weights[i] > 0) {
            sizes[i] += (int)(accumulated / total_weight) -
                        (int)(previous / total_weight);
        }
    }
}

static int ensure_array_to(int **array, int *capacity, int wanted)
{
    int old_cap;
    int new_cap;
    int was_null = (*array == NULL);

    if (*array && wanted <= *capacity)
        return 1;

    old_cap = *capacity;
    new_cap = wanted > old_cap ? wanted : old_cap;
    if (new_cap < 4)
        new_cap = 4;

    *array = (int *)wtk__realloc_array(
        *array, (size_t)new_cap * sizeof(int));
    if (was_null)
        memset(*array, 0, (size_t)new_cap * sizeof(int));
    else if (new_cap > old_cap)
        memset(*array + old_cap, 0,
               (size_t)(new_cap - old_cap) * sizeof(int));
    *capacity = new_cap;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Natural size of a container's children                                    */
/* ------------------------------------------------------------------------- */

static void measure_pack_content(WtkWidget *parent, int *out_w, int *out_h)
{
    int width = 0;
    int height = 0;
    int horizontal = 0;
    int vertical = 0;
    int i;

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int child_w;
        int child_h;
        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &child_w, &child_h);
        child_w += 2 * child->pack_padx;
        child_h += 2 * child->pack_pady;
        if (child->pack_side == WTK_SIDE_TOP ||
            child->pack_side == WTK_SIDE_BOTTOM) {
            if (child_w > width)
                width = child_w;
            height += child_h;
            vertical = 1;
        } else {
            width += child_w;
            if (child_h > height)
                height = child_h;
            horizontal = 1;
        }
    }
    (void)horizontal;
    (void)vertical;
    *out_w = width;
    *out_h = height;
}

static void measure_place_content(WtkWidget *parent, int *out_w, int *out_h)
{
    int width = 0;
    int height = 0;
    int i;

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int child_w;
        int child_h;
        int right;
        int bottom;
        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &child_w, &child_h);
        child_w = child->place_w > 0 ? child->place_w : child_w;
        child_h = child->place_h > 0 ? child->place_h : child_h;
        right = child->place_x + child_w;
        bottom = child->place_y + child_h;
        if (right > width)
            width = right;
        if (bottom > height)
            height = bottom;
    }
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    *out_w = width;
    *out_h = height;
}

static void grid_dimensions(WtkWidget *parent, int *out_cols, int *out_rows)
{
    int cols = 0;
    int rows = 0;
    int i;
    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int end_col;
        int end_row;
        if (!visible_managed(child))
            continue;
        end_col = child->grid_col + (child->grid_colspan > 0
                                         ? child->grid_colspan
                                         : 1);
        end_row = child->grid_row + (child->grid_rowspan > 0
                                         ? child->grid_rowspan
                                         : 1);
        if (end_col > cols)
            cols = end_col;
        if (end_row > rows)
            rows = end_row;
    }
    if (parent->grid_col_configured && parent->grid_col_cap > cols)
        cols = parent->grid_col_cap;
    if (parent->grid_row_configured && parent->grid_row_cap > rows)
        rows = parent->grid_row_cap;
    *out_cols = cols;
    *out_rows = rows;
}

static void measure_grid_content(WtkWidget *parent, int *out_w, int *out_h)
{
    int cols = 0;
    int rows = 0;
    int *col_widths;
    int *row_heights;
    int i;

    grid_dimensions(parent, &cols, &rows);
    if (cols <= 0 || rows <= 0) {
        *out_w = 0;
        *out_h = 0;
        return;
    }

    col_widths = (int *)wtk__alloc_zero((size_t)cols * sizeof(int));
    row_heights = (int *)wtk__alloc_zero((size_t)rows * sizeof(int));

    if (parent->grid_col_configured) {
        for (i = 0; i < cols && i < parent->grid_col_cap; ++i)
            col_widths[i] = parent->grid_col_mins[i];
    }
    if (parent->grid_row_configured) {
        for (i = 0; i < rows && i < parent->grid_row_cap; ++i)
            row_heights[i] = parent->grid_row_mins[i];
    }

    /* Single-cell children first. */
    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int child_w;
        int child_h;
        int cspan;
        int rspan;
        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &child_w, &child_h);
        cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
        rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;
        child_w += 2 * child->grid_padx;
        child_h += 2 * child->grid_pady;
        if (cspan == 1 && child->grid_col >= 0 && child->grid_col < cols) {
            if (child_w > col_widths[child->grid_col])
                col_widths[child->grid_col] = child_w;
        }
        if (rspan == 1 && child->grid_row >= 0 && child->grid_row < rows) {
            if (child_h > row_heights[child->grid_row])
                row_heights[child->grid_row] = child_h;
        }
    }

    /* Spanning children grow their spanned columns/rows as needed. */
    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int child_w;
        int child_h;
        int cspan;
        int rspan;
        int j;
        int needed;
        int available;
        if (!visible_managed(child))
            continue;
        cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
        rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;
        if (cspan <= 1 && rspan <= 1)
            continue;
        wtk__measure_widget(child, &child_w, &child_h);

        if (cspan > 1) {
            available = 0;
            for (j = 0; j < cspan && child->grid_col + j < cols; ++j)
                available += col_widths[child->grid_col + j];
            needed = child_w + 2 * child->grid_padx;
            if (available < needed) {
                int deficit = needed - available;
                int share = deficit / cspan;
                int rem = deficit % cspan;
                for (j = 0; j < cspan && child->grid_col + j < cols; ++j) {
                    col_widths[child->grid_col + j] += share + (rem > 0);
                    if (rem > 0)
                        rem--;
                }
            }
        }
        if (rspan > 1) {
            available = 0;
            for (j = 0; j < rspan && child->grid_row + j < rows; ++j)
                available += row_heights[child->grid_row + j];
            needed = child_h + 2 * child->grid_pady;
            if (available < needed) {
                int deficit = needed - available;
                int share = deficit / rspan;
                int rem = deficit % rspan;
                for (j = 0; j < rspan && child->grid_row + j < rows; ++j) {
                    row_heights[child->grid_row + j] += share + (rem > 0);
                    if (rem > 0)
                        rem--;
                }
            }
        }
    }

    {
        int width = 0;
        int height = 0;
        for (i = 0; i < cols; ++i)
            width += col_widths[i];
        for (i = 0; i < rows; ++i)
            height += row_heights[i];
        *out_w = width;
        *out_h = height;
    }
    free(col_widths);
    free(row_heights);
}

/* ------------------------------------------------------------------------- */
/* Public measurement                                                        */
/* ------------------------------------------------------------------------- */

void wtk__measure_widget(WtkWidget *widget, int *width, int *height)
{
    int natural_w = 0;
    int natural_h = 0;
    int content_w = 0;
    int content_h = 0;
    int line_w = 0;
    int line_h = 0;
    HDC dc;
    HFONT old_font = NULL;

    if (!widget) {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    switch (widget->type) {
    case WTK_TYPE_WINDOW:
        natural_w = 0;
        natural_h = 0;
        break;

    case WTK_TYPE_FRAME:
    case WTK_TYPE_RADIOGROUP: {
        int left;
        int top;
        int right;
        int bottom;
        if (widget->layout_mode == WTK_LAYOUT_PACK)
            measure_pack_content(widget, &content_w, &content_h);
        else if (widget->layout_mode == WTK_LAYOUT_GRID)
            measure_grid_content(widget, &content_w, &content_h);
        else if (widget->layout_mode == WTK_LAYOUT_PLACE)
            measure_place_content(widget, &content_w, &content_h);
        content_insets(widget, &left, &top, &right, &bottom);
        natural_w = content_w + left + right;
        natural_h = content_h + top + bottom;
        break;
    }

    case WTK_TYPE_LABEL:
        measure_text_block(widget, widget->text, widget->wrap_width,
                           &content_w, &content_h);
        add_padding(widget, &content_w, &content_h);
        natural_w = content_w;
        natural_h = content_h;
        break;

    case WTK_TYPE_BUTTON: {
        dc = measuring_dc(&old_font, widget);
        if (dc) {
            measure_line(dc, widget->text ? widget->text : L"", &line_w,
                         &line_h);
            release_measuring_dc(dc, old_font);
        }
        natural_w = line_w + widget->pad_l + widget->pad_r;
        natural_h = line_h + widget->pad_t + widget->pad_b;
        {
            int control_h =
                wtk__logical(widget->app, widget->app->theme.control_height);
            if (natural_h < control_h)
                natural_h = control_h;
        }
        break;
    }

    case WTK_TYPE_CHECKBOX:
    case WTK_TYPE_RADIO: {
        int box = wtk__scale_widget(widget, 18);
        int gap = wtk__scale_widget(widget, 9);
        dc = measuring_dc(&old_font, widget);
        if (dc) {
            measure_line(dc, widget->text ? widget->text : L"", &line_w,
                         &line_h);
            release_measuring_dc(dc, old_font);
        }
        natural_w = box + gap + line_w + widget->pad_l + widget->pad_r;
        natural_h = (box > line_h ? box : line_h) +
                    widget->pad_t + widget->pad_b;
        if (natural_h < wtk__logical(widget->app, 28))
            natural_h = wtk__logical(widget->app, 28);
        break;
    }

    case WTK_TYPE_ENTRY:
        natural_w = wtk__logical(widget->app, 220);
        natural_h = wtk__logical(widget->app,
                                 widget->app->theme.control_height);
        dc = measuring_dc(&old_font, widget);
        if (dc) {
            line_h = font_line_height(dc);
            release_measuring_dc(dc, old_font);
        }
        if (natural_h < line_h + wtk__logical(widget->app, 16))
            natural_h = line_h + wtk__logical(widget->app, 16);
        break;

    case WTK_TYPE_TEXT:
        natural_w = wtk__logical(widget->app, 360);
        natural_h = wtk__logical(widget->app, 180);
        break;

    case WTK_TYPE_LISTBOX:
        natural_w = wtk__logical(widget->app, 260);
        natural_h = wtk__logical(widget->app, 200);
        break;

    case WTK_TYPE_SLIDER:
        natural_w = wtk__logical(widget->app, 220);
        natural_h = wtk__logical(widget->app, 34);
        break;

    case WTK_TYPE_PROGRESS:
        natural_w = wtk__logical(widget->app, 180);
        natural_h = wtk__logical(widget->app,
                                 widget->show_percent ? 38 : 10);
        break;

    case WTK_TYPE_SEPARATOR:
        natural_w = wtk__logical(widget->app, 100);
        natural_h = wtk__scale_widget(widget, 1);
        if (natural_h < 1)
            natural_h = 1;
        break;

    case WTK_TYPE_CANVAS:
        natural_w = wtk__logical(widget->app, 320);
        natural_h = wtk__logical(widget->app, 200);
        break;
    }

    if (widget->req_w > 0)
        natural_w = widget->req_w;
    if (widget->req_h > 0)
        natural_h = widget->req_h;
    if (natural_w < 0)
        natural_w = 0;
    if (natural_h < 0)
        natural_h = 0;

    widget->pref_w = natural_w;
    widget->pref_h = natural_h;
    if (width)
        *width = natural_w;
    if (height)
        *height = natural_h;
}

/* ------------------------------------------------------------------------- */
/* Actual layout                                                             */
/* ------------------------------------------------------------------------- */

static RECT widget_content_rect(WtkWidget *widget)
{
    RECT rc;
    int left;
    int top;
    int right;
    int bottom;
    GetClientRect(widget->hwnd, &rc);
    content_insets(widget, &left, &top, &right, &bottom);
    rc.left += left;
    rc.top += top;
    rc.right -= right;
    rc.bottom -= bottom;
    if (rc.right < rc.left)
        rc.right = rc.left;
    if (rc.bottom < rc.top)
        rc.bottom = rc.top;
    return rc;
}

static void finish_child_layout(WtkWidget *child)
{
    if (!child || !child->hwnd || child->pending_free)
        return;
    if (wtk__is_field(child))
        wtk__native_position(child);
    if (wtk__is_container(child) && child->layout_mode != WTK_LAYOUT_NONE)
        wtk__layout_children(child);
    wtk__invalidate(child);
}

static void place_child(WtkWidget *child, int x, int y, int width, int height)
{
    if (!child || !child->hwnd || child->pending_free)
        return;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    child->x = x;
    child->y = y;
    child->w = width;
    child->h = height;
    SetWindowPos(child->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    finish_child_layout(child);
}

static void layout_pack(WtkWidget *parent, RECT content)
{
    int available_w = content.right - content.left;
    int available_h = content.bottom - content.top;
    int vertical_sum = 0;
    int horizontal_sum = 0;
    int vertical_expanders = 0;
    int horizontal_expanders = 0;
    int extra_v;
    int extra_h;
    int v_share;
    int v_rem;
    int h_share;
    int h_rem;
    int i;
    int left = content.left;
    int top = content.top;
    int right = content.right;
    int bottom = content.bottom;

    if (available_w < 0)
        available_w = 0;
    if (available_h < 0)
        available_h = 0;

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int cw;
        int ch;
        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &cw, &ch);
        if (child->pack_side == WTK_SIDE_TOP ||
            child->pack_side == WTK_SIDE_BOTTOM) {
            vertical_sum += ch + 2 * child->pack_pady;
            if (child->pack_expand)
                vertical_expanders++;
        } else {
            horizontal_sum += cw + 2 * child->pack_padx;
            if (child->pack_expand)
                horizontal_expanders++;
        }
    }

    extra_v = available_h - vertical_sum;
    extra_h = available_w - horizontal_sum;
    if (extra_v < 0)
        extra_v = 0;
    if (extra_h < 0)
        extra_h = 0;
    v_share = vertical_expanders > 0 ? extra_v / vertical_expanders : 0;
    v_rem = vertical_expanders > 0 ? extra_v % vertical_expanders : 0;
    h_share = horizontal_expanders > 0 ? extra_h / horizontal_expanders : 0;
    h_rem = horizontal_expanders > 0 ? extra_h % horizontal_expanders : 0;

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int cw;
        int ch;
        int extra_child_v = 0;
        int extra_child_h = 0;
        int padx;
        int pady;
        int child_w;
        int child_h;
        int x;
        int y;

        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &cw, &ch);
        padx = child->pack_padx;
        pady = child->pack_pady;

        if (child->pack_expand) {
            if (child->pack_side == WTK_SIDE_TOP ||
                child->pack_side == WTK_SIDE_BOTTOM) {
                extra_child_v = v_share + (v_rem > 0);
                if (v_rem > 0)
                    v_rem--;
            } else {
                extra_child_h = h_share + (h_rem > 0);
                if (h_rem > 0)
                    h_rem--;
            }
        }

        if (child->pack_side == WTK_SIDE_TOP ||
            child->pack_side == WTK_SIDE_BOTTOM) {
            int parcel_w = right - left;
            int parcel_h = ch + 2 * pady + extra_child_v;
            int avail_h = bottom - top;
            int avail_w = right - left;
            int parcel_top;
            if (parcel_w < 0)
                parcel_w = 0;
            if (avail_h < 0)
                avail_h = 0;
            if (avail_w < 0)
                avail_w = 0;
            if (parcel_h > avail_h)
                parcel_h = avail_h;
            if (parcel_h < 0)
                parcel_h = 0;

            if (child->pack_side == WTK_SIDE_TOP) {
                parcel_top = top;
                top += parcel_h;
            } else {
                parcel_top = bottom - parcel_h;
                bottom -= parcel_h;
            }

            child_w = (child->pack_fill & WTK_FILL_X)
                          ? parcel_w - 2 * padx
                          : cw;
            child_h = (child->pack_fill & WTK_FILL_Y)
                          ? parcel_h - 2 * pady
                          : ch;
            if (child_w < 0)
                child_w = 0;
            if (child_h < 0)
                child_h = 0;
            if (child_w > parcel_w - 2 * padx && parcel_w - 2 * padx > 0)
                child_w = parcel_w - 2 * padx;
            x = left + padx;
            if (child->pack_side == WTK_SIDE_BOTTOM)
                y = parcel_top + (parcel_h - pady - child_h);
            else
                y = parcel_top + pady;
        } else {
            int parcel_h = bottom - top;
            int parcel_w = cw + 2 * padx + extra_child_h;
            int avail_w = right - left;
            int avail_h = bottom - top;
            int parcel_left;
            if (parcel_h < 0)
                parcel_h = 0;
            if (avail_w < 0)
                avail_w = 0;
            if (avail_h < 0)
                avail_h = 0;
            if (parcel_w > avail_w)
                parcel_w = avail_w;
            if (parcel_w < 0)
                parcel_w = 0;

            if (child->pack_side == WTK_SIDE_LEFT) {
                parcel_left = left;
                left += parcel_w;
            } else {
                parcel_left = right - parcel_w;
                right -= parcel_w;
            }

            child_w = (child->pack_fill & WTK_FILL_X)
                          ? parcel_w - 2 * padx
                          : cw;
            child_h = (child->pack_fill & WTK_FILL_Y)
                          ? parcel_h - 2 * pady
                          : ch;
            if (child_w < 0)
                child_w = 0;
            if (child_h < 0)
                child_h = 0;
            if (child_h > parcel_h - 2 * pady && parcel_h - 2 * pady > 0)
                child_h = parcel_h - 2 * pady;
            y = top + pady;
            if (child->pack_side == WTK_SIDE_RIGHT)
                x = parcel_left + (parcel_w - padx - child_w);
            else
                x = parcel_left + padx;
        }

        if (!child->pending_free)
            place_child(child, x, y, child_w, child_h);
    }
}

static void layout_grid(WtkWidget *parent, RECT content)
{
    int cols = 0;
    int rows = 0;
    int *col_widths;
    int *row_heights;
    int *col_weights;
    int *row_weights;
    int *col_offsets;
    int *row_offsets;
    int extra_w;
    int extra_h;
    int total_w = 0;
    int total_h = 0;
    int i;
    int c;
    int r;
    int derived_cols = 0;
    int derived_rows = 0;

    grid_dimensions(parent, &cols, &rows);
    if (cols <= 0 || rows <= 0)
        return;

    col_widths = (int *)wtk__alloc_zero((size_t)cols * sizeof(int));
    row_heights = (int *)wtk__alloc_zero((size_t)rows * sizeof(int));
    col_weights = (int *)wtk__alloc_zero((size_t)cols * sizeof(int));
    row_weights = (int *)wtk__alloc_zero((size_t)rows * sizeof(int));
    col_offsets = (int *)wtk__alloc_zero((size_t)(cols + 1) * sizeof(int));
    row_offsets = (int *)wtk__alloc_zero((size_t)(rows + 1) * sizeof(int));

    if (parent->grid_col_configured) {
        for (i = 0; i < cols && i < parent->grid_col_cap; ++i) {
            col_widths[i] = parent->grid_col_mins[i];
            col_weights[i] = parent->grid_col_weights[i];
        }
    }
    if (parent->grid_row_configured) {
        for (i = 0; i < rows && i < parent->grid_row_cap; ++i) {
            row_heights[i] = parent->grid_row_mins[i];
            row_weights[i] = parent->grid_row_weights[i];
        }
    }

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int cw;
        int ch;
        int cspan;
        int rspan;
        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &cw, &ch);
        cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
        rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;

        if (cspan == 1 && child->grid_col >= 0 && child->grid_col < cols) {
            if (cw + 2 * child->grid_padx > col_widths[child->grid_col])
                col_widths[child->grid_col] =
                    cw + 2 * child->grid_padx;
        }
        if (rspan == 1 && child->grid_row >= 0 && child->grid_row < rows) {
            if (ch + 2 * child->grid_pady > row_heights[child->grid_row])
                row_heights[child->grid_row] =
                    ch + 2 * child->grid_pady;
        }
    }

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int cw;
        int ch;
        int cspan;
        int rspan;
        int j;
        int available;
        int needed;
        if (!visible_managed(child))
            continue;
        cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
        rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;
        if (cspan <= 1 && rspan <= 1)
            continue;
        wtk__measure_widget(child, &cw, &ch);

        if (cspan > 1) {
            available = 0;
            for (j = 0; j < cspan && child->grid_col + j < cols; ++j)
                available += col_widths[child->grid_col + j];
            needed = cw + 2 * child->grid_padx;
            if (available < needed) {
                int deficit = needed - available;
                int share = deficit / cspan;
                int rem = deficit % cspan;
                for (j = 0; j < cspan && child->grid_col + j < cols; ++j) {
                    col_widths[child->grid_col + j] += share + (rem > 0);
                    if (rem > 0)
                        rem--;
                }
            }
        }
        if (rspan > 1) {
            available = 0;
            for (j = 0; j < rspan && child->grid_row + j < rows; ++j)
                available += row_heights[child->grid_row + j];
            needed = ch + 2 * child->grid_pady;
            if (available < needed) {
                int deficit = needed - available;
                int share = deficit / rspan;
                int rem = deficit % rspan;
                for (j = 0; j < rspan && child->grid_row + j < rows; ++j) {
                    row_heights[child->grid_row + j] += share + (rem > 0);
                    if (rem > 0)
                        rem--;
                }
            }
        }
    }

    if (!parent->grid_col_configured) {
        for (i = 0; i < parent->child_count; ++i) {
            WtkWidget *child = parent->children[i];
            int cspan;
            if (!visible_managed(child))
                continue;
            if ((child->grid_sticky & WTK_STICKY_HORIZONTAL) ==
                WTK_STICKY_HORIZONTAL) {
                cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
                for (c = 0; c < cspan && child->grid_col + c < cols; ++c)
                    col_weights[child->grid_col + c] = 1;
                derived_cols = 1;
            }
        }
    }
    if (!parent->grid_row_configured) {
        for (i = 0; i < parent->child_count; ++i) {
            WtkWidget *child = parent->children[i];
            int rspan;
            if (!visible_managed(child))
                continue;
            if ((child->grid_sticky & WTK_STICKY_VERTICAL) ==
                WTK_STICKY_VERTICAL) {
                rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;
                for (r = 0; r < rspan && child->grid_row + r < rows; ++r)
                    row_weights[child->grid_row + r] = 1;
                derived_rows = 1;
            }
        }
    }
    (void)derived_cols;
    (void)derived_rows;

    for (i = 0; i < cols; ++i)
        total_w += col_widths[i];
    for (i = 0; i < rows; ++i)
        total_h += row_heights[i];

    extra_w = (content.right - content.left) - total_w;
    extra_h = (content.bottom - content.top) - total_h;
    if (extra_w < 0)
        extra_w = 0;
    if (extra_h < 0)
        extra_h = 0;
    distribute_extra(col_widths, col_weights, cols, extra_w);
    distribute_extra(row_heights, row_weights, rows, extra_h);

    for (i = 0; i < cols; ++i)
        col_offsets[i + 1] = col_offsets[i] + col_widths[i];
    for (i = 0; i < rows; ++i)
        row_offsets[i + 1] = row_offsets[i] + row_heights[i];

    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        int cw;
        int ch;
        int col;
        int row;
        int cspan;
        int rspan;
        int cell_x;
        int cell_y;
        int cell_w;
        int cell_h;
        int inner_x;
        int inner_y;
        int inner_w;
        int inner_h;
        int child_w;
        int child_h;
        int x;
        int y;
        WtkSticky sticky;

        if (!visible_managed(child))
            continue;
        wtk__measure_widget(child, &cw, &ch);
        col = child->grid_col;
        row = child->grid_row;
        cspan = child->grid_colspan > 0 ? child->grid_colspan : 1;
        rspan = child->grid_rowspan > 0 ? child->grid_rowspan : 1;
        if (col < 0)
            col = 0;
        if (row < 0)
            row = 0;
        if (col >= cols || row >= rows)
            continue;
        if (col + cspan > cols)
            cspan = cols - col;
        if (row + rspan > rows)
            rspan = rows - row;

        cell_x = content.left + col_offsets[col];
        cell_y = content.top + row_offsets[row];
        cell_w = col_offsets[col + cspan] - col_offsets[col];
        cell_h = row_offsets[row + rspan] - row_offsets[row];

        inner_x = cell_x + child->grid_padx;
        inner_y = cell_y + child->grid_pady;
        inner_w = cell_w - 2 * child->grid_padx;
        inner_h = cell_h - 2 * child->grid_pady;
        if (inner_w < 0)
            inner_w = 0;
        if (inner_h < 0)
            inner_h = 0;

        sticky = child->grid_sticky;
        if ((sticky & WTK_STICKY_HORIZONTAL) == WTK_STICKY_HORIZONTAL) {
            child_w = inner_w;
            x = inner_x;
        } else {
            child_w = cw < inner_w ? cw : inner_w;
            if (sticky & WTK_STICKY_LEFT)
                x = inner_x;
            else if (sticky & WTK_STICKY_RIGHT)
                x = inner_x + inner_w - child_w;
            else
                x = inner_x + (inner_w - child_w) / 2;
        }

        if ((sticky & WTK_STICKY_VERTICAL) == WTK_STICKY_VERTICAL) {
            child_h = inner_h;
            y = inner_y;
        } else {
            child_h = ch < inner_h ? ch : inner_h;
            if (sticky & WTK_STICKY_TOP)
                y = inner_y;
            else if (sticky & WTK_STICKY_BOTTOM)
                y = inner_y + inner_h - child_h;
            else
                y = inner_y + (inner_h - child_h) / 2;
        }

        if (!child->pending_free)
            place_child(child, x, y, child_w, child_h);
    }

    free(col_widths);
    free(row_heights);
    free(col_weights);
    free(row_weights);
    free(col_offsets);
    free(row_offsets);
}

static void layout_place(WtkWidget *parent, RECT content)
{
    int i;
    for (i = 0; i < parent->child_count; ++i) {
        WtkWidget *child = parent->children[i];
        if (!visible_managed(child))
            continue;
        place_child(child, content.left + child->place_x,
                    content.top + child->place_y, child->place_w,
                    child->place_h);
    }
}

void wtk__layout_children(WtkWidget *parent)
{
    RECT content;

    if (!parent || !parent->hwnd || parent->layout_mode == WTK_LAYOUT_NONE)
        return;

    content = widget_content_rect(parent);
    if (parent->layout_mode == WTK_LAYOUT_PACK)
        layout_pack(parent, content);
    else if (parent->layout_mode == WTK_LAYOUT_GRID)
        layout_grid(parent, content);
    else if (parent->layout_mode == WTK_LAYOUT_PLACE)
        layout_place(parent, content);
}

void wtk__layout_window(WtkWidget *window)
{
    WtkApp *app;
    int passes = 0;

    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;
    app = window->app;
    if (!app)
        return;
    if (app->in_layout) {
        app->layout_pending = 1;
        return;
    }

    do {
        app->layout_pending = 0;
        app->in_layout = 1;
        wtk__layout_children(window);
        app->in_layout = 0;
        passes++;
    } while (app->layout_pending && passes < 4);
    app->layout_pending = 0;
}

void wtk__request_relayout(WtkWidget *widget)
{
    WtkWidget *window;
    if (!widget)
        return;
    window = wtk__toplevel(widget);
    if (!window || window->type != WTK_TYPE_WINDOW || !window->hwnd)
        return;
    if (window->app->in_layout) {
        window->app->layout_pending = 1;
        return;
    }
    wtk__layout_window(window);
}

/* ------------------------------------------------------------------------- */
/* Public geometry managers                                                  */
/* ------------------------------------------------------------------------- */

void wtk_pack(WtkWidget *widget, WtkSide side, WtkFill fill, int expand,
              int padx, int pady)
{
    WtkWidget *parent;
    if (!widget)
        return;
    parent = widget->parent;
    if (!parent)
        return;

    widget->layout_managed = 1;
    widget->pack_side = side;
    widget->pack_fill = fill;
    widget->pack_expand = !!expand;
    widget->pack_padx = wtk__logical(widget->app, padx > 0 ? padx : 0);
    widget->pack_pady = wtk__logical(widget->app, pady > 0 ? pady : 0);

    parent->layout_mode = WTK_LAYOUT_PACK;
    wtk__invalidate(widget);
    wtk__request_relayout(parent);
}

void wtk_grid(WtkWidget *widget, int row, int column, int row_span,
              int column_span, WtkSticky sticky, int padx, int pady)
{
    WtkWidget *parent;
    if (!widget)
        return;
    parent = widget->parent;
    if (!parent)
        return;

    widget->layout_managed = 1;
    widget->grid_row = row >= 0 ? row : 0;
    widget->grid_col = column >= 0 ? column : 0;
    widget->grid_rowspan = row_span > 0 ? row_span : 1;
    widget->grid_colspan = column_span > 0 ? column_span : 1;
    widget->grid_sticky = sticky;
    widget->grid_padx = wtk__logical(widget->app, padx > 0 ? padx : 0);
    widget->grid_pady = wtk__logical(widget->app, pady > 0 ? pady : 0);

    parent->layout_mode = WTK_LAYOUT_GRID;
    wtk__invalidate(widget);
    wtk__request_relayout(parent);
}

void wtk_grid_column_configure(WtkWidget *master, int column, int weight,
                               int min_width)
{
    if (!master || column < 0)
        return;
    ensure_array_to(&master->grid_col_weights, &master->grid_col_cap,
                    column + 1);
    ensure_array_to(&master->grid_col_mins, &master->grid_col_cap,
                    column + 1);
    master->grid_col_weights[column] = weight > 0 ? weight : 0;
    master->grid_col_mins[column] =
        min_width > 0 ? wtk__logical(master->app, min_width) : 0;
    master->grid_col_configured = 1;
    master->layout_mode = WTK_LAYOUT_GRID;
    wtk__request_relayout(master);
}

void wtk_grid_row_configure(WtkWidget *master, int row, int weight,
                            int min_height)
{
    if (!master || row < 0)
        return;
    ensure_array_to(&master->grid_row_weights, &master->grid_row_cap, row + 1);
    ensure_array_to(&master->grid_row_mins, &master->grid_row_cap, row + 1);
    master->grid_row_weights[row] = weight > 0 ? weight : 0;
    master->grid_row_mins[row] =
        min_height > 0 ? wtk__logical(master->app, min_height) : 0;
    master->grid_row_configured = 1;
    master->layout_mode = WTK_LAYOUT_GRID;
    wtk__request_relayout(master);
}

void wtk_place(WtkWidget *widget, int x, int y, int width, int height)
{
    WtkWidget *parent;
    if (!widget)
        return;
    parent = widget->parent;
    if (!parent)
        return;

    widget->layout_managed = 1;
    widget->place_x = wtk__logical(widget->app, x);
    widget->place_y = wtk__logical(widget->app, y);
    widget->place_w = wtk__logical(widget->app, width > 0 ? width : 0);
    widget->place_h = wtk__logical(widget->app, height > 0 ? height : 0);

    parent->layout_mode = WTK_LAYOUT_PLACE;
    wtk__invalidate(widget);
    wtk__request_relayout(parent);
}
