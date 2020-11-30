CC := clang++
CFLAGS := -std=c++17 -O3

OBJS = main.o container.o config.o

.PHONY: all clean test

all: containers/masno.container engine

.cpp.o:
	$(CC) $(CFLAGS) -c $<

engine: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o engine

containers/masno.container: containers/masno.cpp
	$(CC) $(CFLAGS) $< -o $@

test:
	sudo ./engine

clean:
	rm $(OBJS) engine containers/masno.container
