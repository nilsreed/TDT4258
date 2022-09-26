TARGET = cache_sim

SRC := cache_sim.c

LDFLAGS := -lm

CC = gcc

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(LDFLAGS)  

debug: $(SRC)
	$(CC) -g -o $(TARGET) $(SRC) $(LDFLAGS)
.PHONY: clean
clean:
	rm $(TARGET)
