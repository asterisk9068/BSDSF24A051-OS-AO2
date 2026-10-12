# Makefile for PA-02: ls Utility
# Instructor: Muhammad Arif Butt, PhD

CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = bin/ls
SRC     = src/ls-v1.0.0.c
OBJ     = obj/ls-v1.0.0.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
