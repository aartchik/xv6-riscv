CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic

TARGET = fifo_logger

all: $(TARGET)

$(TARGET): fifo_logger.c
	$(CC) $(CFLAGS) -o $(TARGET) fifo_logger.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
