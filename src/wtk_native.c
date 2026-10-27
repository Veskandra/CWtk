#include "wtk_internal.h"

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
    if (!widget || !wtk__is_field(widget))
        return;
    if (widget->input_brush) {
        DeleteObject(widget->input_brush);
        widget->input_brush = NULL;
    }
    widget->input_brush =
        CreateSolidBrush(wtk__colorref(wtk__field_bg(widget)));
}

HBRUSH wtk__native_ctlcolor(WtkWidget *widget, HDC dc, UINT message)
{
    WtkColor bg;
    WtkColor fg;
    if (!widget || !wtk__is_field(widget))
        return (HBRUSH)GetStockObject(WHITE_BRUSH);
    (void)message;

    bg = wtk__field_bg(widget);
    fg = wtk__field_fg(widget);
    if (!widget->input_brush)
        wtk__native_refresh_brush(widget);

    SetBkColor(dc, wtk__colorref(bg));
    SetTextColor(dc, wtk__colorref(fg));
    SetBkMode(dc, OPAQUE);
    return widget->input_brush;
}

void wtk__native_theme_changed(WtkWidget *widget)
{
    if (!widget || !wtk__is_field(widget))
        return;
    wtk__native_refresh_brush(widget);
    wtk__native_position(widget);
    if (widget->input)
        InvalidateRect(widget->input, NULL, TRUE);
    wtk__invalidate(widget);
}

void wtk__native_font_changed(WtkWidget *widget)
{
    if (!widget || !wtk__is_field(widget) || !widget->input)
        return;
    SendMessageW(widget->input, WM_SETFONT, (WPARAM)widget->font, TRUE);
    wtk__native_position(widget);
    InvalidateRect(widget->input, NULL, TRUE);
}

void wtk__native_enabled_changed(WtkWidget *widget)
{
    if (!widget || !wtk__is_field(widget))
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

    if (!widget || !wtk__is_field(widget) || !widget->hwnd)
        return;
    app = widget->app;

    if (widget->type == WTK_TYPE_ENTRY) {
        class_name = L"EDIT";
        style |= ES_LEFT | ES_AUTOHSCROLL | ES_NOHIDESEL;
    } else if (widget->type == WTK_TYPE_TEXT) {
        class_name = L"EDIT";
        style |= ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                 ES_NOHIDESEL | WS_VSCROLL;
    } else {
        class_name = L"LISTBOX";
        style |= LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS |
                 WS_VSCROLL;
    }

    input = CreateWindowExW(
        0, class_name, L"", style, 0, 0, 0, 0, widget->hwnd,
        (HMENU)(UINT_PTR)WTK_INPUT_ID, app->instance, NULL);
    if (!input)
        return;

    widget->input = input;
    SendMessageW(input, WM_SETFONT, (WPARAM)widget->font, TRUE);

    if (widget->type == WTK_TYPE_ENTRY ||
        widget->type == WTK_TYPE_TEXT) {
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

void wtk__native_position(WtkWidget *widget)
{
    RECT rc;
    int width;
    int height;
    int inset_x;
    int inset_y;

    if (!widget || !widget->input || !widget->hwnd)
        return;
    GetClientRect(widget->hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;

    if (wtk__style_is_native(widget)) {
        inset_x = wtk__scale_widget(widget, 2);
        inset_y = wtk__scale_widget(widget, 2);
        if (widget->type == WTK_TYPE_ENTRY)
            inset_y = wtk__scale_widget(widget, 2);
    } else if (wtk__style_is_fusion(widget)) {
        inset_x = wtk__scale_widget(widget, 4);
        inset_y = wtk__scale_widget(widget,
                                    widget->type == WTK_TYPE_ENTRY ? 2 : 4);
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
    SetWindowPos(widget->input, NULL, inset_x, inset_y,
                 width - 2 * inset_x, height - 2 * inset_y,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (widget->type == WTK_TYPE_ENTRY ||
        widget->type == WTK_TYPE_TEXT) {
        int margin = wtk__style_is_native(widget)
                         ? wtk__scale_widget(widget, 3)
                         : (wtk__style_is_fusion(widget)
                                ? wtk__scale_widget(widget, 4)
                                : wtk__scale_widget(widget, 5));
        SendMessageW(widget->input, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(margin, margin));
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
    if (text && text->type == WTK_TYPE_TEXT)
        wtk__native_set_text(text, value);
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

int wtk_listbox_add(WtkWidget *listbox, const char *text)
{
    HWND input = listbox_input(listbox);
    WCHAR *wide;
    LRESULT result;
    if (!input)
        return -1;
    wide = wtk__u8_to_u16(text ? text : "");
    result = SendMessageW(input, LB_ADDSTRING, 0, (LPARAM)wide);
    free(wide);
    InvalidateRect(input, NULL, TRUE);
    return result == LB_ERR ? -1 : (int)result;
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
