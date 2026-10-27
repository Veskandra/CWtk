# WinTk Designer - standalone MinGW-w64 build.
#
#   mingw32-make -C designer
#   designer\build\wintk_designer.exe
#
# This subproject compiles its own copy of the framework so it can be built
# without first building the repository root or examples.

CC       := gcc
WINDRES  := windres
ROOT     := ..
BUILD    := build
OBJDIR   := $(BUILD)/obj
STYLEDIR := $(BUILD)/styles
EXE      := $(BUILD)/wintk_designer.exe
STYLE_DLL:= $(STYLEDIR)/wintk_builtin_styles.dll
RES      := $(BUILD)/wintk-res.o

CPPFLAGS += -DUNICODE -D_UNICODE -I$(ROOT)/include -I$(ROOT)/src -Isrc \
	-Istyles
CFLAGS   ?= -O2
CFLAGS   += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS   += -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS  += -mwindows -static-libgcc
LDLIBS   += -luser32 -lgdi32 -lcomctl32 -luxtheme -lcomdlg32

LIB_SRCS := \
	$(ROOT)/src/wtk_core.c \
	$(ROOT)/src/wtk_draw.c \
	$(ROOT)/src/wtk_layout.c \
	$(ROOT)/src/wtk_native.c \
	$(ROOT)/src/wtk_widgets.c

LIB_OBJS := $(patsubst $(ROOT)/src/%.c,$(OBJDIR)/lib_%.o,$(LIB_SRCS))
APP_OBJS := $(OBJDIR)/designer.o $(OBJDIR)/designer_model.o
STYLE_OBJ := $(OBJDIR)/designer_style.o

.PHONY: all clean run

all: $(EXE) $(STYLE_DLL)

$(BUILD) $(OBJDIR) $(STYLEDIR):
	@if not exist "$@" mkdir "$@"

$(OBJDIR)/lib_%.o: $(ROOT)/src/%.c $(ROOT)/src/wtk_internal.h $(ROOT)/include/wintk.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/designer.o: src/designer.c src/designer_model.h \
		styles/style_plugin.h $(ROOT)/include/wintk.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/designer_model.o: src/designer_model.c src/designer_model.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/designer_style.o: styles/designer_style.c styles/designer_style.h \
		styles/style_plugin.h $(ROOT)/src/wtk_internal.h \
		$(ROOT)/include/wintk.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(RES): $(ROOT)/resources/wintk.rc $(ROOT)/resources/wintk.manifest $(ROOT)/resources/wintk.ico | $(BUILD)
	$(WINDRES) -I $(ROOT)/resources -O coff $< -o $@

$(EXE): $(APP_OBJS) $(LIB_OBJS) $(RES) | $(BUILD)
	$(CC) $(APP_OBJS) $(LIB_OBJS) $(RES) -o $@ $(LDFLAGS) $(LDLIBS)

$(STYLE_DLL): $(STYLE_OBJ) $(LIB_OBJS) | $(STYLEDIR)
	$(CC) -shared $(STYLE_OBJ) $(LIB_OBJS) -o $@ \
		-static-libgcc -luser32 -lgdi32 -lcomctl32 -luxtheme

run: $(EXE) $(STYLE_DLL)
	@start "" "$(EXE)"

clean:
	@if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"
