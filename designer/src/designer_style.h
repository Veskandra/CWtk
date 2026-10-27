#ifndef WINTK_DESIGNER_STYLE_H
#define WINTK_DESIGNER_STYLE_H

#include "wintk.h"

typedef enum DesignerStyle {
    DESIGNER_STYLE_NATIVE = 0,
    DESIGNER_STYLE_FUSION,
    DESIGNER_STYLE_WINTK
} DesignerStyle;

/* Registers the selected painter and the matching palette. WINTK uses the
 * core's original GDI drawing with no painter. */
void designer_style_apply(WtkApp *app, DesignerStyle style);

extern const WtkStylePainter designer_style_painter;

#endif
