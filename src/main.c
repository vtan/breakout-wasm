#include <emscripten.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define MAX_BLOCKS 64
#define UPDATES_PER_SEC (2 * 60)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

struct Block {
  SDL_Rect rect;
  bool alive;
  uint8_t color;
};

struct Ball {
  SDL_Rect rect;
  double x;
  double y;
  double dx;
  double dy;
};

SDL_Renderer *renderer;
SDL_Event event;

SDL_Rect paddle = { .x = 0, .y = SCREEN_HEIGHT - 6, .w = 20, .h = 3 };
struct Ball ball = { .rect = { .w = 3, .h = 3 } };
struct Block blocks[MAX_BLOCKS];
size_t blockCount;
uint64_t ticksPerUpdate;
uint64_t ticksAccumulated = 0;
uint64_t ticksLastFrame;
uint64_t ticksNow;
uint64_t runAfterTicks;

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
bool rectsIntersect(SDL_Rect, SDL_Rect);
void render();

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
  SDL_EventState(SDL_KEYDOWN, SDL_DISABLE);
  SDL_EventState(SDL_KEYUP, SDL_DISABLE);

  ticksPerUpdate = (uint64_t) (SDL_GetPerformanceFrequency() / (double) UPDATES_PER_SEC);
  ticksLastFrame = SDL_GetPerformanceCounter();

  SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  srand(time(NULL));
  reset();
  emscripten_set_main_loop(frame, 0, 1);
}

void reset() {
  ball.x = SCREEN_WIDTH / 2 - 1;
  ball.y = SCREEN_HEIGHT - 20;
  ball.dx = 0.5;
  ball.dy = -0.6;
  ball.rect.x = ball.x;
  ball.rect.y = ball.y;

  do {
    generateLevel();
  } while (blockCount == 0);

  runAfterTicks = ticksNow + UPDATES_PER_SEC * ticksPerUpdate;
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
  ticksNow = SDL_GetPerformanceCounter();
  ticksAccumulated += ticksNow - ticksLastFrame;
  if (ticksAccumulated >= ticksPerUpdate) {
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

    for (; ticksAccumulated >= ticksPerUpdate; ticksAccumulated -= ticksPerUpdate) {
      if (ticksNow >= runAfterTicks) {
        update();
      }
    }

    render();
  }
  ticksLastFrame = ticksNow;
}

void update() {
  ball.x += ball.dx;
  ball.y += ball.dy;
  ball.rect.x = (int) round(ball.x);
  ball.rect.y = (int) round(ball.y);

  if (ball.rect.x <= 0 || ball.rect.x + ball.rect.w >= SCREEN_WIDTH) {
    ball.dx = -ball.dx;
  }
  if (ball.rect.y <= 0) {
    ball.dy = -ball.dy;
  } else if (
    ball.dy > 0
    && ball.rect.y + ball.rect.h >= paddle.y + 1
    && rectsIntersect(ball.rect, paddle)
  ) {
    const double ratio = (ball.rect.x - paddle.x) / (double) paddle.w;
    if (ratio < 0.25) {
      ball.dx = -0.6;
      ball.dy = -0.5;
    } else if (ratio < 0.5) {
      ball.dx = -0.5;
      ball.dy = -0.6;
    } else if (ratio < 0.75) {
      ball.dx = 0.5;
      ball.dy = -0.6;
    } else {
      ball.dx = 0.6;
      ball.dy = -0.5;
    }
  } else if (ball.rect.y + ball.rect.h >= SCREEN_HEIGHT) {
    reset();
    return;
  }

  bool reflectX = false;
  bool reflectY = false;
  bool anyBlockAlive = false;
  for (size_t i = 0; i < blockCount; ++i) {
    if (blocks[i].alive) {
      const SDL_Rect* rect = &blocks[i].rect;
      if (rectsIntersect(ball.rect, *rect)) {
        blocks[i].alive = false;
        reflectX = ball.rect.x + ball.rect.w - 1 == rect->x || ball.rect.x == rect->x + rect->w - 1;
        reflectY = ball.rect.y + ball.rect.h - 1 == rect->y || ball.rect.y == rect->y + rect->h - 1;
      }
    }
    anyBlockAlive |= blocks[i].alive;
  }
  if (!anyBlockAlive) {
    reset();
    return;
  }
  ball.dx = reflectX ? -ball.dx : ball.dx;
  ball.dy = reflectY ? -ball.dy : ball.dy;
}

bool rectsIntersect(SDL_Rect rect1, SDL_Rect rect2) {
  return rect1.x < rect2.x + rect2.w
    && rect1.x + rect1.w > rect2.x
    && rect1.y < rect2.y + rect2.h
    && rect1.y + rect1.h > rect2.y;
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
  SDL_RenderFillRect(renderer, &ball.rect);

  SDL_RenderPresent(renderer);
}
