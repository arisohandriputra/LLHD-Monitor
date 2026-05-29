# =============================================================================
#  Original Build Date : 13/03/2012
#  Author  : Ari Sohandri Putra
#  GitHub  : https://github.com/arisohandriputra
#  Makefile — LLHDMonitor  (MinGW-w64)
#
#  Usage:
#    make            -> build Release (auto-detect compiler)
#    make clean      -> remove build output
# =============================================================================
ifeq ($(OS),Windows_NT)
    CC      = g++
    WINDRES = windres
else
    CC      = x86_64-w64-mingw32-g++
    WINDRES = x86_64-w64-mingw32-windres
endif

SRCDIR  = src
OBJDIR  = obj
OUTDIR  = bin
TARGET  = $(OUTDIR)/LLHDMonitor.exe

SRCS    = $(SRCDIR)/main.cpp \
          $(SRCDIR)/mainwnd.cpp \
          $(SRCDIR)/smart.cpp

OBJS    = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
RES_O   = $(OBJDIR)/app_res.o

CFLAGS  = -mwindows -O2 \
          -DWIN32 -D_WIN32 -D_WINDOWS -DNDEBUG \
          -I$(SRCDIR) \
          -Wall -Wno-unused-function -Wno-unused-parameter \
          -Wno-unused-variable -Wno-format -Wno-cast-function-type \
          -fpermissive

LDFLAGS = -mwindows \
          -static -static-libgcc -static-libstdc++ \
          -lcomctl32 -lmsimg32 -lshell32 \
          -luser32 -lgdi32 -lkernel32 -ladvapi32

# =============================================================================
.PHONY: all clean

all: $(OUTDIR) $(OBJDIR) $(TARGET)
	@echo ""
	@echo "  Build complete: $(TARGET)"
	@echo ""

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(RES_O): $(SRCDIR)/app.rc $(SRCDIR)/app.manifest $(SRCDIR)/app.ico | $(OBJDIR)
	@touch $(SRCDIR)/app.manifest $(SRCDIR)/app.rc $(SRCDIR)/app.ico 2>/dev/null || true
	@echo "  RC  $(SRCDIR)/app.rc"
	@$(WINDRES) --include-dir=$(SRCDIR) $(SRCDIR)/app.rc -o $(RES_O)

$(TARGET): $(OBJS) $(RES_O) | $(OUTDIR)
	@echo "  LD  $@"
	@$(CC) $(OBJS) $(RES_O) $(LDFLAGS) -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

clean:
	@rm -rf $(OBJDIR) $(OUTDIR)
	@echo "  Clean."
