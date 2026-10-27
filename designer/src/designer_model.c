#include "designer_model.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const DesignPaletteEntry designer_palette[DK_COUNT] = {
    { DK_LABEL,     "标签",     "label",     320,  28, "标签" },
    { DK_BUTTON,    "按钮",     "button",    110,  40, "按钮" },
    { DK_ENTRY,     "输入框",   "entry",     240,  38, "" },
    { DK_TEXT,      "多行文本", "text",      280, 140, "" },
    { DK_CHECKBOX,  "复选框",   "checkbox",  180,  28, "复选框" },
    { DK_LISTBOX,   "列表框",   "listbox",   240, 150, "" },
    { DK_SLIDER,    "滑块",     "slider",    220,  32, "" },
    { DK_PROGRESS,  "进度条",   "progress",  240,  16, "" },
    { DK_SEPARATOR, "分隔线",   "separator", 260,   2, "" },
    { DK_FRAME,     "面板",     "panel",     300, 180, "" },
    { DK_CANVAS,    "画布",     "canvas",    280, 180, "" }
};

static const char *const kind_names[DK_COUNT] = {
    "标签", "按钮", "输入框", "多行文本", "复选框", "列表框",
    "滑块", "进度条", "分隔线", "面板", "画布"
};

static const char *const kind_prefixes[DK_COUNT] = {
    "label", "button", "entry", "text", "checkbox", "listbox",
    "slider", "progress", "separator", "panel", "canvas"
};

static const char *const kind_placeholders[DK_COUNT] = {
    "", "", "请输入…", "写下你的内容…", "", "",
    "", "", "", "", ""
};

static int kind_default_font(DesignKind kind)
{
    switch (kind) {
    case DK_LABEL:
        return 12;
    case DK_BUTTON:
    case DK_ENTRY:
    case DK_TEXT:
    case DK_CHECKBOX:
    case DK_LISTBOX:
        return 10;
    default:
        return 10;
    }
}

const char *designer_kind_name(DesignKind kind)
{
    if (kind < 0 || kind >= DK_COUNT)
        return "";
    return kind_names[kind];
}

const char *designer_kind_prefix(DesignKind kind)
{
    if (kind < 0 || kind >= DK_COUNT)
        return "widget";
    return kind_prefixes[kind];
}

const char *designer_kind_placeholder(DesignKind kind)
{
    if (kind < 0 || kind >= DK_COUNT)
        return "";
    return kind_placeholders[kind];
}

int designer_kind_has_text(DesignKind kind)
{
    return kind == DK_LABEL || kind == DK_BUTTON || kind == DK_ENTRY ||
           kind == DK_TEXT || kind == DK_CHECKBOX;
}

int designer_kind_has_font(DesignKind kind)
{
    return kind == DK_LABEL || kind == DK_BUTTON || kind == DK_ENTRY ||
           kind == DK_TEXT || kind == DK_CHECKBOX || kind == DK_LISTBOX;
}

static int is_c_keyword(const char *name)
{
    static const char *const keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
        "_Bool", "_Complex", "_Imaginary", "parent", "main"
    };
    size_t i;
    for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (strcmp(name, keywords[i]) == 0)
            return 1;
    }
    return 0;
}

