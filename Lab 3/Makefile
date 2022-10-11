TARGET = stetris

SRC := stetris.c

#LDFLAGS := ...

CC = gcc

$(TARGET): $(SRC)
	$(CC) -o $@ $^

debug: $(SRC)
	$(CC) -g -o $(TARGET) $(SRC)

.PHONY: clean

clean:
	rm $(TARGET)
