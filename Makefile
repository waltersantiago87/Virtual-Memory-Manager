CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -pthread
TARGET = vm

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
