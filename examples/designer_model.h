/*
 * WinTk Designer - document model and C code generation.
 * This file is UI-independent so the generator can be tested without a GUI.
 */
#ifndef WINTK_DESIGNER_MODEL_H
#define WINTK_DESIGNER_MODEL_H

#include <stddef.h>

#define DESIGN_MAX_ITEMS 64
#define DESIGN_NAME_CAP 48
#define DESIGN_TEXT_CAP 256

typedef enum DesignKind {
    DK_LABEL = 0,
    DK_BUTTON,
    DK_ENTRY,
    DK_TEXT,
    DK_CHECKBOX,
    DK_LISTBOX,
    DK_SLIDER,
    DK_PROGRESS,
    DK_SEPARATOR,
    DK_FRAME,
    DK_CANVAS,
    DK_COUNT
} DesignKind;

typedef struct DesignItem {
    DesignKind kind;
    int x;
    int y;
    int w;
    int h;
    int font_size;
    int bold;
    int checked;
    int primary;
    char name[DESIGN_NAME_CAP];
    char text[DESIGN_TEXT_CAP];
} DesignItem;

typedef struct DesignDoc {
    char title[128];
    int page_w;
    int page_h;
    int count;
    DesignItem items[DESIGN_MAX_ITEMS];
} DesignDoc;

typedef struct DesignPaletteEntry {
    DesignKind kind;
    const char *label;
    const char *prefix;
    int width;
    int height;
    const char *default_text;
} DesignPaletteEntry;

extern const DesignPaletteEntry designer_palette[DK_COUNT];

void designer_document_default(DesignDoc *doc);
void designer_document_clear(DesignDoc *doc);

const char *designer_kind_name(DesignKind kind);
const char *designer_kind_prefix(DesignKind kind);
const char *designer_kind_placeholder(DesignKind kind);
int designer_kind_has_text(DesignKind kind);
int designer_kind_has_font(DesignKind kind);

int designer_add_item(DesignDoc *doc, DesignKind kind, int x, int y);
void designer_remove_item(DesignDoc *doc, int index);

/* Produce a valid, unique C identifier for doc->items[index]. name may be
 * NULL/empty to request an automatic name from the kind's prefix. */
void designer_normalize_name(const DesignDoc *doc, int index,
                             const char *name, char *out, size_t out_size);

/* Generate a complete, standalone WinTk C source file. Returns 1 on success,
 * 0 if the output buffer was too small. */
int designer_generate_c(const DesignDoc *doc, char *buffer, size_t capacity);

#endif /* WINTK_DESIGNER_MODEL_H */
