/* Standalone style plugin: the original WinTk self-drawn appearance.
 *
 * It is intentionally separate from wintk_builtin_styles.dll so the classic
 * drawing code never becomes part of the core library or the built-in style
 * bundle. Build it as styles/wintk_classic_style.dll.
 */
#include "wintk.h"
#include "style_plugin.h"

extern const WtkStylePainter wintk_classic_painter;

static WtkTheme classic_theme;
static int classic_theme_ready;

static void classic_plugin_init(void)
{
    if (classic_theme_ready)
        return;
    wtk_theme_default(&classic_theme);
    classic_theme_ready = 1;
}

static const WtkStylePluginStyle classic_styles[] = {
    { "wintk", "WinTk 自绘", WTK_STYLE_WINTK, &classic_theme,
      &wintk_classic_painter }
};

static const WtkStylePlugin classic_plugin = {
    WINTK_STYLE_PLUGIN_ABI,
    "WinTk Classic Style",
    (int)(sizeof(classic_styles) / sizeof(classic_styles[0])),
    classic_styles
};

WINTK_STYLE_EXPORT const WtkStylePlugin *wintk_style_plugin_get(void)
{
    classic_plugin_init();
    return &classic_plugin;
}
