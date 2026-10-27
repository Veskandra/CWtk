/*
 * WinTk - a small, native, Tk-inspired GUI toolkit for C on Windows.
 *
 * Public API. All text passed to WinTk is UTF-8. Strings returned by the
 * library are owned by the widget and remain valid until that widget's text
 * changes or the widget is destroyed.
 *
 * Layout values and colors are logical (96 DPI) units. WinTk scales them for
 * the system DPI. Coordinates in mouse events and the dimensions passed to a
 * canvas draw callback are device pixels, so drawing code can use GDI directly.
 */
#ifndef WINTK_H
#define WINTK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINTK_VERSION_MAJOR 1
#define WINTK_VERSION_MINOR 0
#define WINTK_VERSION_PATCH 0
#define WINTK_VERSION_STRING "1.0.0"

typedef struct WtkApp WtkApp;
typedef struct WtkWidget WtkWidget;
typedef struct WtkMenu WtkMenu;

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#endif

/* ------------------------------------------------------------------------- */
/* Basic types                                                               */
/* ------------------------------------------------------------------------- */

/* 24-bit RGB, laid out as 0x00RRGGBB (not a Win32 COLORREF). */
typedef uint32_t WtkColor;
#define WTK_RGB(r, g, b) \
    ((WtkColor)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

#define WTK_BLACK  WTK_RGB(0, 0, 0)
#define WTK_WHITE  WTK_RGB(255, 255, 255)

typedef enum WtkFontStyle {
    WTK_FONT_NORMAL    = 0,
    WTK_FONT_BOLD      = 1 << 0,
    WTK_FONT_ITALIC    = 1 << 1,
    WTK_FONT_UNDERLINE = 1 << 2
} WtkFontStyle;

typedef enum WtkAlign {
    WTK_ALIGN_LEFT = 0,
    WTK_ALIGN_CENTER,
    WTK_ALIGN_RIGHT
} WtkAlign;

typedef enum WtkSide {
    WTK_SIDE_TOP = 0,
    WTK_SIDE_BOTTOM,
    WTK_SIDE_LEFT,
    WTK_SIDE_RIGHT
} WtkSide;

typedef enum WtkFill {
    WTK_FILL_NONE = 0,
    WTK_FILL_X    = 1,
    WTK_FILL_Y    = 2,
    WTK_FILL_BOTH = 3
} WtkFill;

typedef enum WtkSticky {
    WTK_STICKY_NONE       = 0,
    WTK_STICKY_LEFT       = 1 << 0,
    WTK_STICKY_TOP        = 1 << 1,
    WTK_STICKY_RIGHT      = 1 << 2,
    WTK_STICKY_BOTTOM     = 1 << 3,
    WTK_STICKY_WEST       = WTK_STICKY_LEFT,
    WTK_STICKY_NORTH      = WTK_STICKY_TOP,
    WTK_STICKY_EAST       = WTK_STICKY_RIGHT,
    WTK_STICKY_SOUTH      = WTK_STICKY_BOTTOM,
    WTK_STICKY_HORIZONTAL = WTK_STICKY_LEFT | WTK_STICKY_RIGHT,
    WTK_STICKY_VERTICAL   = WTK_STICKY_TOP | WTK_STICKY_BOTTOM,
    WTK_STICKY_ALL        = WTK_STICKY_HORIZONTAL | WTK_STICKY_VERTICAL
} WtkSticky;

typedef enum WtkButtonStyle {
    WTK_BUTTON_PRIMARY = 0,
    WTK_BUTTON_SECONDARY,
    WTK_BUTTON_GHOST,
    WTK_BUTTON_DANGER
} WtkButtonStyle;

typedef enum WtkCursor {
    WTK_CURSOR_ARROW = 0,
    WTK_CURSOR_HAND,
    WTK_CURSOR_IBEAM,
    WTK_CURSOR_CROSS,
    WTK_CURSOR_SIZE_H,
    WTK_CURSOR_SIZE_V,
    WTK_CURSOR_SIZE_ALL,
    WTK_CURSOR_SIZE_NWSE,
    WTK_CURSOR_SIZE_NESW,
    WTK_CURSOR_NO
} WtkCursor;

/* Visual style of the built-in widgets.
 *
 * AUTO (the default) resolves to FUSION, a neutral Qt-Fusion-like presentation.
 * NATIVE is the Windows UxTheme presentation and WINTK is the original custom
 * look. The environment variable WINTK_STYLE can be set to auto, fusion (or
 * qt), native or wintk before creating the app to choose a default. */
typedef enum WtkStyle {
    WTK_STYLE_AUTO = 0,
    WTK_STYLE_NATIVE,
    WTK_STYLE_WINTK,
    WTK_STYLE_FUSION
} WtkStyle;

typedef enum WtkIcon {
    WTK_ICON_NONE = 0,
    WTK_ICON_INFO,
    WTK_ICON_WARNING,
    WTK_ICON_ERROR,
    WTK_ICON_QUESTION
} WtkIcon;

typedef enum WtkMouseButton {
    WTK_MOUSE_LEFT = 1,
    WTK_MOUSE_RIGHT = 2,
    WTK_MOUSE_MIDDLE = 3
} WtkMouseButton;

#define WTK_MOD_SHIFT   (1u << 0)
#define WTK_MOD_CONTROL (1u << 1)
#define WTK_MOD_ALT     (1u << 2)

/* ------------------------------------------------------------------------- */
/* Events                                                                    */
/* ------------------------------------------------------------------------- */

typedef enum WtkEventType {
    WTK_EVENT_NONE = 0,
    WTK_EVENT_CLICK,
    WTK_EVENT_DOUBLE_CLICK,
    WTK_EVENT_CHANGE,
    WTK_EVENT_SUBMIT,
    WTK_EVENT_FOCUS_IN,
    WTK_EVENT_FOCUS_OUT,
    WTK_EVENT_MOUSE_DOWN,
    WTK_EVENT_MOUSE_UP,
    WTK_EVENT_MOUSE_MOVE,
    WTK_EVENT_MOUSE_ENTER,
    WTK_EVENT_MOUSE_LEAVE,
    WTK_EVENT_MOUSE_WHEEL,
    WTK_EVENT_KEY_DOWN,
    WTK_EVENT_KEY_UP,
    WTK_EVENT_RESIZE,
    WTK_EVENT_CLOSE
} WtkEventType;

typedef struct WtkEvent {
    WtkEventType type;

    /* Mouse position in device pixels, relative to the widget. */
    int x;
    int y;

    /* New size in device pixels for WTK_EVENT_RESIZE. */
    int width;
    int height;

    int button;           /* WtkMouseButton for mouse events. */
    int clicks;           /* Number of clicks, for double-click. */
    int key;              /* Win32 virtual-key code. */
    unsigned modifiers;   /* WTK_MOD_* bit mask. */
    int value;            /* Control value (slider, list selection, ...). */
    int delta;            /* Wheel delta for WTK_EVENT_MOUSE_WHEEL. */

    /* Only used by WTK_EVENT_CLOSE. Set to non-zero to veto the close. */
    int cancel;
} WtkEvent;

typedef void (*WtkEventFn)(WtkWidget *widget, WtkEvent *event, void *userdata);
typedef void (*WtkDrawFn)(WtkWidget *canvas, HDC dc, int width, int height,
                          void *userdata);
typedef void (*WtkTimerFn)(WtkApp *app, void *userdata);
typedef void (*WtkMenuFn)(WtkWidget *window, void *userdata);

/* ------------------------------------------------------------------------- */
/* Theme                                                                     */
/* ------------------------------------------------------------------------- */

typedef struct WtkTheme {
    WtkColor window_bg;
    WtkColor surface;
    WtkColor surface_alt;
    WtkColor text;
    WtkColor muted;
    WtkColor border;
    WtkColor accent;
    WtkColor accent_hover;
    WtkColor accent_pressed;
    WtkColor on_accent;
    WtkColor success;
    WtkColor warning;
    WtkColor danger;

    int corner_radius;    /* Logical pixels. */
    int control_height;   /* Logical pixels. */
} WtkTheme;

/* Fill a theme with WinTk's original light palette (the WinTk style). */
void wtk_theme_default(WtkTheme *theme);
/* Fill a theme with the neutral Qt-Fusion-like palette used by default. */
void wtk_theme_fusion(WtkTheme *theme);

/* ------------------------------------------------------------------------- */
/* Application                                                               */
/* ------------------------------------------------------------------------- */

WtkApp *wtk_app_new(void);
void wtk_app_free(WtkApp *app);

/* Shows all windows that have not been shown yet, then runs the event loop.
 * Returns the event loop's exit code. */
int wtk_run(WtkApp *app);
void wtk_quit(WtkApp *app);

int wtk_app_dpi(WtkApp *app);
int wtk_scale(WtkApp *app, int logical_pixels);

/* Requested and resolved visual style. AUTO resolves to NATIVE on Windows. */
void wtk_app_set_style(WtkApp *app, WtkStyle style);
WtkStyle wtk_app_get_style(WtkApp *app);
WtkStyle wtk_app_resolved_style(WtkApp *app);
const char *wtk_style_name(WtkStyle style);

/* Locale-aware default UI font family selected by WinTk (for example
 * "Microsoft YaHei UI" on Simplified Chinese Windows or "Segoe UI" when no
 * CJK UI font is needed). The returned UTF-8 string is owned by the app.
 * Passing NULL or an empty family to wtk_widget_set_font() uses this family. */
const char *wtk_app_ui_font_family(WtkApp *app);

void wtk_app_set_theme(WtkApp *app, const WtkTheme *theme);
const WtkTheme *wtk_app_get_theme(WtkApp *app);

/* One-shot timer on the UI thread. Returns an id, or 0 on failure. */
unsigned wtk_after(WtkApp *app, unsigned delay_ms, WtkTimerFn callback,
                   void *userdata);
void wtk_cancel_timer(WtkApp *app, unsigned timer_id);

/* Native modal dialogs. owner may be NULL. */
void wtk_message_box(WtkWidget *owner, const char *title, const char *message,
                     WtkIcon icon);
int wtk_ask_yes_no(WtkWidget *owner, const char *title, const char *message);

/* ------------------------------------------------------------------------- */
/* Windows                                                                   */
/* ------------------------------------------------------------------------- */

WtkWidget *wtk_window(WtkApp *app, const char *title, int width, int height);
void wtk_window_show(WtkWidget *window);
void wtk_window_center(WtkWidget *window);
void wtk_window_set_title(WtkWidget *window, const char *title);
void wtk_window_set_min_size(WtkWidget *window, int width, int height);
void wtk_window_set_resizable(WtkWidget *window, int resizable);
void wtk_window_close(WtkWidget *window);

/* Create and attach a menu bar. Menu text may contain '&' mnemonics and a
 * tab-separated accelerator hint such as "&Save\tCtrl+S" (the hint is drawn,
 * but WinTk does not install the keyboard accelerator automatically). */
WtkMenu *wtk_menubar(WtkWidget *window);
WtkMenu *wtk_menu_submenu(WtkMenu *parent, const char *label);
void wtk_menu_add_command(WtkMenu *menu, const char *label, WtkMenuFn callback,
                          void *userdata);
void wtk_menu_add_separator(WtkMenu *menu);

/* ------------------------------------------------------------------------- */
/* Widgets                                                                   */
/* ------------------------------------------------------------------------- */

WtkWidget *wtk_frame(WtkWidget *parent);
WtkWidget *wtk_label(WtkWidget *parent, const char *text);
WtkWidget *wtk_button(WtkWidget *parent, const char *text);
WtkWidget *wtk_entry(WtkWidget *parent);
WtkWidget *wtk_text(WtkWidget *parent);
WtkWidget *wtk_listbox(WtkWidget *parent);
WtkWidget *wtk_checkbox(WtkWidget *parent, const char *text, int checked);
WtkWidget *wtk_radiogroup(WtkWidget *parent);
WtkWidget *wtk_radio(WtkWidget *group, const char *text);
WtkWidget *wtk_slider(WtkWidget *parent, int minimum, int maximum, int value);
WtkWidget *wtk_progress(WtkWidget *parent);
WtkWidget *wtk_separator(WtkWidget *parent);
WtkWidget *wtk_canvas(WtkWidget *parent, int width, int height);

/* Generic configuration. */
WtkApp *wtk_widget_app(WtkWidget *widget);
WtkWidget *wtk_widget_parent(WtkWidget *widget);
/* Escape hatch for native Win32 interop (owner-drawn extensions, timers...).
 * For entries, text areas and listboxes this is the host window; use the
 * native child returned by GetWindow(..., GW_CHILD) if necessary. */
HWND wtk_widget_hwnd(WtkWidget *widget);
void wtk_widget_set_background(WtkWidget *widget, WtkColor color);
void wtk_widget_set_foreground(WtkWidget *widget, WtkColor color);
/* family may be NULL/empty to use wtk_app_ui_font_family(). */
void wtk_widget_set_font(WtkWidget *widget, const char *family, int point_size,
                         unsigned style);
void wtk_widget_set_padding(WtkWidget *widget, int left, int top, int right,
                            int bottom);
/* Requested logical size. Pass 0 for automatic sizing in that axis. */
void wtk_widget_set_size(WtkWidget *widget, int width, int height);
void wtk_widget_set_border(WtkWidget *widget, WtkColor color, int width,
                           int radius);
void wtk_widget_set_visible(WtkWidget *widget, int visible);
void wtk_widget_set_enabled(WtkWidget *widget, int enabled);
void wtk_widget_set_cursor(WtkWidget *widget, WtkCursor cursor);
/* Hide the accent focus outline for custom widgets such as splitters. */
void wtk_widget_set_focus_ring(WtkWidget *widget, int enabled);
void wtk_widget_focus(WtkWidget *widget);
void wtk_widget_redraw(WtkWidget *widget);
void wtk_widget_destroy(WtkWidget *widget);

/* Binds a callback to an event. Multiple callbacks may be bound to the same
 * event; they are invoked in binding order. */
void wtk_bind(WtkWidget *widget, WtkEventType type, WtkEventFn callback,
              void *userdata);
void wtk_unbind(WtkWidget *widget, WtkEventType type, WtkEventFn callback);

/* Label. */
void wtk_label_set_text(WtkWidget *label, const char *text);
const char *wtk_label_get_text(WtkWidget *label);
void wtk_label_set_align(WtkWidget *label, WtkAlign align);
/* Wrap at the given logical width. 0 disables wrapping. */
void wtk_label_set_wrap_width(WtkWidget *label, int logical_width);

/* Button. Bind WTK_EVENT_CLICK to run a command. */
void wtk_button_set_text(WtkWidget *button, const char *text);
const char *wtk_button_get_text(WtkWidget *button);
void wtk_button_set_style(WtkWidget *button, WtkButtonStyle style);
void wtk_button_set_align(WtkWidget *button, WtkAlign align);

/* Single-line entry. Bind WTK_EVENT_CHANGE and WTK_EVENT_SUBMIT. */
void wtk_entry_set_text(WtkWidget *entry, const char *text);
const char *wtk_entry_get_text(WtkWidget *entry);
void wtk_entry_set_placeholder(WtkWidget *entry, const char *text);
void wtk_entry_set_password(WtkWidget *entry, int password);
void wtk_entry_set_readonly(WtkWidget *entry, int readonly);
void wtk_entry_select_all(WtkWidget *entry);

/* Multi-line text editor. Bind WTK_EVENT_CHANGE and WTK_EVENT_SUBMIT
 * (Ctrl+Enter submits). */
void wtk_text_set_text(WtkWidget *text, const char *value);
const char *wtk_text_get_text(WtkWidget *text);
void wtk_text_set_placeholder(WtkWidget *widget, const char *placeholder);
void wtk_text_set_readonly(WtkWidget *text, int readonly);

/* Listbox. Bind WTK_EVENT_CHANGE and WTK_EVENT_DOUBLE_CLICK. */
int wtk_listbox_add(WtkWidget *listbox, const char *text);
void wtk_listbox_clear(WtkWidget *listbox);
int wtk_listbox_count(WtkWidget *listbox);
int wtk_listbox_get_selection(WtkWidget *listbox);
void wtk_listbox_select(WtkWidget *listbox, int index);
void wtk_listbox_remove(WtkWidget *listbox, int index);
const char *wtk_listbox_get_item(WtkWidget *listbox, int index);

/* Checkbox. Bind WTK_EVENT_CHANGE. */
int wtk_checkbox_get(WtkWidget *checkbox);
void wtk_checkbox_set(WtkWidget *checkbox, int checked);

/* Radio button group. Children are added to the group, not to an outer
 * frame. Bind WTK_EVENT_CHANGE on the group widget. */
int wtk_radiogroup_get(WtkWidget *group);
void wtk_radiogroup_set(WtkWidget *group, int index);

/* Slider. Bind WTK_EVENT_CHANGE; event->value contains the new value. */
int wtk_slider_get(WtkWidget *slider);
void wtk_slider_set(WtkWidget *slider, int value);
void wtk_slider_set_step(WtkWidget *slider, int step);

/* Determinate progress bar, fraction in [0.0, 1.0]. */
void wtk_progress_set(WtkWidget *progress, double fraction);
double wtk_progress_get(WtkWidget *progress);
void wtk_progress_set_show_percent(WtkWidget *progress, int show);

/* Canvas. The draw callback receives a double-buffered HDC in device pixels. */
void wtk_canvas_set_draw(WtkWidget *canvas, WtkDrawFn callback, void *userdata);
void wtk_canvas_redraw(WtkWidget *canvas);

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

/* Tk-style pack geometry manager.
 *   side:   edge of the parent's cavity to pack against
 *   fill:   axes on which the widget fills its parcel
 *   expand: distribute leftover space along the packing axis
 *   padx/pady: external padding in logical pixels
 * A parent must consistently use pack or grid. */
void wtk_pack(WtkWidget *widget, WtkSide side, WtkFill fill, int expand,
              int padx, int pady);

/* Grid geometry manager. Rows and columns are zero-based. */
void wtk_grid(WtkWidget *widget, int row, int column, int row_span,
              int column_span, WtkSticky sticky, int padx, int pady);

/* Give grid columns/rows a growth weight and/or a minimum logical size.
 * Columns containing a child with WTK_STICKY_HORIZONTAL automatically get a
 * weight of 1 if no explicit column weights have been configured. Rows use
 * WTK_STICKY_VERTICAL for the same fallback. */
void wtk_grid_column_configure(WtkWidget *master, int column, int weight,
                               int min_width);
void wtk_grid_row_configure(WtkWidget *master, int row, int weight,
                            int min_height);

/* Absolute placement. Coordinates and size are logical pixels, relative to
 * the parent's content area. Intended for visual designers and pixel-perfect
 * forms; a parent should not mix place with pack/grid. */
void wtk_place(WtkWidget *widget, int x, int y, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* WINTK_H */
