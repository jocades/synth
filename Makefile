CC := clang
BUILD_DIR := ./build
CLAGS := -std=c11 -Wall -Wextra
FRAMEWORKS := -framework AudioToolBox -framework ApplicationServices

$(BUILD_DIR)/main: main.c
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $@ $<

run: $(BUILD_DIR)/main
	$(BUILD_DIR)/main

.PHONY: run
