#ifndef CHIPPY_H
#define CHIPPY_H

#include <SDL3/SDL.h>
#include "cstack.h"

#define CHIPPY_WINDOW_NAME "CHIPPY-08, A CHIP8 Emulator"
#define CHIPPY_WELCOME_MSG "CHIPPY-08"
#define CHIPPY_WINDOW_WIDTH 600
#define CHIPPY_WINDOW_HEIGHT 600

// Tests primarily taken from
// https://github.com/Timendus/chip8-test-suite?tab=readme-ov-file
#define CHIPPY_ROM_PATH "roms\\5-quirks.ch8"

// Display 
#define CHIPPY_DISPLAY_WIDTH 64
#define CHIPPY_DISPLAY_HEIGHT 32

// Wanted to use 1bit formats for this but SDL3 doesn't support it with textures
// Could do it with surfaces but it converts to 32 bit format when you make textures from surfaces anyway,
// so there's not much point.
#define CHIPPY_DISPLAY_FORMAT SDL_PIXELFORMAT_ABGR32

#define CHIPPY_DISPLAY_TEXTURE_FLAGS SDL_TEXTUREACCESS_STREAMING
#define CHIPPY_DISPLAY_SCALE_MODE SDL_SCALEMODE_NEAREST

// Rom Colors
extern const SDL_Color g_DisplayColors[2];

// App Time
#define CHIPPY_START_SCREEN_TEXT_SIZE 8
#define CHIPPY_FIXED_STEP (1.0 / 60.0)
#define CHIPPY_START_SCREEN_DELAY 2.0

#define SECONDS(t) ((double)(t) * 0.001)
#define MILLISECONDS(t) (uint64_t)(t * 1000.0)

extern double g_TimeStep;
extern uint64_t g_DeltaTime;
extern uint64_t g_CurrentTime;

// Rom Memory
#define CHIPPY_ROM_MEM_SIZE 4096
#define CHIPPY_STARTING_PROGRAM_COUNTER 0x200

// Rom Instructions
#define CHIPPY_CYCLES_PER_SEC 700.0 // Instruction Limit
#define CHIPPY_SEC_PER_CYCLE (1.0 / CHIPPY_CYCLES_PER_SEC)

#define NNN(x) (x & 0x0FFF)
#define NN(x) (x & 0x00FF)
#define N(x) (x & 0x000F)
#define X(x) ((x & 0x0F00) >> 8)
#define Y(x) ((x & 0x00F0) >> 4)

#define OP(x) ((x & 0xF000) >> 12)

typedef void (*CHIPPY_FPtr)(uint16_t);

// Rom Inputs

/*
    CHIP8-Hex            PC-Scancode
    1 2 3 C              1 2 3 4
    4 5 6 D      ->      Q W E R
    7 8 9 E              A S D F
    A 0 B F              Z X C V
*/
extern const uint8_t g_InputHexTable[16];
extern const uint64_t g_InputBitMask;
extern uint64_t g_InputBitMap;

#define GET_ANY_INPUT_DOWN (g_InputBitMap & g_InputBitMask)
#define GET_INPUT(code) ((g_InputBitMap >> code) & 1ull)
#define GET_INPUT_FROM_HEX(input) (GET_INPUT(g_InputHexTable[input - 1]))
// (!!isDown) is a trick to ensure this value is 0 or 1 and nothing else
#define SET_INPUT(code, isDown) g_InputBitMap = (g_InputBitMap & ~(1ull << code)) | ((uint64_t)(!!isDown) << code);
#define IS_VALID_INPUT(code) (g_InputBitMask & (1ull << code))

SDL_AppResult CHIPPY_Init(SDL_Renderer* g_Renderer);
void CHIPPY_Shutdown();

void CHIPPY_Update();
void CHIPPY_WelcomeMsg(SDL_Renderer* g_Renderer);
SDL_AppResult CHIPPY_InputEvent(SDL_Scancode key_code, int IsDown);

uint32_t* CHIPPY_GetDisplayBuffer();
SDL_Texture* CHIPPY_GetDisplayTexture();

#endif