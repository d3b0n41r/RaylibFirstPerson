# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -O2

# Paths
SRC = main.c
OUT = game

# Libraries
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# flags
CFLAGS = -Wno-unused-variable -Wno-unused-function

# Build target
all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

# Clean build artifacts
clean:
	rm -f $(OUT)
