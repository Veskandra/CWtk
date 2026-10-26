#ifndef WINTK_INTERNAL_H
#define WINTK_INTERNAL_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "wintk.h"

#include <commctrl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <wchar.h>

#define WTK_WM_SWEEP      (WM_APP + 0x51)
#define WTK_INPUT_ID      1
#define WTK_INPUT_SUBCLASS 0x57544B

typedef enum WtkWidgetType {
    WTK_TYPE_WINDOW = 0,
    WTK_TYPE_FRAME,
    WTK_TYPE_LABEL,
    WTK_TYPE_BUTTON,
    WTK_TYPE_ENTRY,
    WTK_TYPE_TEXT,
    WTK_TYPE_LISTBOX,
    WTK_TYPE_CHECKBOX,
    WTK_TYPE_RADIOGROUP,
    WTK_TYPE_RADIO,
    WTK_TYPE_SLIDER,
    WTK_TYPE_PROGRESS,
    WTK_TYPE_SEPARATOR,
    WTK_TYPE_CANVAS
} WtkWidgetType;

typedef enum WtkLayoutMode {
    WTK_LAYOUT_NONE = 0,
    WTK_LAYOUT_PACK,
    WTK_LAYOUT_GRID,
    WTK_LAYOUT_PLACE
} WtkLayoutMode;

typedef struct WtkBinding {
    WtkEventType type;
    WtkEventFn callback;
    void *userdata;
} WtkBinding;

typedef struct WtkTimer {
    unsigned id;
    WtkTimerFn callback;
    void *userdata;
} WtkTimer;

typedef struct WtkMenuCommand {
    UINT id;
    WtkMenuFn callback;
    void *userdata;
} WtkMenuCommand;

struct WtkMenu {
    WtkApp *app;
    HWND window;
    HMENU handle;
    WtkMenu *parent;
    WtkMenu *first_child;
    WtkMenu *next_sibling;
    WtkMenuCommand *commands;
    int command_count;
    int command_cap;
    int is_bar;
};

struct WtkApp {
    HINSTANCE instance;
    WtkTheme theme;
    int dpi;
    HWND message_hwnd;
    WCHAR ui_font_family[LF_FACESIZE];
    char *ui_font_family_utf8;

    WtkWidget **windows;
    int window_count;
    int window_cap;
    int open_windows;
    int running;
    int shutting_down;
    int in_layout;
    int layout_pending;

    WtkWidget **graveyard;
    int grave_count;
    int grave_cap;
    int sweep_posted;
    int dispatch_depth;

    WtkTimer *timers;
    int timer_count;
    int timer_cap;
    unsigned next_timer_id;

    UINT next_menu_id;
};

struct WtkWidget {
    WtkWidgetType type;
    WtkApp *app;
    WtkWidget *parent;
    HWND hwnd;                 /* The widget's own window (a host for fields). */
    HWND input;                /* Native EDIT/LISTBOX child, if any. */

    WtkWidget **children;
    int child_count;
    int child_cap;

    WtkBinding *bindings;
    int bind_count;
    int bind_cap;

    WCHAR *text;
    WCHAR *placeholder;
    char *text_cache;
    int text_cache_valid;

    HFONT font;
    WCHAR font_family[LF_FACESIZE];
    int font_pt;
    unsigned font_style;

    WtkColor bg;
    WtkColor fg;
    WtkColor border;
    int bg_set;
    int fg_set;
    int border_set;
    int border_width;          /* Device pixels. */
    int border_radius;         /* Device pixels. */

    int pad_l;
    int pad_t;
    int pad_r;
    int pad_b;

    int req_w;
    int req_h;
    int pref_w;
    int pref_h;
    int x;
    int y;
    int w;
    int h;

    WtkLayoutMode layout_mode;
    int layout_managed;

    WtkSide pack_side;
    WtkFill pack_fill;
    int pack_expand;
    int pack_padx;
    int pack_pady;

    int grid_row;
    int grid_col;
    int grid_rowspan;
    int grid_colspan;
    WtkSticky grid_sticky;
    int grid_padx;
    int grid_pady;

    int place_x;
    int place_y;
    int place_w;
    int place_h;

    int *grid_col_weights;
    int *grid_col_mins;
    int grid_col_cap;
    int grid_col_configured;
    int *grid_row_weights;
    int *grid_row_mins;
    int grid_row_cap;
    int grid_row_configured;

    int visible;
    int enabled;
    int hover;
    int pressed;
    int focused;
    int focusable;
    int tracking_mouse;
    WtkCursor cursor;

    WtkButtonStyle button_style;
    WtkAlign align;
    int wrap_width;            /* Device pixels, 0 = no wrap. */

    int checked;
    int value_min;
    int value_max;
    int value;
    int value_step;

    double progress;
    int show_percent;

