# Makefile location: src/app/opencv/Makefile

# ===== Resolve project root as an absolute path =====
THISDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT    := $(abspath $(THISDIR)/../../..)

BUILDDIR := $(ROOT)/build/opencv
BINDIR   := $(ROOT)/bin
TARGET   := opencv_app
OUT      := $(BINDIR)/$(TARGET)

CC  := gcc
CXX := g++

CSTD   := -std=c11
CXXSTD := -std=c++17
WARN   := -Wall -Wextra -Wno-dangling-else -Wmissing-field-initializers  -Wpointer-sign -Wtautological-constant-out-of-range-compare

BUILD ?= debug
ifeq ($(BUILD),release)
  OPT := -O2 -DNDEBUG
else
  OPT := -O0 -g
endif

INCS := -I$(ROOT)/src/app -I$(ROOT)/src/app/opencv -I$(ROOT)/src/app/driver -I$(ROOT)/src/app/helper -I$(ROOT)/src/app/ui -I$(ROOT)/src/app/app
DEFS := -DENABLE_OPENCV \
    -DENABLE_FMRADIO=1 \
    -DENABLE_VOX=1 \
    -DENABLE_DTMF_CALLING=1 \
    -DENABLE_FLASHLIGHT=1 \
    -DENABLE_BIG_FREQ=1 \
    -DENABLE_KEEP_MEM_NAME=1 \
    -DENABLE_WIDE_RX=1 \
    -DENABLE_NO_CODE_SCAN_TIMEOUT=1 \
    -DENABLE_AM_FIX=1 \
    -DENABLE_SQUELCH_MORE_SENSITIVE=1 \
    -DENABLE_FASTER_CHANNEL_SCAN=1 \
    -DENABLE_RSSI_BAR=1 \
    -DENABLE_COPY_CHAN_TO_VFO=1 \
    -DENABLE_SPECTRUM=1 \
    -DENABLE_REDUCE_LOW_MID_TX_POWER=0 \
    -DENABLE_SCAN_RANGES=1 \
    -DENABLE_MDC1200=1 \
    -DENABLE_MDC1200_CONTACT=1 \
    -DENABLE_MDC1200_EDIT=1 \
    -DENABLE_CHINESE_FULL=4 \
    -DENABLE_CUSTOM_SIDEFUNCTIONS=1 \
    -DENABLE_SIDEFUNCTIONS_SEND=1 \
    -DENABLE_TURN=1 \
    -DENABLE_WARNING=1 \
    -DENABLE_PINYIN=1 \
    -DENABLE_MESSENGER=1 \
    -DENABLE_MESSENGER_DELIVERY_NOTIFICATION=1 \
    -DENABLE_MESSENGER_NOTIFICATION=1 \


FORCE_INC := -include stdio.h 

# ---- GLFW (auto detect via pkg-config; fallback to common Homebrew paths) ----
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell pkg-config --libs   glfw3 2>/dev/null)
ifeq ($(strip $(GLFW_CFLAGS)),)
  GLFW_CFLAGS := -I/opt/homebrew/include -I/usr/local/include
endif
ifeq ($(strip $(GLFW_LIBS)),)
  GLFW_LIBS := -L/opt/homebrew/lib -L/usr/local/lib -lglfw
endif

CFLAGS   := $(CSTD)   $(WARN) $(OPT) $(INCS) $(GLFW_CFLAGS) $(DEFS) $(FORCE_INC) -MMD -MP
CXXFLAGS := $(CXXSTD) $(WARN) $(OPT) $(INCS) $(GLFW_CFLAGS) $(DEFS) $(FORCE_INC) -MMD -MP
LDFLAGS  :=
LDLIBS   := $(GLFW_LIBS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LDFLAGS += -framework ApplicationServices -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME_S),Linux)
  LDLIBS += -lX11
endif

# ===== Auto discover sources under $(ROOT)/src =====
C_SRCS_ABS   := $(shell find $(ROOT)/src/app -type f -name '*.c')
CPP_SRCS_ABS := $(shell find $(ROOT)/src/app -type f -name '*.cpp')

# Convert to paths relative to $(ROOT) (e.g., src/app/main.cpp)
C_SRCS   := $(patsubst $(ROOT)/%,%,$(C_SRCS_ABS))
CPP_SRCS := $(patsubst $(ROOT)/%,%,$(CPP_SRCS_ABS))

OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(C_SRCS)) \
        $(patsubst %.cpp,$(BUILDDIR)/%.o,$(CPP_SRCS))

DEPS := $(OBJS:.o=.d)

.PHONY: all clean run
all: $(OUT)

$(OUT): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# C sources (note the prerequisite is under $(ROOT))
$(BUILDDIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# C++ sources
$(BUILDDIR)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

run: $(OUT)
	$(OUT)

clean:
	rm -rf $(BUILDDIR) $(OUT)
