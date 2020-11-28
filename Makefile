CCFLAGS := -std=c11 -Wall -Wextra
EMFLAGS := -s USE_SDL=2

all:
	emcc src/main.c $(CCFLAGS) $(EMFLAGS) -o dist/breakout.js

release: CCFLAGS += -O3
release: EMFLAGS += -s FILESYSTEM=0
release: all
