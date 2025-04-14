CC=gcc
CFLAGS=-Wall -Wextra -O2 -Wno-implicit-function-declaration -Wno-int-conversion -Wno-unused-variable -Wno-unused-function -Wno-unused-result -Wno-sign-compare -Wno-format
TARGET=openwrt_management
LDFLAGS=

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -I. -o $(TARGET) main.c ur_management.c $(LDFLAGS)

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
