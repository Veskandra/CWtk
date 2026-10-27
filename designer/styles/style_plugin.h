#ifndef WINTK_STYLE_PLUGIN_H
#define WINTK_STYLE_PLUGIN_H

#include "wintk.h"

#define WINTK_STYLE_PLUGIN_ABI 1

typedef struct WtkStylePluginStyle {
    const char *id;
    const char *name;
    WtkStyle hint;             /* compatibility hint for the painter */
    const WtkTheme *theme;     /* palette applied by Designer */
    const WtkStylePainter *painter; /* NULL for core WinTk GDI */
} WtkStylePluginStyle;

typedef struct WtkStylePlugin {
    unsigned abi_version;
    const char *name;
    int style_count;
    const WtkStylePluginStyle *styles;
} WtkStylePlugin;

typedef const WtkStylePlugin *(*WtkStylePluginGetFn)(void);

#ifdef _WIN32
#define WINTK_STYLE_EXPORT __declspec(dllexport)
#else
#define WINTK_STYLE_EXPORT
#endif

#endif
