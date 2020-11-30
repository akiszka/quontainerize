CC := clang++
CFLAGS := -std=c++17 -O3

OBJS = main.o

.PHONY: all clean test

all: containers/masno engine

$(OBJS): $(OBJS:.o=.cpp)
	$(CC) $(CFLAGS) -c $<

engine: $(OBJS)
	$(CC) $(CFLAGS) $< -o engine

containers/masno: containers/masno.cpp
	$(CC) $(CFLAGS) $< -o $@

test:
	sudo ./engine

clean:
	rm $(OBJS) engine containers/masno
