#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define MAX_BLOCKS 64

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

struct Block {
  SDL_Rect rect;
  bool alive;
  uint8_t color;
};

SDL_Renderer *renderer;
SDL_Event event;

SDL_Rect paddle = { .x = 0, .y = SCREEN_HEIGHT - 6, .w = 16, .h = 3 };
SDL_Rect ball = { .w = 2, .h = 2 };
int ballDx;
int ballDy;
struct Block blocks[MAX_BLOCKS];
size_t blockCount;
unsigned int runAfter;

const uint8_t blockColors[4][3] = {
  { 0x63, 0x66, 0x62 },
  { 0xC5, 0x1F, 0x29 },
  { 0x12, 0x76, 0x0C },
  { 0x82, 0x47, 0xBF }
};

void reset();
void generateLevel();
void frame();
void update();
void render();

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
  SDL_EventState(SDL_KEYDOWN, SDL_DISABLE);
  SDL_EventState(SDL_KEYUP, SDL_DISABLE);

  SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  srand(time(NULL));
  reset();
  emscripten_set_main_loop(frame, 0, 1);
}

void reset() {
  ball.x = SCREEN_WIDTH / 2 - 1;
  ball.y = SCREEN_HEIGHT - 20;
  ballDx = 1;
  ballDy = -1;

  do {
    generateLevel();
  } while (blockCount == 0);

  runAfter = SDL_GetTicks() + 1000;
}

void generateLevel() {
  blockCount = 0;
  int blockY = 8;
  while (blockCount < MAX_BLOCKS && blockY <= 64) {
    int blocksInRow = rand() % 16;
    if (blocksInRow > 10) {
      blocksInRow = 0;
    } else if (blockCount + blocksInRow > MAX_BLOCKS) {
      blocksInRow = MAX_BLOCKS - blockCount;
    }
    blockY += 6;

    if (blocksInRow > 0) {
      const int color = rand() % 4;
      const int colorSpread = 1 + rand() % 2;
      const int colorRunLength = 1 + rand() % 2;
      const int colorRunSkip = 1 + rand() % 3;
      const int width = min(10 + rand() % max(1, SCREEN_WIDTH / blocksInRow - 10), SCREEN_WIDTH / 4);
      int offset;
      switch (rand() % 8) {
        case 0:
          offset = 4;
          break;
        case 1:
          offset = SCREEN_WIDTH - 4 - blocksInRow * width;
          break;
        default:
          offset = (SCREEN_WIDTH - blocksInRow * width) / 2;
      }

      for (int i = 0; i < blocksInRow; ++i) {
        struct Block* block = &blocks[blockCount + i];
        block->alive = 1;
        block->rect.w = width;
        block->rect.h = 6;
        block->rect.x = offset + i * width;
        block->rect.y = blockY;
        block->color = (color + ((i / colorRunLength) % colorSpread) * colorRunSkip) % 4;
      }
      blockCount += blocksInRow;
    }
  }
}

void frame() {
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_MOUSEMOTION) {
      paddle.x = event.motion.x - paddle.w / 2;
      if (paddle.x < 0) {
        paddle.x = 0;
      } else if (paddle.x >= SCREEN_WIDTH - paddle.w) {
        paddle.x = SCREEN_WIDTH - paddle.w;
      }
    }
  }

  if (SDL_GetTicks() >= runAfter) {
    update();
  }
  render();
}

void update() {
  ball.x += ballDx;
  ball.y += ballDy;

  if (ball.x <= 0 || ball.x + ball.w >= SCREEN_WIDTH) {
    ballDx = -ballDx;
  }
  if (ball.y <= 0) {
    ballDy = -ballDy;
  } else if (
    ball.y + ball.h == paddle.y
    && ball.x + ball.w >= paddle.x
    && ball.x < paddle.x + paddle.w
  ) {
    ballDy = -ballDy;
  } else if (ball.y + ball.h >= SCREEN_HEIGHT) {
    reset();
    return;
  }

  bool reflectX = false;
  bool reflectY = false;
  bool anyBlockAlive = false;
  for (size_t i = 0; i < blockCount; ++i) {
    if (blocks[i].alive) {
      const SDL_Rect* rect = &blocks[i].rect;
      if (
        (ball.x + ball.w >= rect->x - 1 && ball.x <= rect->x + rect->w)
        && (ball.y + ball.h > rect->y && ball.y < rect->y + rect->h)
      ) {
        blocks[i].alive = false;
        reflectX = true;
      }
      if (
        (ball.y + ball.h >= rect->y - 1 && ball.y <= rect->y + rect->h)
        && (ball.x + ball.w > rect->x && ball.x < rect->x + rect->w)
      ) {
        blocks[i].alive = false;
        reflectY = true;
      }
    }
    anyBlockAlive |= blocks[i].alive;
  }
  if (!anyBlockAlive) {
    reset();
    return;
  }
  ballDx = reflectX ? -ballDx : ballDx;
  ballDy = reflectY ? -ballDy : ballDy;
}

void render() {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  for (size_t i = 0; i < blockCount; ++i) {
    const struct Block* block = &blocks[i];
    if (block->alive) {
      const uint8_t* color = blockColors[block->color];
      SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], 255);
      SDL_RenderDrawRect(renderer, &block->rect);
    }
  }

  SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
  SDL_RenderDrawRect(renderer, &paddle);

  SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
  SDL_RenderFillRect(renderer, &ball);

  SDL_RenderPresent(renderer);
}