static int name_used(const DesignDoc *doc, int except_index, const char *name)
{
    int i;
    for (i = 0; i < doc->count; ++i) {
        if (i == except_index)
            continue;
        if (strcmp(doc->items[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static void sanitize_identifier(const char *input, char *out, size_t out_size)
{
    size_t j = 0;
    size_t i;
    int previous_underscore = 0;

    if (out_size == 0)
        return;
    out[0] = '\0';
    if (!input)
        return;

    for (i = 0; input[i] && j + 1 < out_size; ++i) {
        unsigned char c = (unsigned char)input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            if (j == 0 && c >= '0' && c <= '9') {
                if (j + 2 >= out_size)
                    break;
                out[j++] = '_';
            }
            out[j++] = (char)c;
            previous_underscore = 0;
        } else if (c == '_' || c >= 0x80 || c == ' ' || c == '-' || c == '.') {
            if (j > 0 && !previous_underscore && j + 1 < out_size) {
                out[j++] = '_';
                previous_underscore = 1;
            }
        }
    }
    if (j > 0 && out[j - 1] == '_' && j > 1)
        j--;
    out[j] = '\0';

    if (out[0] == '\0' || strcmp(out, "_") == 0) {
        out[0] = '\0';
        return;
    }
    if (is_c_keyword(out)) {
        char prefixed[DESIGN_NAME_CAP];
        snprintf(prefixed, sizeof(prefixed), "w_%s", out);
        snprintf(out, out_size, "%s", prefixed);
    }
}

void designer_normalize_name(const DesignDoc *doc, int index,
                             const char *name, char *out, size_t out_size)
{
    char base[DESIGN_NAME_CAP];
    char candidate[DESIGN_NAME_CAP];
    const char *prefix;
    int attempt;

    if (!out || out_size == 0)
        return;
    if (!doc || index < 0 || index >= doc->count) {
        out[0] = '\0';
        return;
    }

    prefix = designer_kind_prefix(doc->items[index].kind);
    sanitize_identifier(name, base, sizeof(base));
    if (base[0] == '\0') {
        for (attempt = 1; attempt < 10000; ++attempt) {
            snprintf(candidate, sizeof(candidate), "%s%d", prefix, attempt);
            if (!name_used(doc, index, candidate)) {
                snprintf(out, out_size, "%s", candidate);
                return;
            }
        }
        snprintf(out, out_size, "%s", prefix);
        return;
    }

    snprintf(candidate, sizeof(candidate), "%s", base);
    if (!name_used(doc, index, candidate)) {
        snprintf(out, out_size, "%s", candidate);
        return;
    }

    for (attempt = 2; attempt < 10000; ++attempt) {
        snprintf(candidate, sizeof(candidate), "%s_%d", base, attempt);
        if (!name_used(doc, index, candidate)) {
            snprintf(out, out_size, "%s", candidate);
            return;
        }
    }
    snprintf(out, out_size, "%s_1", base);
}

int designer_add_item(DesignDoc *doc, DesignKind kind, int x, int y)
{
    DesignItem *item;
    int index;

    if (!doc || kind < 0 || kind >= DK_COUNT ||
        doc->count >= DESIGN_MAX_ITEMS)
        return -1;

    index = doc->count++;
    item = &doc->items[index];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->x = x;
    item->y = y;
    item->w = designer_palette[kind].width;
    item->h = designer_palette[kind].height;
    item->font_size = kind_default_font(kind);
    item->bold = 0;
    item->checked = 0;
    item->primary = 0;
    item->text[0] = '\0';
    if (designer_palette[kind].default_text) {
        snprintf(item->text, sizeof(item->text), "%s",
                 designer_palette[kind].default_text);
    }
    item->name[0] = '\0';
    {
        char unique[DESIGN_NAME_CAP];
        designer_normalize_name(doc, index, "", unique, sizeof(unique));
        snprintf(item->name, sizeof(item->name), "%s", unique);
    }
    return index;
}

void designer_remove_item(DesignDoc *doc, int index)
{
    if (!doc || index < 0 || index >= doc->count)
        return;
    if (index + 1 < doc->count) {
        memmove(&doc->items[index], &doc->items[index + 1],
                (size_t)(doc->count - index - 1) * sizeof(DesignItem));
    }
    doc->count--;
}

void designer_document_clear(DesignDoc *doc)
{
    if (!doc)
        return;
    memset(doc, 0, sizeof(*doc));
    snprintf(doc->title, sizeof(doc->title), "%s", "未命名页面");
    doc->page_w = 760;
    doc->page_h = 520;
    doc->style = 0;
}

void designer_document_default(DesignDoc *doc)
{
    int i;

    if (!doc)
        return;
    designer_document_clear(doc);
    snprintf(doc->title, sizeof(doc->title), "%s", "我的页面");

    i = designer_add_item(doc, DK_LABEL, 40, 36);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 540;
        item->h = 44;
        item->font_size = 24;
        item->bold = 1;
        snprintf(item->text, sizeof(item->text), "%s",
                 "欢迎使用 WinTk Designer");
    }

    i = designer_add_item(doc, DK_LABEL, 40, 90);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 640;
        item->h = 28;
        item->font_size = 11;
        snprintf(item->text, sizeof(item->text), "%s",
                 "拖拽左侧控件到画布，右侧编辑属性，底部实时生成 C 代码。");
    }

    i = designer_add_item(doc, DK_ENTRY, 40, 146);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 300;
        item->h = 38;
        item->font_size = 10;
    }

    i = designer_add_item(doc, DK_BUTTON, 356, 146);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 120;
        item->h = 38;
        item->font_size = 10;
        item->bold = 1;
        item->primary = 1;
        snprintf(item->text, sizeof(item->text), "%s", "开始设计");
    }

    i = designer_add_item(doc, DK_CHECKBOX, 40, 204);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 220;
        item->h = 28;
        item->font_size = 10;
        item->checked = 1;
        snprintf(item->text, sizeof(item->text), "%s", "记住这个页面");
    }

    i = designer_add_item(doc, DK_SEPARATOR, 40, 254);
    if (i >= 0) {
        DesignItem *item = &doc->items[i];
        item->w = 660;
        item->h = 2;
    }
}

