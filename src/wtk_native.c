#include "wtk_internal.h"
#include <commctrl.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static unsigned native_modifiers(void)
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

static void native_emit_key(WtkWidget *widget, WtkEventType type, WPARAM key)
{
    WtkEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key = (int)key;
    event.modifiers = native_modifiers();
    wtk__dispatch(widget, &event);
}

void wtk__native_refresh_brush(WtkWidget *widget)
{
    WtkColor bg;
    if (!widget || (!wtk__is_field(widget) &&
                    !wtk__uses_native_control(widget)))
        return;
    if (widget->input_brush) {
        DeleteObject(widget->input_brush);
        widget->input_brush = NULL;
    }
    bg = wtk__is_field(widget)
             ? wtk__field_bg(widget)
             : (widget->bg_set
                    ? widget->bg
                    : (widget->parent ? wtk__container_bg(widget->parent)
                                      : widget->app->theme.surface));
    widget->input_brush = CreateSolidBrush(wtk__colorref(bg));
}

HBRUSH wtk__native_ctlcolor(WtkWidget *widget, HDC dc, UINT message)
{
    WtkColor bg;
    WtkColor fg;
    if (!widget ||
        (!wtk__is_field(widget) && !wtk__uses_native_control(widget)))
        return (HBRUSH)GetStockObject(WHITE_BRUSH);
    (void)message;

    if (wtk__is_field(widget)) {
        bg = wtk__field_bg(widget);
        fg = wtk__field_fg(widget);
    } else {
        bg = widget->bg_set
                 ? widget->bg
                 : (widget->parent ? wtk__container_bg(widget->parent)
                                   : widget->app->theme.surface);
        fg = widget->fg_set ? widget->fg : widget->app->theme.text;
    }
    if (!widget->input_brush)
        wtk__native_refresh_brush(widget);

    SetBkColor(dc, wtk__colorref(bg));
    SetTextColor(dc, wtk__colorref(fg));
    SetBkMode(dc, widget->type == WTK_TYPE_LABEL ||
                          widget->type == WTK_TYPE_FRAME ||
                          widget->type == WTK_TYPE_RADIOGROUP
                      ? TRANSPARENT
                      : OPAQUE);
    return widget->input_brush;
}

void wtk__native_theme_changed(WtkWidget *widget)
{
    if (!widget || (!wtk__is_field(widget) &&
                    !wtk__uses_native_control(widget)))
        return;
    wtk__native_refresh_brush(widget);
    wtk__native_position(widget);
    if (widget->input)
        InvalidateRect(widget->input, NULL, TRUE);
    wtk__invalidate(widget);
}

