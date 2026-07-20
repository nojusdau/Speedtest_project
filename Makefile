CC       := gcc
CFLAGS   := -Wall -Wextra -O2 -std=gnu11 -Isrc
LDLIBS   := -lcurl -lm

SRC_DIR  := src
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/speedtest

SOURCES  := $(wildcard $(SRC_DIR)/*.c)
OBJECTS  := $(SOURCES:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o
	rm -rf $(BIN_DIR)

run: all
	./$(TARGET) --help