/* ------------------------------------------------------------------------- */
/* C code generation                                                         */
/* ------------------------------------------------------------------------- */

typedef struct CodeBuffer {
    char *data;
    size_t cap;
    size_t len;
    int overflow;
} CodeBuffer;

static void code_append(CodeBuffer *buffer, const char *text)
{
    size_t length;
    if (!buffer || buffer->overflow || !text)
        return;
    length = strlen(text);
    if (length >= buffer->cap - buffer->len) {
        buffer->overflow = 1;
        if (buffer->cap > 0)
            buffer->data[buffer->cap - 1] = '\0';
        return;
    }
    memcpy(buffer->data + buffer->len, text, length + 1);
    buffer->len += length;
}

static void code_printf(CodeBuffer *buffer, const char *format, ...)
{
    va_list args;
    int written;
    size_t remaining;

    if (!buffer || buffer->overflow)
        return;
    if (buffer->cap <= buffer->len) {
        buffer->overflow = 1;
        return;
    }
    remaining = buffer->cap - buffer->len;
    va_start(args, format);
    written = vsnprintf(buffer->data + buffer->len, remaining, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= remaining) {
        buffer->overflow = 1;
        buffer->data[buffer->cap - 1] = '\0';
        return;
    }
    buffer->len += (size_t)written;
}

static void code_quoted(CodeBuffer *buffer, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    code_append(buffer, "\"");
    while (*p) {
        unsigned char c = *p++;
        switch (c) {
        case '"':
            code_append(buffer, "\\\"");
            break;
        case '\\':
            code_append(buffer, "\\\\");
            break;
        case '\n':
            code_append(buffer, "\\n");
            break;
        case '\r':
            code_append(buffer, "\\r");
            break;
        case '\t':
            code_append(buffer, "\\t");
            break;
        default:
            if (c < 0x20) {
                char escape[8];
                snprintf(escape, sizeof(escape), "\\%03o", c);
                code_append(buffer, escape);
            } else {
                char one[2];
                one[0] = (char)c;
                one[1] = '\0';
                code_append(buffer, one);
            }
            break;
        }
    }
    code_append(buffer, "\"");
}

static void code_item_name(const DesignItem *item, int index, char *out,
                           size_t out_size)
{
    if (item->name[0]) {
        snprintf(out, out_size, "%s", item->name);
    } else {
        snprintf(out, out_size, "%s%d", designer_kind_prefix(item->kind),
                 index + 1);
    }
}

