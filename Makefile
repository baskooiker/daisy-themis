# Project Name
TARGET = Themis

# Sources
CPP_SOURCES = Themis.cpp \
              globals.cpp \
              groove.cpp \
              drums.cpp \
              melody.cpp \
              display.cpp \
              config.cpp \
              core/themis_platform.cpp \
              core/themis_data.cpp \
              core/themis_patterns.cpp \
              core/themis_melody.cpp \
              core/themis_sequencer.cpp

# Include paths
C_INCLUDES += -I.

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
