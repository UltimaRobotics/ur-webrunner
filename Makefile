# Makefile for OpenWRT Management Interface

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = openwrt_management
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install:
	install -m 755 $(TARGET) /usr/bin/

.PHONY: all clean install
