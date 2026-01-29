# CFBASIC Makefile
# Cross-platform build system for Linux, macOS, and Windows

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lm
TARGET = basic
SOURCES = cfbasic.c interpreter.c lexer.c utils.c editor.c
OBJECTS = $(SOURCES:.c=.o)

# Platform detection
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    CFLAGS += -D_POSIX_C_SOURCE=200809L
endif

ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
    CFLAGS += -D_POSIX_C_SOURCE=200809L
endif

ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    TARGET = basic.exe
    LDFLAGS =
endif

# Default target
all: $(TARGET)

# Build executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET) for $(PLATFORM)"

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) basic.exe

# Install (Linux/macOS only)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Test
test: $(TARGET)
	@echo "Running basic tests..."
	@echo '10 PRINT "HELLO, WORLD!"' | ./$(TARGET)

# Windows build (using MinGW)
windows:
	x86_64-w64-mingw32-gcc $(CFLAGS) $(SOURCES) -o basic.exe

# Help
help:
	@echo "CFBASIC Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build basic (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  test      - Run basic tests"
	@echo "  windows   - Cross-compile for Windows using MinGW"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall test windows help
