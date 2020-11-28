all:
	emcc src/main.c -std=c11 -Wall -Wextra -o dist/breakout.js -s USE_SDL=2