static void code_font_style(CodeBuffer *buffer, const DesignItem *item)
{
    code_printf(buffer, "    wtk_widget_set_font(%s, NULL, %d, %s);\n",
                item->name, item->font_size,
                item->bold ? "WTK_FONT_BOLD" : "0");
}

static void code_widget(CodeBuffer *buffer, const DesignItem *item)
{
    const char *name = item->name;

    switch (item->kind) {
    case DK_LABEL:
        code_printf(buffer, "    WtkWidget *%s = wtk_label(parent, ", name);
        code_quoted(buffer, item->text);
        code_append(buffer, ");\n");
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_font_style(buffer, item);
        break;

    case DK_BUTTON:
        code_printf(buffer, "    WtkWidget *%s = wtk_button(parent, ", name);
        code_quoted(buffer, item->text);
        code_append(buffer, ");\n");
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        if (item->primary)
            code_printf(buffer,
                        "    wtk_button_set_style(%s, WTK_BUTTON_PRIMARY);\n",
                        name);
        code_font_style(buffer, item);
        break;

    case DK_ENTRY:
        code_printf(buffer, "    WtkWidget *%s = wtk_entry(parent);\n", name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        if (item->text[0]) {
            code_printf(buffer, "    wtk_entry_set_text(%s, ", name);
            code_quoted(buffer, item->text);
            code_append(buffer, ");\n");
        }
        code_printf(buffer, "    wtk_entry_set_placeholder(%s, ", name);
        code_quoted(buffer, designer_kind_placeholder(DK_ENTRY));
        code_append(buffer, ");\n");
        code_font_style(buffer, item);
        break;

    case DK_TEXT:
        code_printf(buffer, "    WtkWidget *%s = wtk_text(parent);\n", name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        if (item->text[0]) {
            code_printf(buffer, "    wtk_text_set_text(%s, ", name);
            code_quoted(buffer, item->text);
            code_append(buffer, ");\n");
        }
        code_printf(buffer, "    wtk_text_set_placeholder(%s, ", name);
        code_quoted(buffer, designer_kind_placeholder(DK_TEXT));
        code_append(buffer, ");\n");
        code_font_style(buffer, item);
        break;

    case DK_CHECKBOX:
        code_printf(buffer, "    WtkWidget *%s = wtk_checkbox(parent, ",
                    name);
        code_quoted(buffer, item->text);
        code_printf(buffer, ", %d);\n", item->checked ? 1 : 0);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_font_style(buffer, item);
        break;

    case DK_LISTBOX:
        code_printf(buffer, "    WtkWidget *%s = wtk_listbox(parent);\n",
                    name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_font_style(buffer, item);
        code_printf(buffer, "    wtk_listbox_add(%s, \"项目一\");\n", name);
        code_printf(buffer, "    wtk_listbox_add(%s, \"项目二\");\n", name);
        code_printf(buffer, "    wtk_listbox_add(%s, \"项目三\");\n", name);
        break;

    case DK_SLIDER:
        code_printf(buffer,
                    "    WtkWidget *%s = wtk_slider(parent, 0, 100, 50);\n",
                    name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        break;

    case DK_PROGRESS:
        code_printf(buffer, "    WtkWidget *%s = wtk_progress(parent);\n",
                    name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_printf(buffer, "    wtk_progress_set(%s, 0.5);\n", name);
        break;

    case DK_SEPARATOR:
        code_printf(buffer, "    WtkWidget *%s = wtk_separator(parent);\n",
                    name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        break;

    case DK_FRAME:
        code_printf(buffer, "    WtkWidget *%s = wtk_frame(parent);\n", name);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_printf(buffer,
                    "    wtk_widget_set_background(%s, "
                    "WTK_RGB(237, 241, 247));\n",
                    name);
        code_printf(buffer,
                    "    wtk_widget_set_border(%s, "
                    "WTK_RGB(220, 226, 236), 1, 8);\n",
                    name);
        break;

    case DK_CANVAS:
        code_printf(buffer,
                    "    WtkWidget *%s = wtk_canvas(parent, %d, %d);\n",
                    name, item->w, item->h);
        code_printf(buffer, "    wtk_place(%s, %d, %d, %d, %d);\n",
                    name, item->x, item->y, item->w, item->h);
        code_printf(buffer,
                    "    wtk_widget_set_background(%s, WTK_RGB(255, 255, 255));\n",
                    name);
        code_printf(buffer,
                    "    /* Add your GDI drawing with wtk_canvas_set_draw(%s, ...). */\n",
                    name);
        break;

    default:
        break;
    }
    code_append(buffer, "\n");
}

int designer_generate_c(const DesignDoc *doc, char *buffer, size_t capacity)
{
    CodeBuffer out;
    DesignDoc blank;
    int i;

    if (!buffer || capacity == 0)
        return 0;

    if (!doc) {
        designer_document_clear(&blank);
        doc = &blank;
    }

    memset(&out, 0, sizeof(out));
    out.data = buffer;
    out.cap = capacity;
    buffer[0] = '\0';

    code_append(&out,
                "/*\n"
                " * Generated by WinTk Designer.\n"
                " * Build as a UTF-8 Win32 C program; link user32, gdi32,\n"
                " * comctl32, uxtheme and the WinTk static library.\n"
                " */\n"
                "#include \"wintk.h\"\n\n");
    code_printf(&out, "#define PAGE_W %d\n", doc->page_w);
    code_printf(&out, "#define PAGE_H %d\n\n", doc->page_h);
    code_append(&out, "static void build_page(WtkWidget *parent)\n{\n");
    if (doc->count == 0) {
        code_append(&out,
                    "    (void)parent; /* Drop widgets onto the page in "
                    "WinTk Designer. */\n");
    }
    for (i = 0; i < doc->count && i < DESIGN_MAX_ITEMS; ++i) {
        char name[DESIGN_NAME_CAP];
        DesignItem item = doc->items[i];
        code_item_name(&item, i, name, sizeof(name));
        snprintf(item.name, sizeof(item.name), "%s", name);
        code_widget(&out, &item);
    }
    code_append(&out, "}\n\n");

    code_append(&out,
                "int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,\n"
                "                   LPSTR command_line, int show_command)\n"
                "{\n"
                "    WtkApp *app = wtk_app_new();\n"
                "    WtkWidget *window;\n"
                "    WtkWidget *page;\n"
                "    (void)instance;\n"
                "    (void)previous;\n"
                "    (void)command_line;\n"
                "    (void)show_command;\n\n"
                "    if (!app)\n"
                "        return 1;\n"
                "    wtk_app_set_style(app, ");
    if (doc->style == 1)
        code_append(&out, "WTK_STYLE_NATIVE");
    else if (doc->style == 2)
        code_append(&out, "WTK_STYLE_WINTK");
    else if (doc->style == 3)
        code_append(&out, "WTK_STYLE_FUSION");
    else
        code_append(&out, "WTK_STYLE_AUTO");
    code_append(&out,
                ");\n"
                "    window = wtk_window(app, ");
    code_quoted(&out, doc->title);
    code_append(&out,
                ",\n"
                "                       PAGE_W + 48, PAGE_H + 120);\n"
                "    if (!window) {\n"
                "        wtk_app_free(app);\n"
                "        return 1;\n"
                "    }\n\n"
                "    page = wtk_frame(window);\n"
                "    wtk_widget_set_background(page, WTK_RGB(255, 255, 255));\n"
                "    wtk_pack(page, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);\n"
                "    build_page(page);\n\n"
                "    wtk_window_center(window);\n"
                "    wtk_run(app);\n"
                "    wtk_app_free(app);\n"
                "    return 0;\n"
                "}\n");

    return out.overflow ? 0 : 1;
}
