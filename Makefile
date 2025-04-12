CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=openwrt_management
LDFLAGS=

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Create directory structure if it doesn't exist
setup:
	mkdir -p public/css public/js public/img templates

# Install dependencies (none required for the OpenWRT management interface)
install:
	@echo "No external dependencies required"

.PHONY: all clean run setup install