CC = gcc
CFLAGS = -Wall -O2 -Iinclude
TARGET = shell
SRC = src/shell.c
OBJ = object/shell.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

object/shell.o: $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
