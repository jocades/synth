CC := clang
CFLAGS := -std=c11 -Wall -Wextra -fcolor-diagnostics
LIBS := -framework AudioToolBox -framework ApplicationServices -lncurses

ifeq ($(mode),debug)
	CFLAGS += -O0 -g
	BUILD_DIR := build/debug
else
	CFLAGS += -O2
	BUILD_DIR := build/release
endif

NAME := synth
TARGET := $(BUILD_DIR)/$(NAME)

$(TARGET): main.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

run: $(TARGET)
	$(TARGET)

.PHONY: run