    int readonly;
    int password;
    int suppress_change;
    HBRUSH input_brush;

    WtkDrawFn draw_callback;
    void *draw_data;

    int min_w;                 /* Device pixels, only for top-level windows. */
    int min_h;
    int resizable;
    int shown;
    int open_counted;
    int pending_free;
    WtkMenu *menu;
};

/* ------------------------------------------------------------------------- */
/* Cross-file helpers                                                        */
/* ------------------------------------------------------------------------- */

void *wtk__alloc_zero(size_t size);
void *wtk__realloc_array(void *ptr, size_t size);
WtkWidget *wtk__alloc_widget(WtkApp *app, WtkWidget *parent,
                             WtkWidgetType type);
WCHAR *wtk__u8_to_u16(const char *text);
char *wtk__u16_to_u8(const WCHAR *text);
void wtk__set_widget_text(WtkWidget *widget, const char *text);
void wtk__invalidate_text_cache(WtkWidget *widget);
char *wtk__get_cached_text(WtkWidget *widget);
COLORREF wtk__colorref(WtkColor color);
WtkColor wtk__shade(WtkColor color, int percent);
int wtk__logical(WtkApp *app, int pixels);
int wtk__scale_widget(WtkWidget *widget, int pixels);

WtkColor wtk__container_bg(WtkWidget *container);
WtkColor wtk__widget_bg(WtkWidget *widget);
WtkColor wtk__field_bg(WtkWidget *widget);
WtkColor wtk__field_fg(WtkWidget *widget);
int wtk__is_field(WtkWidget *widget);
int wtk__is_container(WtkWidget *widget);
WtkWidget *wtk__toplevel(WtkWidget *widget);
HWND wtk__focus_target(WtkWidget *widget);
int wtk__radio_index(WtkWidget *widget);
int wtk__radio_selected(WtkWidget *widget);

void wtk__invalidate(WtkWidget *widget);
void wtk__invalidate_native(WtkWidget *widget);
void wtk__dispatch(WtkWidget *widget, WtkEvent *event);
void wtk__emit(WtkWidget *widget, WtkEventType type);
void wtk__emit_change(WtkWidget *widget, int value);

/* Core / window management. */
void wtk__register_window_class(HINSTANCE instance);
void wtk__register_widget_class(HINSTANCE instance);
void wtk__add_window(WtkApp *app, WtkWidget *window);
WtkWidget *wtk__window_from_hwnd(WtkApp *app, HWND hwnd);
void wtk__focus_next(WtkWidget *window, int reverse);
void wtk__free_menu_tree(WtkMenu *menu);
void wtk__handle_menu_command(WtkWidget *window, UINT id);
void wtk__sweep_graveyard(WtkApp *app);
void wtk__queue_graveyard(WtkWidget *widget);
void wtk__free_widget_tree(WtkWidget *widget);
void wtk__begin_dispatch(WtkApp *app);
void wtk__end_dispatch(WtkApp *app);

/* Widgets. */
WtkWidget *wtk__create_widget(WtkWidget *parent, WtkWidgetType type,
                              const char *text);
void wtk__add_child(WtkWidget *parent, WtkWidget *child);
void wtk__remove_child(WtkWidget *parent, WtkWidget *child);
void wtk__refresh_widget_font(WtkWidget *widget);

/* Native edit/listbox helpers. */
void wtk__create_native_input(WtkWidget *widget);
void wtk__native_position(WtkWidget *widget);
void wtk__native_command(WtkWidget *widget, WPARAM wparam, LPARAM lparam);
HBRUSH wtk__native_ctlcolor(WtkWidget *widget, HDC dc, UINT message);
void wtk__native_theme_changed(WtkWidget *widget);
void wtk__native_font_changed(WtkWidget *widget);
void wtk__native_enabled_changed(WtkWidget *widget);
void wtk__native_set_text(WtkWidget *widget, const char *text);
const char *wtk__native_get_text(WtkWidget *widget);
void wtk__native_refresh_brush(WtkWidget *widget);

/* Painting. */
void wtk__paint_widget(WtkWidget *widget, HDC dc, const RECT *bounds);
void wtk__paint_window_background(WtkWidget *window, HDC dc,
                                  const RECT *bounds);
void wtk__fill_rect_color(HDC dc, const RECT *rect, WtkColor color);

/* Layout. */
void wtk__measure_widget(WtkWidget *widget, int *width, int *height);
void wtk__layout_window(WtkWidget *window);
void wtk__layout_children(WtkWidget *parent);
void wtk__request_relayout(WtkWidget *widget);

/* Radio group internal selection. */
void wtk__radio_select(WtkWidget *group, int index, int focus, int notify);
void wtk__radio_step(WtkWidget *radio, int direction);

#endif /* WINTK_INTERNAL_H */
