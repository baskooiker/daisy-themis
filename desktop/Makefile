# Themis Desktop - Simple Makefile (no CMake required)
# Prerequisites: sudo apt install libsdl2-dev librtmidi-dev libgl1-mesa-dev

CXX = g++
SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)
CXXFLAGS = -std=c++17 -O2 -Wall -I. -I../core -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends $(SDL2_CFLAGS)
LDFLAGS = $(SDL2_LIBS) -lGL -lrtmidi -lpthread -ldl

# ImGui - will be downloaded if not present
IMGUI_DIR = imgui
IMGUI_URL = https://github.com/ocornut/imgui/archive/refs/tags/v1.90.1.tar.gz

# Source files
CORE_SRCS = ../core/themis_platform.cpp \
            ../core/themis_data.cpp \
            ../core/themis_patterns.cpp \
            ../core/themis_melody.cpp \
            ../core/themis_chords.cpp \
            ../core/themis_sequencer.cpp \
            ../core/themis_rhythm.cpp \
            ../core/themis_acid.cpp

DESKTOP_SRCS = main.cpp \
               platform_desktop.cpp \
               audio.cpp \
               ui.cpp \
               ui_synth.cpp \
               ui_mixer.cpp \
               ui_pattern.cpp \
               midi_out.cpp \
               config.cpp

IMGUI_SRCS = $(IMGUI_DIR)/imgui.cpp \
             $(IMGUI_DIR)/imgui_demo.cpp \
             $(IMGUI_DIR)/imgui_draw.cpp \
             $(IMGUI_DIR)/imgui_tables.cpp \
             $(IMGUI_DIR)/imgui_widgets.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

ALL_SRCS = $(CORE_SRCS) $(DESKTOP_SRCS) $(IMGUI_SRCS)
OBJS = $(ALL_SRCS:.cpp=.o)

TARGET = themis_desktop

# Add MIDI support flag
CXXFLAGS += -DTHEMIS_ENABLE_MIDI

.PHONY: all clean imgui

all: imgui $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Download and extract ImGui if not present
imgui:
	@if [ ! -d "$(IMGUI_DIR)" ]; then \
		echo "Downloading ImGui..."; \
		curl -L $(IMGUI_URL) -o imgui.tar.gz; \
		tar xzf imgui.tar.gz; \
		mv imgui-1.90.1 $(IMGUI_DIR); \
		rm imgui.tar.gz; \
		echo "ImGui downloaded."; \
	fi

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f ../core/*.o

distclean: clean
	rm -rf $(IMGUI_DIR)

# Dependencies
main.o: main.cpp platform_desktop.h audio.h ui.h config.h
platform_desktop.o: platform_desktop.cpp platform_desktop.h
audio.o: audio.cpp audio.h
ui.o: ui.cpp ui.h
midi_out.o: midi_out.cpp midi_out.h
config.o: config.cpp config.h
