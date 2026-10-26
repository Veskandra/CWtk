# WinTk - MinGW-w64 build (run from a Windows shell).
#
#   mingw32-make            build the library, examples and designer
#   mingw32-make gallery    build only the gallery
#   mingw32-make hello      build only the hello example
#   mingw32-make designer   build only the visual designer
#   mingw32-make run        launch the gallery
#   mingw32-make run-designer  launch the visual designer
#   mingw32-make clean
#
# For CMake use the CMakeLists.txt instead.

CC       := gcc
WINDRES  := windres
AR       := ar

BUILD    := build
OBJDIR   := $(BUILD)/obj
LIB      := $(BUILD)/libwintk.a
RES      := $(BUILD)/wintk-res.o
GALLERY  := $(BUILD)/wintk_gallery.exe
HELLO    := $(BUILD)/hello.exe
DESIGNER := $(BUILD)/wintk_designer.exe

CPPFLAGS += -DUNICODE -D_UNICODE -Iinclude -Isrc
CFLAGS   ?= -O2
CFLAGS   += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS   += -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS  += -mwindows -static-libgcc
LDLIBS   += -luser32 -lgdi32 -lcomctl32

LIB_SRCS := \
	src/wtk_core.c \
	src/wtk_draw.c \
	src/wtk_layout.c \
	src/wtk_native.c \
	src/wtk_widgets.c

LIB_OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(LIB_SRCS))

.PHONY: all gallery hello designer library run run-designer clean

all: library gallery hello designer

library: $(LIB)

gallery: $(GALLERY)

hello: $(HELLO)

designer: $(DESIGNER)

$(BUILD) $(OBJDIR):
	@if not exist "$@" mkdir "$@"

$(OBJDIR)/%.o: src/%.c src/wtk_internal.h include/wintk.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS) | $(BUILD)
	$(AR) rcs $@ $^

$(RES): resources/wintk.rc resources/wintk.manifest resources/wintk.ico | $(BUILD)
	$(WINDRES) -I resources -O coff $< -o $@

$(GALLERY): examples/gallery.c $(LIB) $(RES) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(RES) -o $@ $(LDFLAGS) $(LDLIBS)

$(HELLO): examples/hello.c $(LIB) $(RES) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(RES) -o $@ $(LDFLAGS) $(LDLIBS)

$(DESIGNER): examples/designer.c examples/designer_model.c examples/designer_model.h $(LIB) $(RES) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/designer.c examples/designer_model.c $(LIB) $(RES) -o $@ $(LDFLAGS) $(LDLIBS) -lcomdlg32

run: $(GALLERY)
	@start "" "$(GALLERY)"

run-designer: $(DESIGNER)
	@start "" "$(DESIGNER)"

clean:
	@if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"
