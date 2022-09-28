CC=cc
CFLAGS=-Wall -Wextra -Wshadow -std=c17 -Wpedantic -ggdb $(shell pkg-config --cflags sdl2)
LDFLAGS=$(shell pkg-config --libs sdl2) -lm

all: whine
whine: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
clean:
ifneq	(,$(wildcard whine))
	rm whine
endif