void wtk__native_label_style(WtkWidget *widget)
{
    LONG_PTR style;
    if (!widget || widget->type != WTK_TYPE_LABEL ||
        !wtk__uses_native_control(widget) || !widget->input)
        return;
    style = GetWindowLongPtrW(widget->input, GWL_STYLE);
    style &= ~(SS_TYPEMASK | SS_NOPREFIX | SS_ENDELLIPSIS);
    if (widget->align == WTK_ALIGN_CENTER)
        style |= SS_CENTER;
    else if (widget->align == WTK_ALIGN_RIGHT)
        style |= SS_RIGHT;
    else
        style |= SS_LEFT;
    style |= SS_NOPREFIX;
    if (widget->wrap_width <= 0)
        style |= SS_ENDELLIPSIS;
    SetWindowLongPtrW(widget->input, GWL_STYLE, style);
    SetWindowPos(widget->input, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(widget->input, NULL, TRUE);
}

void wtk__native_font_changed(WtkWidget *widget)
{
    if (!widget || !widget->input ||
        (!wtk__is_field(widget) && !wtk__uses_native_control(widget)))
        return;
    SendMessageW(widget->input, WM_SETFONT, (WPARAM)widget->font, TRUE);
    wtk__native_position(widget);
    InvalidateRect(widget->input, NULL, TRUE);
}

void wtk__native_enabled_changed(WtkWidget *widget)
{
    if (!widget || (!wtk__is_field(widget) &&
                    !wtk__uses_native_control(widget)))
        return;
    if (widget->input)
        EnableWindow(widget->input, widget->enabled);
    wtk__native_refresh_brush(widget);
    wtk__invalidate_native(widget);
}

static void draw_placeholder(WtkWidget *widget, HDC dc)
{
    RECT rc;
    RECT text_rect;
    HFONT old_font;
    int left_margin;
    int line_height;
    TEXTMETRICW tm;

    if (!widget || !widget->placeholder || !widget->placeholder[0])
        return;
    if (!widget->input || GetFocus() == widget->input)
        return;
    if (GetWindowTextLengthW(widget->input) > 0)
        return;

    GetClientRect(widget->input, &rc);
    left_margin = (int)LOWORD(
        SendMessageW(widget->input, EM_GETMARGINS, 0, 0));
    if (left_margin < 1)
        left_margin = wtk__scale_widget(widget, 5);

    text_rect = rc;
    text_rect.left += left_margin;
    text_rect.right -= wtk__scale_widget(widget, 3);

    old_font = (HFONT)SelectObject(dc, widget->font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, wtk__colorref(
                         wtk__style_is_native(widget)
                             ? wtk__system_color(COLOR_GRAYTEXT)
                             : widget->app->theme.muted));
    if (!GetTextMetricsW(dc, &tm))
        tm.tmHeight = 16;
    line_height = tm.tmHeight;

    if (widget->type == WTK_TYPE_ENTRY) {
        text_rect.top = (rc.bottom - rc.top - line_height) / 2;
        text_rect.bottom = text_rect.top + line_height;
        DrawTextW(dc, widget->placeholder, -1, &text_rect,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
                      DT_END_ELLIPSIS);
    } else {
        text_rect.top += wtk__scale_widget(widget, 4);
        DrawTextW(dc, widget->placeholder, -1, &text_rect,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }
    SelectObject(dc, old_font);
}

static LRESULT CALLBACK input_subclass_proc(HWND hwnd, UINT message,
                                            WPARAM wparam, LPARAM lparam,
                                            UINT_PTR subclass_id,
                                            DWORD_PTR ref_data)
{
    WtkWidget *widget = (WtkWidget *)ref_data;
    LRESULT result;
    (void)subclass_id;

    if (!widget || widget->pending_free)
        return DefSubclassProc(hwnd, message, wparam, lparam);

    switch (message) {
    case WM_PAINT:
        result = DefSubclassProc(hwnd, message, wparam, lparam);
        {
            HDC dc = GetDC(hwnd);
            draw_placeholder(widget, dc);
            ReleaseDC(hwnd, dc);
        }
        return result;

    case WM_PRINTCLIENT:
    case WM_PRINT:
        result = DefSubclassProc(hwnd, message, wparam, lparam);
        draw_placeholder(widget, (HDC)wparam);
        return result;

    case WM_KEYDOWN:
        native_emit_key(widget, WTK_EVENT_KEY_DOWN, wparam);
        if (widget->pending_free)
            return 0;
        if (widget->type == WTK_TYPE_ENTRY && wparam == VK_RETURN) {
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_SUBMIT;
            wtk__dispatch(widget, &event);
            return 0;
        }
        if (widget->type == WTK_TYPE_TEXT && wparam == VK_RETURN &&
            (GetKeyState(VK_CONTROL) & 0x8000)) {
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_SUBMIT;
            wtk__dispatch(widget, &event);
            return 0;
        }
        break;

    case WM_KEYUP:
        native_emit_key(widget, WTK_EVENT_KEY_UP, wparam);
        if (widget->pending_free)
            return 0;
        break;

    case WM_CHAR:
        if (widget->type == WTK_TYPE_ENTRY &&
            (wparam == L'\r' || wparam == L'\n'))
            return 0;
        if (widget->type == WTK_TYPE_TEXT && wparam == L'\r' &&
            (GetKeyState(VK_CONTROL) & 0x8000))
            return 0;
        break;

    default:
        break;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

void wtk__create_native_input(WtkWidget *widget)
{
    WtkApp *app;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS;
    const WCHAR *class_name;
    HWND input;

    if (!widget || !widget->hwnd)
        return;
    if (!wtk__is_field(widget) && !wtk__uses_native_control(widget))
        return;
    app = widget->app;

    if (wtk__uses_native_control(widget)) {
        switch (widget->type) {
        case WTK_TYPE_LABEL: {
            DWORD align = SS_LEFT;
            if (widget->align == WTK_ALIGN_CENTER)
                align = SS_CENTER;
            else if (widget->align == WTK_ALIGN_RIGHT)
                align = SS_RIGHT;
            class_name = L"STATIC";
            style |= align | SS_NOPREFIX;
            if (widget->wrap_width <= 0)
                style |= SS_ENDELLIPSIS;
            break;
        }
        case WTK_TYPE_FRAME:
        case WTK_TYPE_RADIOGROUP:
            class_name = L"BUTTON";
            style |= BS_GROUPBOX;
            break;
        case WTK_TYPE_SEPARATOR:
            class_name = L"STATIC";
            style |= SS_ETCHEDHORZ;
            break;
        case WTK_TYPE_BUTTON:
            class_name = L"BUTTON";
            style |= (widget->button_style == WTK_BUTTON_PRIMARY
                          ? BS_DEFPUSHBUTTON
                          : BS_PUSHBUTTON) |
                     WS_TABSTOP;
            break;
        case WTK_TYPE_CHECKBOX:
            class_name = L"BUTTON";
            style |= BS_AUTOCHECKBOX | WS_TABSTOP;
            break;
        case WTK_TYPE_RADIO:
            class_name = L"BUTTON";
            style |= BS_RADIOBUTTON | WS_TABSTOP;
            break;
        case WTK_TYPE_SLIDER:
            class_name = TRACKBAR_CLASSW;
            style |= TBS_HORZ | TBS_NOTICKS | WS_TABSTOP;
            break;
        case WTK_TYPE_PROGRESS:
            class_name = PROGRESS_CLASSW;
            style |= PBS_SMOOTH;
            break;
        default:
            return;
        }
    } else if (widget->type == WTK_TYPE_ENTRY) {
        class_name = L"EDIT";
        style |= ES_LEFT | ES_AUTOHSCROLL | ES_NOHIDESEL | WS_BORDER;
    } else if (widget->type == WTK_TYPE_TEXT) {
        class_name = L"EDIT";
        style |= ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                 ES_NOHIDESEL | WS_VSCROLL | WS_BORDER;
    } else if (widget->type == WTK_TYPE_TREE) {
        class_name = WC_TREEVIEWW;
        style |= TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
                 TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP | WS_BORDER;
    } else if (widget->virtual_count > 0) {
        /* 虚拟列表：条目本身不存字符串，只存数量，绘制靠 WM_DRAWITEM */
        class_name = L"LISTBOX";
        style |= LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED |
                 LBS_NODATA | WS_VSCROLL | WS_BORDER;
    } else {
        class_name = L"LISTBOX";
        style |= LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS |
                 WS_VSCROLL | WS_BORDER;
    }

    input = CreateWindowExW(
        0, class_name, L"", style, 0, 0, 0, 0, widget->hwnd,
        (HMENU)(UINT_PTR)WTK_INPUT_ID, app->instance, NULL);
    if (!input)
        return;

    widget->input = input;
    SendMessageW(input, WM_SETFONT, (WPARAM)widget->font, TRUE);

    if (wtk__uses_native_control(widget)) {
        if (widget->type == WTK_TYPE_LABEL ||
            widget->type == WTK_TYPE_BUTTON ||
            widget->type == WTK_TYPE_CHECKBOX ||
            widget->type == WTK_TYPE_RADIO ||
            widget->type == WTK_TYPE_FRAME ||
            widget->type == WTK_TYPE_RADIOGROUP) {
            SetWindowTextW(input, widget->text ? widget->text : L"");
            if (widget->type == WTK_TYPE_CHECKBOX ||
                widget->type == WTK_TYPE_RADIO)
                SendMessageW(input, BM_SETCHECK,
                             widget->checked ? BST_CHECKED : BST_UNCHECKED,
                             0);
        } else if (widget->type == WTK_TYPE_SLIDER) {
            SendMessageW(input, TBM_SETRANGE, TRUE,
                         MAKELPARAM(widget->value_min, widget->value_max));
            SendMessageW(input, TBM_SETPOS, TRUE, widget->value);
            SendMessageW(input, TBM_SETLINESIZE, 0, widget->value_step);
            SendMessageW(input, TBM_SETPAGESIZE, 0, widget->value_step * 10);
        } else if (widget->type == WTK_TYPE_PROGRESS) {
            SendMessageW(input, PBM_SETRANGE32, 0, 1000);
            SendMessageW(input, PBM_SETPOS,
                         (WPARAM)(widget->progress * 1000.0), 0);
        }
        wtk__native_refresh_brush(widget);
        wtk__native_position(widget);
        EnableWindow(input, widget->enabled);
        return;
    }

    if (widget->type == WTK_TYPE_LISTBOX && widget->virtual_count > 0) {
        SendMessageW(input, LB_SETITEMHEIGHT, 0,
                     (LPARAM)wtk__native_virtual_height(widget));
        SendMessageW(input, LB_SETCOUNT, (WPARAM)widget->virtual_count, 0);
    }

    if (widget->type == WTK_TYPE_ENTRY ||
        widget->type == WTK_TYPE_TEXT) {
        /* 原生 EDIT 默认只接受约 32K 字符，较大的 JSON/日志会被静默截断。 */
        SendMessageW(input, EM_SETLIMITTEXT, (WPARAM)0x7FFFFFFE, 0);
        SetWindowSubclass(input, input_subclass_proc, WTK_INPUT_SUBCLASS,
                          (DWORD_PTR)widget);
        SendMessageW(input, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(wtk__scale_widget(widget, 5),
                                wtk__scale_widget(widget, 5)));
    }

    if (widget->readonly)
        SendMessageW(input, EM_SETREADONLY, TRUE, 0);
    if (widget->password && widget->type == WTK_TYPE_ENTRY)
        SendMessageW(input, EM_SETPASSWORDCHAR, 0x25CF, 0);

    wtk__native_refresh_brush(widget);
    wtk__native_position(widget);
    EnableWindow(input, widget->enabled);
}

static int entry_line_height(WtkWidget *widget)
{
    HDC dc;
    HFONT font;
    HGDIOBJ old_font = NULL;
    TEXTMETRICW tm;
    int height = 0;

    if (!widget || !widget->input)
        return 0;
    dc = GetDC(widget->input);
    if (!dc)
        return 0;
    font = (HFONT)SendMessageW(widget->input, WM_GETFONT, 0, 0);
    if (font)
        old_font = SelectObject(dc, font);
    if (GetTextMetricsW(dc, &tm))
        height = tm.tmHeight + tm.tmExternalLeading;
    if (old_font)
        SelectObject(dc, old_font);
    ReleaseDC(widget->input, dc);
    return height;
}

void wtk__native_position(WtkWidget *widget)
{
    RECT rc;
    int width;
    int height;
    int inset_x;
    int inset_y;
    int child_x;
    int child_y;
    int child_w;
    int child_h;

    if (!widget || !widget->input || !widget->hwnd)
        return;
    GetClientRect(widget->hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    if (wtk__uses_native_control(widget)) {
        SetWindowPos(widget->input, NULL, 0, 0, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    if (widget->app->style_painter &&
        widget->app->style_painter->field_metrics) {
        WtkPaintState state;
        int text_margin = -1;
        wtk__build_paint_state(widget, &state);
        inset_x = wtk__scale_widget(widget, 4);
        inset_y = wtk__scale_widget(widget,
                                    widget->type == WTK_TYPE_ENTRY ? 2 : 4);
        widget->style_margin = wtk__scale_widget(widget, 4);
        widget->app->style_painter->field_metrics(
            widget, &state, &inset_x, &inset_y, &text_margin,
            widget->app->style_painter_data);
        if (text_margin >= 0)
            widget->style_margin = text_margin;
    } else if (widget->type == WTK_TYPE_ENTRY) {
        inset_x = wtk__scale_widget(widget, 7);
        inset_y = wtk__scale_widget(widget, 3);
    } else {
        inset_x = wtk__scale_widget(widget, 7);
        inset_y = wtk__scale_widget(widget, 7);
    }

    if (width < 2 * inset_x)
        width = 2 * inset_x;
    if (height < 2 * inset_y)
        height = 2 * inset_y;

    child_x = inset_x;
    child_y = inset_y;
    child_w = width - 2 * inset_x;
    child_h = height - 2 * inset_y;

    /* A single-line EDIT keeps its text at the top of the client area. Size
     * the child to one line and center it in the host so the text and caret
     * share the vertical center of the visible field. */
    if (widget->type == WTK_TYPE_ENTRY) {
        int line_h = entry_line_height(widget);
        if (line_h > 0) {
            /* The caret is one text line tall and sits at the top of the
             * EDIT client area. Centering a line-height child therefore
             * centers the text and caret together. */
            if (line_h > child_h)
                line_h = child_h;
            child_y = inset_y + (child_h - line_h) / 2;
            child_h = line_h;
        }
    }

    if (child_w < 1)
        child_w = 1;
    if (child_h < 1)
        child_h = 1;
    SetWindowPos(widget->input, NULL, child_x, child_y, child_w, child_h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (widget->type == WTK_TYPE_ENTRY ||
        widget->type == WTK_TYPE_TEXT) {
        int margin = widget->style_margin >= 0
                         ? widget->style_margin
                         : wtk__scale_widget(widget, 5);
        SendMessageW(widget->input, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(margin, margin));
    }
}

/* Native WM_NOTIFY (tree control) */
void wtk__native_notify(WtkWidget *widget, LPARAM lparam)
{
    NMHDR *hdr = (NMHDR *)lparam;
    if (!widget || !hdr || hdr->hwndFrom != widget->input)
        return;
    if (widget->type != WTK_TYPE_TREE)
        return;
    if (hdr->code == TVN_SELCHANGEDW) {
        NMTREEVIEWW *nm = (NMTREEVIEWW *)lparam;
        WtkEvent event;
        /* 程序自己清空/重建树时也会收到这个通知：用 suppress_events 挡掉，
         * 否则应用会被"非用户操作"的选中变化带着跑。 */
        if (widget->suppress_change)
            return;
        memset(&event, 0, sizeof(event));
        event.type = WTK_EVENT_CHANGE;
        event.value = (int)(INT_PTR)nm->itemNew.lParam;
        wtk__dispatch(widget, &event);
    } else if (hdr->code == NM_DBLCLK) {
        WtkEvent event;
        HTREEITEM h;
        int index = -1;
        if (widget->suppress_change)
            return;
        h = (HTREEITEM)SendMessageW(widget->input, TVM_GETNEXTITEM, TVGN_CARET, 0);
        if (h) {
            TVITEMW item;
            memset(&item, 0, sizeof(item));
            item.mask = TVIF_PARAM;
            item.hItem = h;
            if (SendMessageW(widget->input, TVM_GETITEMW, 0, (LPARAM)&item))
                index = (int)item.lParam;
        }
        memset(&event, 0, sizeof(event));
        event.type = WTK_EVENT_DOUBLE_CLICK;
        event.button = WTK_MOUSE_LEFT;
        event.clicks = 2;
        event.value = index;
        wtk__dispatch(widget, &event);
    }
}

void wtk__native_command(WtkWidget *widget, WPARAM wparam, LPARAM lparam)
{
    int code;
    if (!widget || !widget->input || (HWND)lparam != widget->input)
        return;
    code = HIWORD(wparam);

    if (widget->type == WTK_TYPE_ENTRY || widget->type == WTK_TYPE_TEXT) {
        switch (code) {
        case EN_CHANGE:
            wtk__invalidate_text_cache(widget);
            if (widget->input)
                InvalidateRect(widget->input, NULL, TRUE);
            if (!widget->suppress_change)
                wtk__emit_change(widget, 0);
            break;
        case EN_SETFOCUS:
            widget->focused = 1;
            wtk__invalidate(widget);
            if (widget->input)
                InvalidateRect(widget->input, NULL, TRUE);
            wtk__emit(widget, WTK_EVENT_FOCUS_IN);
            break;
        case EN_KILLFOCUS:
            widget->focused = 0;
            wtk__invalidate(widget);
            if (widget->input)
                InvalidateRect(widget->input, NULL, TRUE);
            wtk__emit(widget, WTK_EVENT_FOCUS_OUT);
            break;
        default:
            break;
        }
    } else if (widget->type == WTK_TYPE_LISTBOX) {
        switch (code) {
        case LBN_SELCHANGE: {
            int index = (int)SendMessageW(widget->input, LB_GETCURSEL, 0, 0);
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_CHANGE;
            event.value = index;
            wtk__dispatch(widget, &event);
            break;
        }
        case LBN_DBLCLK: {
            int index = (int)SendMessageW(widget->input, LB_GETCURSEL, 0, 0);
            WtkEvent event;
            memset(&event, 0, sizeof(event));
            event.type = WTK_EVENT_DOUBLE_CLICK;
            event.button = WTK_MOUSE_LEFT;
            event.clicks = 2;
            event.value = index;
            wtk__dispatch(widget, &event);
            break;
        }
        case LBN_SETFOCUS:
            widget->focused = 1;
            wtk__invalidate(widget);
            wtk__emit(widget, WTK_EVENT_FOCUS_IN);
            break;
        case LBN_KILLFOCUS:
            widget->focused = 0;
            wtk__invalidate(widget);
            wtk__emit(widget, WTK_EVENT_FOCUS_OUT);
            break;
        default:
            break;
        }
    } else if (wtk__uses_native_control(widget)) {
        switch (code) {
        case BN_CLICKED:
            if (widget->type == WTK_TYPE_BUTTON) {
                wtk__emit(widget, WTK_EVENT_CLICK);
            } else if (widget->type == WTK_TYPE_CHECKBOX) {
                widget->checked =
                    SendMessageW(widget->input, BM_GETCHECK, 0, 0) ==
                    BST_CHECKED;
                wtk__emit_change(widget, widget->checked);
            } else if (widget->type == WTK_TYPE_RADIO) {
                WtkWidget *group = widget->parent;
                int index = wtk__radio_index(widget);
                if (group && index >= 0)
                    wtk__radio_select(group, index, 0, 1);
            }
            break;
        case BN_SETFOCUS:
            widget->focused = 1;
            wtk__emit(widget, WTK_EVENT_FOCUS_IN);
            break;
        case BN_KILLFOCUS:
            widget->focused = 0;
            wtk__emit(widget, WTK_EVENT_FOCUS_OUT);
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Native text storage                                                       */
/* ------------------------------------------------------------------------- */

/* Multiline EDIT controls use CRLF internally. Normalize the public LF
 * convention when setting, and turn CRLF back into LF when reading, so the
 * UTF-8 API stays consistent on every Windows version. */
static WCHAR *normalize_multiline_text(const WCHAR *source)
{
    size_t length = 0;
    size_t i;
    size_t j = 0;
    WCHAR *out;

    if (!source)
        return NULL;
    while (source[length])
        length++;
    out = (WCHAR *)malloc((length * 2 + 1) * sizeof(WCHAR));
    if (!out)
        return NULL;
    for (i = 0; i < length; ++i) {
        WCHAR c = source[i];
        if (c == L'\r') {
            out[j++] = L'\r';
            out[j++] = L'\n';
            if (source[i + 1] == L'\n')
                i++;
        } else if (c == L'\n') {
            out[j++] = L'\r';
            out[j++] = L'\n';
        } else {
            out[j++] = c;
        }
    }
    out[j] = 0;
    return out;
}

static void normalize_wide_to_lf(WCHAR *text)
{
    size_t read_pos;
    size_t write_pos = 0;
    if (!text)
        return;
    for (read_pos = 0; text[read_pos]; ++read_pos) {
        WCHAR c = text[read_pos];
        if (c == L'\r') {
            text[write_pos++] = L'\n';
            if (text[read_pos + 1] == L'\n')
                read_pos++;
        } else {
            text[write_pos++] = c;
        }
    }
    text[write_pos] = 0;
}

void wtk__native_set_text(WtkWidget *widget, const char *text)
{
    WCHAR *wide;
    if (!widget || !wtk__is_field(widget) || !widget->input)
        return;
    wide = wtk__u8_to_u16(text ? text : "");
    if (widget->type == WTK_TYPE_TEXT && wide) {
        WCHAR *normalized = normalize_multiline_text(wide);
        if (normalized) {
            free(wide);
            wide = normalized;
        }
    }
    widget->suppress_change = 1;
    SetWindowTextW(widget->input, wide);
    widget->suppress_change = 0;
    free(wide);
    wtk__invalidate_text_cache(widget);
    if (widget->input)
        InvalidateRect(widget->input, NULL, TRUE);
    wtk__invalidate(widget);
}

const char *wtk__native_get_text(WtkWidget *widget)
{
    int length;
    WCHAR *wide;
    char *utf8;
    if (!widget || !wtk__is_field(widget) || !widget->input)
        return "";
    if (widget->text_cache_valid && widget->text_cache)
        return widget->text_cache;

    length = GetWindowTextLengthW(widget->input);
    if (length < 0)
        length = 0;
    wide = (WCHAR *)wtk__alloc_zero((size_t)(length + 1) * sizeof(WCHAR));
    GetWindowTextW(widget->input, wide, length + 1);
    if (widget->type == WTK_TYPE_TEXT)
        normalize_wide_to_lf(wide);
    utf8 = wtk__u16_to_u8(wide);
    free(wide);

    free(widget->text_cache);
    widget->text_cache = utf8;
    widget->text_cache_valid = 1;
    return widget->text_cache;
}

/* ------------------------------------------------------------------------- */
/* Entry                                                                     */
/* ------------------------------------------------------------------------- */

void wtk_entry_set_text(WtkWidget *entry, const char *text)
{
    if (entry && entry->type == WTK_TYPE_ENTRY)
        wtk__native_set_text(entry, text);
}

const char *wtk_entry_get_text(WtkWidget *entry)
{
    if (!entry || entry->type != WTK_TYPE_ENTRY)
        return "";
    return wtk__native_get_text(entry);
}

void wtk_entry_set_placeholder(WtkWidget *entry, const char *text)
{
    WCHAR *wide;
    if (!entry || entry->type != WTK_TYPE_ENTRY)
        return;
    wide = wtk__u8_to_u16(text ? text : "");
    free(entry->placeholder);
    entry->placeholder = wide;
    if (entry->input)
        InvalidateRect(entry->input, NULL, TRUE);
}

void wtk_entry_set_password(WtkWidget *entry, int password)
{
    if (!entry || entry->type != WTK_TYPE_ENTRY)
        return;
    entry->password = !!password;
    if (entry->input) {
        SendMessageW(entry->input, EM_SETPASSWORDCHAR,
                     entry->password ? 0x25CF : 0, 0);
        InvalidateRect(entry->input, NULL, TRUE);
    }
}

void wtk_entry_set_readonly(WtkWidget *entry, int readonly)
{
    if (!entry || entry->type != WTK_TYPE_ENTRY)
        return;
    entry->readonly = !!readonly;
    if (entry->input)
        SendMessageW(entry->input, EM_SETREADONLY, readonly ? TRUE : FALSE, 0);
    wtk__native_refresh_brush(entry);
    wtk__invalidate_native(entry);
}

void wtk_entry_select_all(WtkWidget *entry)
{
    if (!entry || entry->type != WTK_TYPE_ENTRY || !entry->input)
        return;
    SetFocus(entry->input);
    SendMessageW(entry->input, EM_SETSEL, 0, (LPARAM)-1);
}

/* ------------------------------------------------------------------------- */
/* Multiline text                                                            */
/* ------------------------------------------------------------------------- */

void wtk_text_set_text(WtkWidget *text, const char *value)
{
    if (!text || text->type != WTK_TYPE_TEXT)
        return;
    /* 内容没变就别折腾（SetWindowText 会触发重排 + 重绘） */
    if (value && text->input) {
        const char *cur = wtk__native_get_text(text);
        if (cur && strcmp(cur, value) == 0)
            return;
    }
    if (text->input)
        SendMessageW(text->input, WM_SETREDRAW, 0, 0);
    wtk__native_set_text(text, value);
    if (text->input) {
        SendMessageW(text->input, WM_SETREDRAW, 1, 0);
        InvalidateRect(text->input, NULL, TRUE);
    }
}

/* O(1) 追加：定位到末尾再插入，不必把全文重新 SetWindowText。
 * 原来只能 wtk_text_set_text(全文)，文本越长每次追加越慢（平方级）。 */
void wtk_text_append(WtkWidget *text, const char *value)
{
    WCHAR *wide;

    if (!text || text->type != WTK_TYPE_TEXT || !text->input || !value || !value[0])
        return;
    wide = wtk__u8_to_u16(value);
    if (!wide)
        return;
    text->suppress_change = 1;
    SendMessageW(text->input, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(text->input, EM_REPLACESEL, FALSE, (LPARAM)wide);
    text->suppress_change = 0;
    free(wide);
    wtk__invalidate_text_cache(text);
    if (IsWindowVisible(text->input))
        InvalidateRect(text->input, NULL, TRUE);
}

/* 真正的控件 HWND（不是宿主窗口） */
/* 批量更新：在这对开关之间对控件做的修改不重绘，结束时只刷一次。
 * 实测（400 次文本追加）：不开 1445 ms，开了 54 ms。 */
void wtk_widget_bulk_begin(WtkWidget *widget)
{
    if (!widget || !widget->input)
        return;
    SendMessageW(widget->input, WM_SETREDRAW, 0, 0);
    widget->bulk_depth++;
}

void wtk_widget_bulk_end(WtkWidget *widget)
{
    if (!widget || !widget->input)
        return;
    if (widget->bulk_depth > 0)
        widget->bulk_depth--;
    if (widget->bulk_depth)
        return;
    SendMessageW(widget->input, WM_SETREDRAW, 1, 0);
    if (IsWindowVisible(widget->input)) {
        InvalidateRect(widget->input, NULL, TRUE);
        UpdateWindow(widget->input);
    }
}

HWND wtk_widget_input_hwnd(WtkWidget *widget)
{
    if (!widget)
        return NULL;
    return widget->input;
}

const char *wtk_text_get_text(WtkWidget *text)
{
    if (!text || text->type != WTK_TYPE_TEXT)
        return "";
    return wtk__native_get_text(text);
}

void wtk_text_set_placeholder(WtkWidget *widget, const char *placeholder)
{
    WCHAR *wide;
    if (!widget || widget->type != WTK_TYPE_TEXT)
        return;
    wide = wtk__u8_to_u16(placeholder ? placeholder : "");
    free(widget->placeholder);
    widget->placeholder = wide;
    if (widget->input)
        InvalidateRect(widget->input, NULL, TRUE);
}

void wtk_text_set_readonly(WtkWidget *text, int readonly)
{
    if (!text || text->type != WTK_TYPE_TEXT)
        return;
    text->readonly = !!readonly;
    if (text->input)
        SendMessageW(text->input, EM_SETREADONLY, readonly ? TRUE : FALSE, 0);
    wtk__native_refresh_brush(text);
    wtk__invalidate_native(text);
}

/* ------------------------------------------------------------------------- */
/* Listbox                                                                   */
/* ------------------------------------------------------------------------- */

static HWND listbox_input(WtkWidget *listbox)
{
    if (!listbox || listbox->type != WTK_TYPE_LISTBOX || !listbox->input)
        return NULL;
    return listbox->input;
}

/* 内部：真正追加字符串，不做无效化 */
static LRESULT listbox_add_raw(HWND input, const char *text)
{
    WCHAR *wide = wtk__u8_to_u16(text ? text : "");
    LRESULT r;
    if (!wide)
        return LB_ERR;
    r = SendMessageW(input, LB_ADDSTRING, 0, (LPARAM)wide);
    free(wide);
    return r;
}

int wtk_listbox_add(WtkWidget *listbox, const char *text)
{
    HWND input = listbox_input(listbox);
    LRESULT result;

    if (!input)
        return -1;
    result = listbox_add_raw(input, text);
    /* 控件不可见时重绘没有意义（大量追加时能省掉成千上万次无效化） */
    if (IsWindowVisible(input))
        InvalidateRect(input, NULL, TRUE);
    return result == LB_ERR ? -1 : (int)result;
}

/* 批量追加：关掉重绘一次性塞完，最后只刷一次。
 * 逐条 wtk_listbox_add 在 1.8 万条规模下会卡死界面（每条一次 InvalidateRect）。 */
int wtk_listbox_add_many(WtkWidget *listbox, const char *const *texts, int count)
{
    HWND input = listbox_input(listbox);
    int i, added = 0;

    if (!input || !texts || count <= 0)
        return -1;
    wtk_widget_bulk_begin(listbox);
    for (i = 0; i < count; i++)
        if (listbox_add_raw(input, texts[i]) != LB_ERR)
            added++;
    wtk_widget_bulk_end(listbox);
    return added;
}

/* ---- 虚拟列表 ---- */

int wtk__native_virtual_height(WtkWidget *widget)
{
    int h;
    if (!widget)
        return 22;
    if (widget->item_height > 0)
        return widget->item_height;
    h = widget->font ? entry_line_height(widget) : 0;
    if (h <= 0)
        h = wtk__scale_widget(widget, 20);
    /* 行高 = 字高 + 上下呼吸，至少 22 逻辑像素 */
    h += wtk__scale_widget(widget, 6);
    if (h < wtk__scale_widget(widget, 22))
        h = wtk__scale_widget(widget, 22);
    return h;
}

LRESULT wtk__native_draw_virtual_item(WtkWidget *widget, DRAWITEMSTRUCT *dis)
{
    const WtkTheme *theme;
    RECT rc;
    int index = (int)dis->itemID;
    int selected = (dis->itemState & ODS_SELECTED) ? 1 : 0;
    COLORREF bg, fg;
    HBRUSH brush;
    HGDIOBJ old_font;

    if (!widget || !dis)
        return FALSE;
    theme = widget->app ? wtk_app_get_theme(widget->app) : NULL;
    rc = dis->rcItem;

    /* 注意 WtkColor 是 0xRRGGBB，COLORREF 是 0xBBGGRR —— 必须过 wtk__colorref */
    bg = wtk__colorref(selected ? (theme ? theme->accent : WTK_RGB(0x2D, 0x7F, 0xF6))
                                : (theme ? theme->surface : WTK_RGB(0xFF, 0xFF, 0xFF)));
    fg = wtk__colorref(selected ? (theme ? theme->on_accent : WTK_RGB(0xFF, 0xFF, 0xFF))
                                : (theme ? theme->text : WTK_RGB(0x1F, 0x23, 0x29)));

    brush = CreateSolidBrush(bg);
    if (brush) {
        FillRect(dis->hDC, &rc, brush);
        DeleteObject(brush);
    }
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);
    old_font = SelectObject(dis->hDC, widget->font);
    rc.left += wtk__scale_widget(widget, 6);
    rc.right -= wtk__scale_widget(widget, 4);

    if (index >= 0 && index < widget->virtual_count && widget->item_draw)
        widget->item_draw(widget, index, dis->hDC, &rc, widget->item_draw_data);

    if (old_font)
        SelectObject(dis->hDC, old_font);
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dis->hDC, &dis->rcItem);
    return TRUE;
}

int wtk_listbox_set_virtual(WtkWidget *list, int count, WtkListItemFn draw,
                            void *userdata)
{
    if (!list || list->type != WTK_TYPE_LISTBOX)
        return -1;
    list->item_draw = draw;
    list->item_draw_data = userdata;
    if (count <= 0) {
        /* 退回普通 listbox */
        if (list->virtual_count > 0) {
            list->virtual_count = 0;
            if (list->input) {
                DestroyWindow(list->input);
                list->input = NULL;
            }
            wtk__create_native_input(list);
        }
        return 0;
    }
    if (list->virtual_count > 0 && list->input) {
        list->virtual_count = count;
        SendMessageW(list->input, LB_SETCOUNT, (WPARAM)count, 0);
        wtk_listbox_virtual_refresh(list);
        return 0;
    }
    list->virtual_count = count;
    if (list->input) {
        DestroyWindow(list->input);
        list->input = NULL;
    }
    wtk__create_native_input(list);
    return list->input ? 0 : -1;
}

void wtk_listbox_virtual_refresh(WtkWidget *list)
{
    if (!list || !list->input)
        return;
    SendMessageW(list->input, LB_SETITEMHEIGHT, 0,
                 (LPARAM)wtk__native_virtual_height(list));
    if (IsWindowVisible(list->input)) {
        InvalidateRect(list->input, NULL, TRUE);
        UpdateWindow(list->input);
    }
}

int wtk_listbox_virtual_count(WtkWidget *list)
{
    if (!list || list->type != WTK_TYPE_LISTBOX)
        return 0;
    return list->virtual_count;
}

void wtk_listbox_clear(WtkWidget *listbox)
{
    HWND input = listbox_input(listbox);
    if (!input)
        return;
    SendMessageW(input, LB_RESETCONTENT, 0, 0);
    wtk__invalidate_text_cache(listbox);
    InvalidateRect(input, NULL, TRUE);
}

int wtk_listbox_count(WtkWidget *listbox)
{
    HWND input = listbox_input(listbox);
    if (!input)
        return 0;
    return (int)SendMessageW(input, LB_GETCOUNT, 0, 0);
}

int wtk_listbox_get_selection(WtkWidget *listbox)
{
    HWND input = listbox_input(listbox);
    LRESULT result;
    if (!input)
        return -1;
    result = SendMessageW(input, LB_GETCURSEL, 0, 0);
    return result == LB_ERR ? -1 : (int)result;
}

void wtk_listbox_select(WtkWidget *listbox, int index)
{
    HWND input = listbox_input(listbox);
    if (!input)
        return;
    if (index < -1)
        index = -1;
    SendMessageW(input, LB_SETCURSEL, (WPARAM)index, 0);
    if (index >= 0) {
        SendMessageW(input, LB_SETCARETINDEX, (WPARAM)index, TRUE);
        SendMessageW(input, LB_SETTOPINDEX, (WPARAM)index, 0);
    }
    InvalidateRect(input, NULL, TRUE);
}

void wtk_listbox_remove(WtkWidget *listbox, int index)
{
    HWND input = listbox_input(listbox);
    if (!input || index < 0)
        return;
    SendMessageW(input, LB_DELETESTRING, (WPARAM)index, 0);
    wtk__invalidate_text_cache(listbox);
    InvalidateRect(input, NULL, TRUE);
}

const char *wtk_listbox_get_item(WtkWidget *listbox, int index)
{
    HWND input = listbox_input(listbox);
    int length;
    WCHAR *wide;
    char *utf8;

    if (!input || index < 0)
        return "";
    length = (int)SendMessageW(input, LB_GETTEXTLEN, (WPARAM)index, 0);
    if (length == LB_ERR || length < 0)
        return "";
    wide = (WCHAR *)wtk__alloc_zero((size_t)(length + 1) * sizeof(WCHAR));
    if (SendMessageW(input, LB_GETTEXT, (WPARAM)index, (LPARAM)wide) ==
        LB_ERR) {
        free(wide);
        return "";
    }
    utf8 = wtk__u16_to_u8(wide);
    free(wide);

    free(listbox->text_cache);
    listbox->text_cache = utf8;
    listbox->text_cache_valid = 1;
    return listbox->text_cache;
}
