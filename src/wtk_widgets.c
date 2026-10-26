#include "wtk_internal.h"

#define WTK_CLASS_WIDGET L"WinTkWidget"

/* ------------------------------------------------------------------------- */
/* Creating widgets                                                          */
/* ------------------------------------------------------------------------- */

static HFONT create_font(WtkApp *app, const WCHAR *family, int point_size,
                         unsigned style)
{
    LOGFONTW lf;
    int dpi;

    if (!app)
        return NULL;
    dpi = app->dpi ? app->dpi : 96;
    if (!family || !family[0]) {
        family = app->ui_font_family[0] ? app->ui_font_family
                                        : L"Segoe UI";
    }
    if (point_size <= 0)
        point_size = 10;

    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(point_size, dpi, 72);
    if (lf.lfHeight == 0)
        lf.lfHeight = -1;
    lf.lfWeight = (style & WTK_FONT_BOLD) ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = (style & WTK_FONT_ITALIC) ? TRUE : FALSE;
    lf.lfUnderline = (style & WTK_FONT_UNDERLINE) ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy(lf.lfFaceName, family, LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = L'\0';

    return CreateFontIndirectW(&lf);
}

WtkWidget *wtk__alloc_widget(WtkApp *app, WtkWidget *parent,
                             WtkWidgetType type)
{
    WtkWidget *widget = (WtkWidget *)wtk__alloc_zero(sizeof(*widget));

    widget->app = app;
    widget->parent = parent;
    widget->type = type;
    widget->visible = 1;
    widget->enabled = 1;
    widget->resizable = 1;
    widget->layout_mode = WTK_LAYOUT_NONE;
    widget->pack_side = WTK_SIDE_TOP;
    widget->grid_rowspan = 1;
    widget->grid_colspan = 1;
    widget->align = WTK_ALIGN_LEFT;
    widget->button_style = WTK_BUTTON_SECONDARY;
    widget->value_step = 1;
    widget->border_radius = app ? wtk__logical(app, app->theme.corner_radius) : 10;
    widget->font_pt = 10;
    if (app && app->ui_font_family[0])
        wcsncpy(widget->font_family, app->ui_font_family,
                LF_FACESIZE - 1);
    else
        wcsncpy(widget->font_family, L"Segoe UI", LF_FACESIZE - 1);
    widget->font_family[LF_FACESIZE - 1] = L'\0';
    widget->font = create_font(app, widget->font_family, widget->font_pt, 0);

    switch (type) {
    case WTK_TYPE_BUTTON:
        widget->focusable = 1;
        widget->cursor = WTK_CURSOR_HAND;
        widget->align = WTK_ALIGN_CENTER;
        widget->pad_l = wtk__logical(app, 20);
        widget->pad_r = wtk__logical(app, 20);
        widget->pad_t = wtk__logical(app, 8);
        widget->pad_b = wtk__logical(app, 8);
        break;
    case WTK_TYPE_ENTRY:
    case WTK_TYPE_TEXT:
    case WTK_TYPE_LISTBOX:
    case WTK_TYPE_CHECKBOX:
    case WTK_TYPE_RADIO:
    case WTK_TYPE_SLIDER:
        widget->focusable = 1;
        break;
    case WTK_TYPE_CANVAS:
        widget->focusable = 1;
        break;
    default:
        break;
    }

    return widget;
}

void wtk__add_child(WtkWidget *parent, WtkWidget *child)
{
    if (!parent || !child)
        return;
    if (parent->child_count >= parent->child_cap) {
        int new_cap = parent->child_cap ? parent->child_cap * 2 : 8;
        parent->children = (WtkWidget **)wtk__realloc_array(
            parent->children, (size_t)new_cap * sizeof(WtkWidget *));
        parent->child_cap = new_cap;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

void wtk__remove_child(WtkWidget *parent, WtkWidget *child)
{
    int i;
    if (!parent || !child)
        return;
    for (i = 0; i < parent->child_count; ++i) {
        if (parent->children[i] == child) {
            memmove(&parent->children[i], &parent->children[i + 1],
                    (size_t)(parent->child_count - i - 1) *
                        sizeof(WtkWidget *));
            parent->child_count--;
            child->parent = NULL;
            return;
        }
    }
}

void wtk__invalidate_text_cache(WtkWidget *widget)
{
    if (!widget)
        return;
    free(widget->text_cache);
    widget->text_cache = NULL;
    widget->text_cache_valid = 0;
}

char *wtk__get_cached_text(WtkWidget *widget)
{
    if (!widget)
        return NULL;
    if (widget->text_cache_valid && widget->text_cache)
        return widget->text_cache;

    if (wtk__is_field(widget)) {
        const char *text = wtk__native_get_text(widget);
        return (char *)text;
    }

    wtk__invalidate_text_cache(widget);
    widget->text_cache = wtk__u16_to_u8(widget->text ? widget->text : L"");
    widget->text_cache_valid = 1;
    return widget->text_cache;
}

void wtk__set_widget_text(WtkWidget *widget, const char *text)
{
    WCHAR *wide;
    if (!widget)
        return;

    if (wtk__is_field(widget)) {
        wtk__native_set_text(widget, text);
        return;
    }

    wide = wtk__u8_to_u16(text ? text : "");
    free(widget->text);
    widget->text = wide;
    wtk__invalidate_text_cache(widget);
    wtk__invalidate(widget);
    wtk__request_relayout(widget);
}

static void free_widget_contents(WtkWidget *widget)
{
    int i;
    if (!widget)
        return;

    for (i = 0; i < widget->child_count; ++i)
        free_widget_contents(widget->children[i]);

    if (widget->menu)
        wtk__free_menu_tree(widget->menu);
    if (widget->font)
        DeleteObject(widget->font);
    if (widget->input_brush)
        DeleteObject(widget->input_brush);
    free(widget->text);
    free(widget->placeholder);
    free(widget->text_cache);
    free(widget->bindings);
    free(widget->children);
    free(widget->grid_col_weights);
    free(widget->grid_col_mins);
    free(widget->grid_row_weights);
    free(widget->grid_row_mins);
    free(widget);
}

void wtk__free_widget_tree(WtkWidget *widget)
{
    free_widget_contents(widget);
}

/* ------------------------------------------------------------------------- */
/* Event binding / dispatch                                                  */
/* ------------------------------------------------------------------------- */

void wtk__dispatch(WtkWidget *widget, WtkEvent *event)
{
    WtkApp *app;
    int count;
    int i;

    if (!widget || !event)
        return;
    app = widget->app;
    if (!app || app->shutting_down || widget->pending_free)
        return;

    count = widget->bind_count;
    if (count <= 0)
        return;

    wtk__begin_dispatch(app);
    for (i = 0; i < count && i < widget->bind_count; ++i) {
        if (widget->bindings[i].type != event->type)
            continue;
        if (widget->bindings[i].callback)
            widget->bindings[i].callback(widget, event,
                                         widget->bindings[i].userdata);
        if (widget->pending_free || app->shutting_down)
            break;
    }
    wtk__end_dispatch(app);
}

void wtk__emit(WtkWidget *widget, WtkEventType type)
{
    WtkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    wtk__dispatch(widget, &event);
}

void wtk__emit_change(WtkWidget *widget, int value)
{
    WtkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = WTK_EVENT_CHANGE;
    event.value = value;
    wtk__dispatch(widget, &event);
}

static unsigned current_modifiers(void)
{
    unsigned mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= WTK_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= WTK_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= WTK_MOD_ALT;
    return mods;
}

void wtk_bind(WtkWidget *widget, WtkEventType type, WtkEventFn callback,
              void *userdata)
{
    if (!widget || !callback)
        return;
    if (widget->bind_count >= widget->bind_cap) {
        int new_cap = widget->bind_cap ? widget->bind_cap * 2 : 4;
        widget->bindings = (WtkBinding *)wtk__realloc_array(
            widget->bindings, (size_t)new_cap * sizeof(WtkBinding));
        widget->bind_cap = new_cap;
    }
    widget->bindings[widget->bind_count].type = type;
    widget->bindings[widget->bind_count].callback = callback;
    widget->bindings[widget->bind_count].userdata = userdata;
    widget->bind_count++;
}

void wtk_unbind(WtkWidget *widget, WtkEventType type, WtkEventFn callback)
{
    int i;
    if (!widget)
        return;
    for (i = 0; i < widget->bind_count;) {
        if (widget->bindings[i].type == type &&
            widget->bindings[i].callback == callback) {
            memmove(&widget->bindings[i], &widget->bindings[i + 1],
                    (size_t)(widget->bind_count - i - 1) *
                        sizeof(WtkBinding));
            widget->bind_count--;
        } else {
            i++;
        }
    }
}

void wtk__invalidate(WtkWidget *widget)
{
    if (widget && widget->hwnd)
        InvalidateRect(widget->hwnd, NULL, FALSE);
}

void wtk__invalidate_native(WtkWidget *widget)
{
    if (!widget)
        return;
    if (widget->input)
        InvalidateRect(widget->input, NULL, TRUE);
    wtk__invalidate(widget);
}

/* ------------------------------------------------------------------------- */
/* Custom window procedure                                                   */
/* ------------------------------------------------------------------------- */

static HCURSOR cursor_handle(WtkCursor cursor)
{
    switch (cursor) {
    case WTK_CURSOR_HAND:
        return LoadCursorW(NULL, IDC_HAND);
    case WTK_CURSOR_IBEAM:
        return LoadCursorW(NULL, IDC_IBEAM);
    case WTK_CURSOR_CROSS:
        return LoadCursorW(NULL, IDC_CROSS);
    case WTK_CURSOR_SIZE_H:
        return LoadCursorW(NULL, IDC_SIZEWE);
    case WTK_CURSOR_SIZE_V:
        return LoadCursorW(NULL, IDC_SIZENS);
    default:
        return LoadCursorW(NULL, IDC_ARROW);
    }
}

static void emit_mouse(WtkWidget *widget, WtkEventType type, LPARAM lparam,
                       int button)
{
    WtkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.x = (short)LOWORD(lparam);
    event.y = (short)HIWORD(lparam);
    event.button = button;
    event.clicks = type == WTK_EVENT_DOUBLE_CLICK ? 2 : 1;
    event.modifiers = current_modifiers();
    wtk__dispatch(widget, &event);
}

static int point_in_widget(WtkWidget *widget, LPARAM lparam)
{
    RECT rc;
    POINT pt;
    if (!widget || !widget->hwnd)
        return 0;
    GetClientRect(widget->hwnd, &rc);
    pt.x = (short)LOWORD(lparam);
    pt.y = (short)HIWORD(lparam);
    return PtInRect(&rc, pt);
}

static void emit_click_at(WtkWidget *widget, int x, int y)
{
    WtkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = WTK_EVENT_CLICK;
    event.x = x;
    event.y = y;
    event.button = WTK_MOUSE_LEFT;
    event.clicks = 1;
    wtk__dispatch(widget, &event);
}

static void button_activate(WtkWidget *widget, int x, int y)
{
    if (widget->type == WTK_TYPE_BUTTON) {
        widget->pressed = 0;
        wtk__invalidate(widget);
        emit_click_at(widget, x, y);
    } else if (widget->type == WTK_TYPE_CHECKBOX) {
        widget->checked = !widget->checked;
        wtk__invalidate(widget);
        wtk__emit_change(widget, widget->checked);
    } else if (widget->type == WTK_TYPE_RADIO) {
        WtkWidget *group = widget->parent;
        int index;
        widget->pressed = 0;
        wtk__invalidate(widget);
        if (group && group->type == WTK_TYPE_RADIOGROUP) {
            index = wtk__radio_index(widget);
            if (index >= 0)
                wtk__radio_select(group, index, 1, 1);
        }
    }
}

static void slider_update_from_x(WtkWidget *widget, int x, int notify)
{
    int left;
    int right;
    double fraction;
    int value;

    if (!widget)
        return;
    left = wtk__scale_widget(widget, 12);
    right = widget->w - wtk__scale_widget(widget, 12);
    if (right <= left) {
        value = widget->value_min;
    } else {
        RECT rc;
        GetClientRect(widget->hwnd, &rc);
        fraction = (double)(x - left) / (double)(right - left);
        if (fraction < 0.0)
            fraction = 0.0;
        if (fraction > 1.0)
            fraction = 1.0;
        value = widget->value_min +
                (int)(fraction * (widget->value_max - widget->value_min) + 0.5);
        if (value < widget->value_min)
            value = widget->value_min;
        if (value > widget->value_max)
            value = widget->value_max;
    }

    if (value != widget->value) {
        widget->value = value;
        wtk__invalidate(widget);
        if (notify)
            wtk__emit_change(widget, value);
    }
}

static void slider_adjust(WtkWidget *widget, int delta)
{
    int value;
    if (!widget)
        return;
    value = widget->value + delta;
    if (value < widget->value_min)
        value = widget->value_min;
    if (value > widget->value_max)
        value = widget->value_max;
    if (value != widget->value) {
        widget->value = value;
        wtk__invalidate(widget);
        wtk__emit_change(widget, value);
    }
}

static int focus_this(WtkWidget *widget)
{
    if (!widget)
        return 0;
    if (widget->input) {
        SetFocus(widget->input);
        return 1;
    }
    if (widget->focusable && widget->hwnd) {
        SetFocus(widget->hwnd);
        return 1;
    }
    return 0;
}

static LRESULT CALLBACK widget_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam)
{
    WtkWidget *widget =
        (WtkWidget *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
        widget = (WtkWidget *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)widget);
        widget->hwnd = hwnd;
        return TRUE;
    }

    if (!widget)
        return DefWindowProcW(hwnd, message, wparam, lparam);

    if (wtk__is_field(widget)) {
        switch (message) {
        case WM_COMMAND:
            wtk__native_command(widget, wparam, lparam);
            return 0;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
            return (LRESULT)wtk__native_ctlcolor(widget, (HDC)wparam, message);
        default:
            break;
        }
    }

    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (rc.right > rc.left && rc.bottom > rc.top) {
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bitmap =
                CreateCompatibleBitmap(dc, rc.right - rc.left, rc.bottom - rc.top);
            if (mem && bitmap) {
                HGDIOBJ old = SelectObject(mem, bitmap);
                wtk__paint_widget(widget, mem, &rc);
                BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteObject(bitmap);
            } else {
                wtk__paint_widget(widget, dc, &rc);
            }
            if (mem)
                DeleteDC(mem);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        widget->w = rc.right - rc.left;
        widget->h = rc.bottom - rc.top;
        if (wtk__is_field(widget)) {
            wtk__native_position(widget);
        }
        if (widget->type == WTK_TYPE_CANVAS && widget->hwnd) {
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_RESIZE;
            event.width = widget->w;
            event.height = widget->h;
            wtk__dispatch(widget, &event);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lparam);
        TRACKMOUSEEVENT tme;
        if (!widget->tracking_mouse) {
            widget->tracking_mouse = 1;
            memset(&tme, 0, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            if (!widget->hover) {
                widget->hover = 1;
                wtk__invalidate(widget);
                emit_mouse(widget, WTK_EVENT_MOUSE_ENTER, lparam,
                           WTK_MOUSE_LEFT);
            }
        }
        if (widget->type == WTK_TYPE_SLIDER && widget->pressed &&
            GetCapture() == hwnd) {
            slider_update_from_x(widget, x, 1);
        }
        emit_mouse(widget, WTK_EVENT_MOUSE_MOVE, lparam, WTK_MOUSE_LEFT);
        return 0;
    }

    case WM_MOUSELEAVE:
        widget->tracking_mouse = 0;
        if (widget->hover) {
            widget->hover = 0;
            wtk__invalidate(widget);
            wtk__emit(widget, WTK_EVENT_MOUSE_LEAVE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        RECT rc;
        int interactive = (widget->type == WTK_TYPE_BUTTON ||
                           widget->type == WTK_TYPE_CHECKBOX ||
                           widget->type == WTK_TYPE_RADIO ||
                           widget->type == WTK_TYPE_SLIDER);
        focus_this(widget);
        if (interactive) {
            widget->pressed = 1;
            SetCapture(hwnd);
            wtk__invalidate(widget);
        }
        if (widget->type == WTK_TYPE_SLIDER) {
            int x = (short)LOWORD(lparam);
            GetClientRect(hwnd, &rc);
            slider_update_from_x(widget, x, 1);
        }
        emit_mouse(widget, WTK_EVENT_MOUSE_DOWN, lparam, WTK_MOUSE_LEFT);
        return 0;
    }

    case WM_LBUTTONUP: {
        int was_pressed = widget->pressed;
        int inside = point_in_widget(widget, lparam);
        LPARAM coordinates = lparam;
        if (GetCapture() == hwnd)
            ReleaseCapture();
        widget->pressed = 0;
        wtk__invalidate(widget);
        emit_mouse(widget, WTK_EVENT_MOUSE_UP, coordinates, WTK_MOUSE_LEFT);
        if (was_pressed && inside && !widget->pending_free)
            button_activate(widget, (short)LOWORD(coordinates),
                            (short)HIWORD(coordinates));
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        focus_this(widget);
        emit_mouse(widget, WTK_EVENT_DOUBLE_CLICK, lparam, WTK_MOUSE_LEFT);
        return 0;

    case WM_KEYDOWN: {
        WtkEvent event;
        memset(&event, 0, sizeof(event));
        event.type = WTK_EVENT_KEY_DOWN;
        event.key = (int)wparam;
        event.modifiers = current_modifiers();
        wtk__dispatch(widget, &event);
        if (widget->pending_free)
            return 0;

        if (widget->type == WTK_TYPE_BUTTON) {
            if ((wparam == VK_SPACE || wparam == VK_RETURN) &&
                !widget->pressed) {
                widget->pressed = 1;
                wtk__invalidate(widget);
            }
        } else if (widget->type == WTK_TYPE_CHECKBOX) {
            if (wparam == VK_SPACE || wparam == VK_RETURN) {
                widget->checked = !widget->checked;
                wtk__invalidate(widget);
                wtk__emit_change(widget, widget->checked);
            }
        } else if (widget->type == WTK_TYPE_RADIO) {
            if (wparam == VK_SPACE || wparam == VK_RETURN) {
                WtkWidget *group = widget->parent;
                int index = wtk__radio_index(widget);
                if (group && group->type == WTK_TYPE_RADIOGROUP && index >= 0)
                    wtk__radio_select(group, index, 0, 1);
            } else if (wparam == VK_LEFT || wparam == VK_UP) {
                wtk__radio_step(widget, -1);
            } else if (wparam == VK_RIGHT || wparam == VK_DOWN) {
                wtk__radio_step(widget, 1);
            }
        } else if (widget->type == WTK_TYPE_SLIDER) {
            int page = widget->value_step > 0
                           ? widget->value_step * 10
                           : 10;
            if (wparam == VK_LEFT || wparam == VK_DOWN)
                slider_adjust(widget, -widget->value_step);
            else if (wparam == VK_RIGHT || wparam == VK_UP)
                slider_adjust(widget, widget->value_step);
            else if (wparam == VK_PRIOR)
                slider_adjust(widget, page);
            else if (wparam == VK_NEXT)
                slider_adjust(widget, -page);
            else if (wparam == VK_HOME)
                slider_adjust(widget, widget->value_min - widget->value);
            else if (wparam == VK_END)
                slider_adjust(widget, widget->value_max - widget->value);
        }
        return 0;
    }

    case WM_KEYUP:
        if (widget->type == WTK_TYPE_BUTTON && widget->pressed &&
            (wparam == VK_SPACE || wparam == VK_RETURN)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            button_activate(widget, (rc.right - rc.left) / 2,
                            (rc.bottom - rc.top) / 2);
            if (widget->pending_free)
                return 0;
        }
        {
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_KEY_UP;
            event.key = (int)wparam;
            event.modifiers = current_modifiers();
            wtk__dispatch(widget, &event);
        }
        return 0;

    case WM_CHAR:
        /* Keep Space/Enter from producing the default beep. */
        if (wparam == L' ' || wparam == L'\r' || wparam == L'\t')
            return 0;
        break;

    case WM_SETFOCUS:
        widget->focused = 1;
        wtk__invalidate(widget);
        wtk__emit(widget, WTK_EVENT_FOCUS_IN);
        return 0;

    case WM_KILLFOCUS:
        widget->focused = 0;
        wtk__invalidate(widget);
        wtk__emit(widget, WTK_EVENT_FOCUS_OUT);
        return 0;

    case WM_CAPTURECHANGED:
        if (widget->pressed) {
            widget->pressed = 0;
            wtk__invalidate(widget);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(cursor_handle(widget->cursor));
            return TRUE;
        }
        break;

    case WM_MOUSEWHEEL: {
        WtkEvent event;
        POINT pt;
        pt.x = (short)LOWORD(lparam);
        pt.y = (short)HIWORD(lparam);
        ScreenToClient(hwnd, &pt);
        memset(&event, 0, sizeof(event));
        event.type = WTK_EVENT_MOUSE_WHEEL;
        event.x = pt.x;
        event.y = pt.y;
        event.delta = GET_WHEEL_DELTA_WPARAM(wparam);
        event.modifiers = current_modifiers();
        wtk__dispatch(widget, &event);
        return 0;
    }

    case WM_GETDLGCODE:
        if (widget->focusable)
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        break;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void wtk__register_widget_class(HINSTANCE instance)
{
    WNDCLASSEXW wc;
    if (GetClassInfoW(instance, WTK_CLASS_WIDGET, NULL))
        return;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = widget_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = WTK_CLASS_WIDGET;
    RegisterClassExW(&wc);
}

WtkWidget *wtk__create_widget(WtkWidget *parent, WtkWidgetType type,
                              const char *text)
{
    WtkApp *app;
    WtkWidget *widget;
    DWORD style;
    WCHAR *wtext;

    if (!parent || !parent->app)
        return NULL;
    app = parent->app;
    widget = wtk__alloc_widget(app, parent, type);
    if (!widget)
        return NULL;

    wtext = wtk__u8_to_u16(text ? text : "");
    free(widget->text);
    widget->text = wtext;

    style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    if (widget->focusable && !wtk__is_field(widget))
        style |= WS_TABSTOP;

    widget->hwnd = CreateWindowExW(
        0, WTK_CLASS_WIDGET, L"", style, 0, 0, 0, 0,
        parent->hwnd, NULL, app->instance, widget);
    if (!widget->hwnd) {
        free_widget_contents(widget);
        return NULL;
    }

    if (wtk__is_field(widget))
        wtk__create_native_input(widget);

    wtk__add_child(parent, widget);
    wtk__invalidate(widget);
    return widget;
}

/* ------------------------------------------------------------------------- */
/* Generic configuration                                                     */
/* ------------------------------------------------------------------------- */

WtkApp *wtk_widget_app(WtkWidget *widget)
{
    return widget ? widget->app : NULL;
}

WtkWidget *wtk_widget_parent(WtkWidget *widget)
{
    return widget ? widget->parent : NULL;
}

HWND wtk_widget_hwnd(WtkWidget *widget)
{
    return widget ? widget->hwnd : NULL;
}

void wtk__refresh_widget_font(WtkWidget *widget)
{
    HFONT old;
    HFONT fresh;
    if (!widget)
        return;
    old = widget->font;
    fresh = create_font(widget->app, widget->font_family,
                        widget->font_pt, widget->font_style);
    if (fresh) {
        widget->font = fresh;
        if (old)
            DeleteObject(old);
    }
    wtk__native_font_changed(widget);
    wtk__request_relayout(widget);
    wtk__invalidate(widget);
}

void wtk_widget_set_font(WtkWidget *widget, const char *family,
                         int point_size, unsigned style)
{
    WCHAR *wide;
    if (!widget)
        return;
    if (!family || !family[0])
        family = wtk_app_ui_font_family(widget->app);
    wide = wtk__u8_to_u16(family);
    wcsncpy(widget->font_family, wide, LF_FACESIZE - 1);
    widget->font_family[LF_FACESIZE - 1] = L'\0';
    free(wide);
    widget->font_pt = point_size > 0 ? point_size : 10;
    widget->font_style = style;
    wtk__refresh_widget_font(widget);
}

void wtk_widget_set_background(WtkWidget *widget, WtkColor color)
{
    if (!widget)
        return;
    widget->bg = color;
    widget->bg_set = 1;
    if (wtk__is_field(widget)) {
        wtk__native_theme_changed(widget);
        wtk__invalidate_native(widget);
    }
    if (wtk__is_container(widget) && widget->hwnd) {
        RedrawWindow(widget->hwnd, NULL, NULL,
                     RDW_INVALIDATE | RDW_ALLCHILDREN);
    } else {
        wtk__invalidate(widget);
    }
}

void wtk_widget_set_foreground(WtkWidget *widget, WtkColor color)
{
    if (!widget)
        return;
    widget->fg = color;
    widget->fg_set = 1;
    if (wtk__is_field(widget)) {
        wtk__invalidate_native(widget);
    }
    wtk__invalidate(widget);
}

void wtk_widget_set_padding(WtkWidget *widget, int left, int top, int right,
                            int bottom)
{
    if (!widget)
        return;
    widget->pad_l = wtk__logical(widget->app, left > 0 ? left : 0);
    widget->pad_t = wtk__logical(widget->app, top > 0 ? top : 0);
    widget->pad_r = wtk__logical(widget->app, right > 0 ? right : 0);
    widget->pad_b = wtk__logical(widget->app, bottom > 0 ? bottom : 0);
    wtk__request_relayout(widget);
    wtk__invalidate(widget);
}

void wtk_widget_set_size(WtkWidget *widget, int width, int height)
{
    if (!widget)
        return;
    if (widget->type == WTK_TYPE_WINDOW) {
        RECT rc;
        int pw;
        int ph;
        if (!widget->hwnd)
            return;
        GetWindowRect(widget->hwnd, &rc);
        pw = width > 0 ? wtk__logical(widget->app, width)
                       : rc.right - rc.left;
        ph = height > 0 ? wtk__logical(widget->app, height)
                        : rc.bottom - rc.top;
        SetWindowPos(widget->hwnd, NULL, 0, 0, pw, ph,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    widget->req_w = width > 0 ? wtk__logical(widget->app, width) : 0;
    widget->req_h = height > 0 ? wtk__logical(widget->app, height) : 0;
    wtk__request_relayout(widget);
    wtk__invalidate(widget);
}

void wtk_widget_set_border(WtkWidget *widget, WtkColor color, int width,
                           int radius)
{
    if (!widget)
        return;
    widget->border = color;
    widget->border_set = 1;
    widget->border_width = width > 0 ? wtk__logical(widget->app, width) : 0;
    widget->border_radius = radius >= 0 ? wtk__logical(widget->app, radius) : 0;
    wtk__request_relayout(widget);
    wtk__invalidate(widget);
}

void wtk_widget_set_visible(WtkWidget *widget, int visible)
{
    if (!widget)
        return;
    visible = !!visible;
    if (widget->visible == visible && widget->hwnd)
        return;
    widget->visible = visible;
    if (!widget->hwnd)
        return;

    if (widget->type == WTK_TYPE_WINDOW) {
        ShowWindow(widget->hwnd, visible ? SW_SHOW : SW_HIDE);
        widget->shown = visible;
        if (visible)
            wtk__layout_window(widget);
    } else {
        ShowWindow(widget->hwnd, visible ? SW_SHOWNA : SW_HIDE);
        wtk__request_relayout(widget->parent ? widget->parent : widget);
    }
}

void wtk_widget_set_enabled(WtkWidget *widget, int enabled)
{
    if (!widget)
        return;
    enabled = !!enabled;
    if (widget->enabled == enabled)
        return;
    widget->enabled = enabled;
    if (widget->hwnd)
        EnableWindow(widget->hwnd, enabled);
    if (widget->input)
        EnableWindow(widget->input, enabled);
    wtk__native_enabled_changed(widget);
    if (!enabled)
        widget->focused = 0;
    wtk__invalidate(widget);
}

void wtk_widget_set_cursor(WtkWidget *widget, WtkCursor cursor)
{
    if (!widget)
        return;
    widget->cursor = cursor;
    if (widget->hwnd)
        SendMessageW(widget->hwnd, WM_SETCURSOR,
                     (WPARAM)widget->hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

void wtk_widget_focus(WtkWidget *widget)
{
    HWND target;
    if (!widget)
        return;
    target = wtk__focus_target(widget);
    if (target)
        SetFocus(target);
    else if (widget->parent && widget->parent->hwnd)
        wtk__focus_next(wtk__toplevel(widget), 0);
}

void wtk_widget_redraw(WtkWidget *widget)
{
    if (!widget)
        return;
    wtk__invalidate_native(widget);
}

static void mark_pending(WtkWidget *widget)
{
    int i;
    if (!widget)
        return;
    widget->pending_free = 1;
    for (i = 0; i < widget->child_count; ++i)
        mark_pending(widget->children[i]);
}

void wtk_widget_destroy(WtkWidget *widget)
{
    if (!widget || widget->pending_free)
        return;

    if (widget->type == WTK_TYPE_WINDOW) {
        if (widget->hwnd && IsWindow(widget->hwnd))
            DestroyWindow(widget->hwnd);
        return;
    }

    {
        WtkWidget *parent = widget->parent;
        mark_pending(widget);
        if (parent)
            wtk__remove_child(parent, widget);
        if (widget->hwnd && IsWindow(widget->hwnd))
            DestroyWindow(widget->hwnd);
        widget->hwnd = NULL;
        widget->input = NULL;
        if (parent) {
            wtk__request_relayout(parent);
            wtk__invalidate(parent);
        }
        wtk__queue_graveyard(widget);
    }
}

/* ------------------------------------------------------------------------- */
/* Leaf widget implementations                                               */
/* ------------------------------------------------------------------------- */

WtkWidget *wtk_frame(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_FRAME, "");
}

WtkWidget *wtk_label(WtkWidget *parent, const char *text)
{
    return wtk__create_widget(parent, WTK_TYPE_LABEL, text);
}

WtkWidget *wtk_button(WtkWidget *parent, const char *text)
{
    return wtk__create_widget(parent, WTK_TYPE_BUTTON, text);
}

WtkWidget *wtk_entry(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_ENTRY, "");
}

WtkWidget *wtk_text(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_TEXT, "");
}

WtkWidget *wtk_listbox(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_LISTBOX, "");
}

WtkWidget *wtk_checkbox(WtkWidget *parent, const char *text, int checked)
{
    WtkWidget *widget = wtk__create_widget(parent, WTK_TYPE_CHECKBOX, text);
    if (widget)
        widget->checked = !!checked;
    return widget;
}

WtkWidget *wtk_radiogroup(WtkWidget *parent)
{
    WtkWidget *group = wtk__create_widget(parent, WTK_TYPE_RADIOGROUP, "");
    if (group)
        group->value = -1;
    return group;
}

WtkWidget *wtk_radio(WtkWidget *group, const char *text)
{
    WtkWidget *radio;
    int index;
    if (!group || group->type != WTK_TYPE_RADIOGROUP)
        return NULL;
    radio = wtk__create_widget(group, WTK_TYPE_RADIO, text);
    if (!radio)
        return NULL;
    index = 0;
    {
        int i;
        for (i = 0; i < group->child_count; ++i) {
            if (group->children[i] != radio &&
                group->children[i]->type == WTK_TYPE_RADIO)
                index++;
        }
    }
    if (group->value < 0)
        group->value = index;
    return radio;
}

WtkWidget *wtk_slider(WtkWidget *parent, int minimum, int maximum, int value)
{
    WtkWidget *slider;
    if (maximum == minimum)
        maximum = minimum + 1;
    if (maximum < minimum) {
        int tmp = minimum;
        minimum = maximum;
        maximum = tmp;
    }
    slider = wtk__create_widget(parent, WTK_TYPE_SLIDER, "");
    if (!slider)
        return NULL;
    slider->value_min = minimum;
    slider->value_max = maximum;
    if (value < minimum)
        value = minimum;
    if (value > maximum)
        value = maximum;
    slider->value = value;
    slider->value_step = 1;
    return slider;
}

WtkWidget *wtk_progress(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_PROGRESS, "");
}

WtkWidget *wtk_separator(WtkWidget *parent)
{
    return wtk__create_widget(parent, WTK_TYPE_SEPARATOR, "");
}

WtkWidget *wtk_canvas(WtkWidget *parent, int width, int height)
{
    WtkWidget *canvas =
        wtk__create_widget(parent, WTK_TYPE_CANVAS, "");
    if (!canvas)
        return NULL;
    if (width <= 0)
        width = 320;
    if (height <= 0)
        height = 200;
    canvas->req_w = wtk__logical(canvas->app, width);
    canvas->req_h = wtk__logical(canvas->app, height);
    return canvas;
}

void wtk_label_set_text(WtkWidget *label, const char *text)
{
    if (label && label->type == WTK_TYPE_LABEL)
        wtk__set_widget_text(label, text);
}

const char *wtk_label_get_text(WtkWidget *label)
{
    if (!label || label->type != WTK_TYPE_LABEL)
        return "";
    return wtk__get_cached_text(label);
}

void wtk_label_set_align(WtkWidget *label, WtkAlign align)
{
    if (!label || label->type != WTK_TYPE_LABEL)
        return;
    label->align = align;
    wtk__invalidate(label);
}

void wtk_label_set_wrap_width(WtkWidget *label, int logical_width)
{
    if (!label || label->type != WTK_TYPE_LABEL)
        return;
    label->wrap_width =
        logical_width > 0 ? wtk__logical(label->app, logical_width) : 0;
    wtk__request_relayout(label);
    wtk__invalidate(label);
}

void wtk_button_set_text(WtkWidget *button, const char *text)
{
    if (button && button->type == WTK_TYPE_BUTTON)
        wtk__set_widget_text(button, text);
}

const char *wtk_button_get_text(WtkWidget *button)
{
    if (!button || button->type != WTK_TYPE_BUTTON)
        return "";
    return wtk__get_cached_text(button);
}

void wtk_button_set_style(WtkWidget *button, WtkButtonStyle style)
{
    if (!button || button->type != WTK_TYPE_BUTTON)
        return;
    button->button_style = style;
    wtk__invalidate(button);
}

void wtk_button_set_align(WtkWidget *button, WtkAlign align)
{
    if (!button || button->type != WTK_TYPE_BUTTON)
        return;
    button->align = align;
    wtk__invalidate(button);
}

int wtk_checkbox_get(WtkWidget *checkbox)
{
    return (checkbox && checkbox->type == WTK_TYPE_CHECKBOX)
               ? checkbox->checked
               : 0;
}

void wtk_checkbox_set(WtkWidget *checkbox, int checked)
{
    if (!checkbox || checkbox->type != WTK_TYPE_CHECKBOX)
        return;
    checkbox->checked = !!checked;
    wtk__invalidate(checkbox);
}

int wtk_radiogroup_get(WtkWidget *group)
{
    return (group && group->type == WTK_TYPE_RADIOGROUP) ? group->value : -1;
}

void wtk__radio_select(WtkWidget *group, int index, int focus, int notify)
{
    int i;
    int current = 0;
    WtkWidget *target = NULL;
    int changed;

    if (!group || group->type != WTK_TYPE_RADIOGROUP)
        return;

    for (i = 0; i < group->child_count; ++i) {
        WtkWidget *child = group->children[i];
        if (child->type != WTK_TYPE_RADIO)
            continue;
        if (current == index)
            target = child;
        current++;
    }
    if (!target || index < 0)
        return;

    changed = group->value != index;
    group->value = index;
    for (i = 0; i < group->child_count; ++i) {
        if (group->children[i]->type == WTK_TYPE_RADIO)
            wtk__invalidate(group->children[i]);
    }
    wtk__invalidate(group);

    if (focus)
        SetFocus(target->hwnd);
    if (notify && changed)
        wtk__emit_change(group, index);
}

void wtk_radiogroup_set(WtkWidget *group, int index)
{
    wtk__radio_select(group, index, 0, 0);
}

void wtk__radio_step(WtkWidget *radio, int direction)
{
    WtkWidget *group;
    int count = 0;
    int i;
    int index;
    int next;

    if (!radio || radio->type != WTK_TYPE_RADIO || !radio->parent ||
        radio->parent->type != WTK_TYPE_RADIOGROUP)
        return;
    group = radio->parent;
    for (i = 0; i < group->child_count; ++i) {
        if (group->children[i]->type == WTK_TYPE_RADIO)
            count++;
    }
    if (count <= 1)
        return;
    index = group->value >= 0 ? group->value : 0;
    next = (index + direction + count) % count;
    wtk__radio_select(group, next, 1, 1);
}

int wtk_slider_get(WtkWidget *slider)
{
    return (slider && slider->type == WTK_TYPE_SLIDER) ? slider->value : 0;
}

void wtk_slider_set(WtkWidget *slider, int value)
{
    if (!slider || slider->type != WTK_TYPE_SLIDER)
        return;
    if (value < slider->value_min)
        value = slider->value_min;
    if (value > slider->value_max)
        value = slider->value_max;
    if (slider->value == value)
        return;
    slider->value = value;
    wtk__invalidate(slider);
}

void wtk_slider_set_step(WtkWidget *slider, int step)
{
    if (!slider || slider->type != WTK_TYPE_SLIDER)
        return;
    slider->value_step = step > 0 ? step : 1;
}

void wtk_progress_set(WtkWidget *progress, double fraction)
{
    if (!progress || progress->type != WTK_TYPE_PROGRESS)
        return;
    if (fraction < 0.0)
        fraction = 0.0;
    if (fraction > 1.0)
        fraction = 1.0;
    progress->progress = fraction;
    wtk__invalidate(progress);
}

double wtk_progress_get(WtkWidget *progress)
{
    return (progress && progress->type == WTK_TYPE_PROGRESS)
               ? progress->progress
               : 0.0;
}

void wtk_progress_set_show_percent(WtkWidget *progress, int show)
{
    if (!progress || progress->type != WTK_TYPE_PROGRESS)
        return;
    progress->show_percent = !!show;
    wtk__request_relayout(progress);
    wtk__invalidate(progress);
}

void wtk_canvas_set_draw(WtkWidget *canvas, WtkDrawFn callback,
                         void *userdata)
{
    if (!canvas || canvas->type != WTK_TYPE_CANVAS)
        return;
    canvas->draw_callback = callback;
    canvas->draw_data = userdata;
    wtk__invalidate(canvas);
}

void wtk_canvas_redraw(WtkWidget *canvas)
{
    if (canvas && canvas->type == WTK_TYPE_CANVAS)
        wtk__invalidate(canvas);
}
