#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_Window *g_Window = NULL;
SDL_Renderer *g_Renderer = NULL;

#include <stdio.h>
#include <stdlib.h>

#include "cstack.h"

#define CHIPPY_ROM_PATH "roms\\1-ibm-logo.ch8"

// Display 
#define CHIPPY_DISPLAY_WIDTH 64
#define CHIPPY_DISPLAY_HEIGHT 32

// Wanted to use 1bit formats for this but SDL3 doesn't support it with textures
// Could do it with surfaces but it converts to RGBA32 when you make textures from surfaces anyway,
// so there's not much point.
#define CHIPPY_DISPLAY_FORMAT SDL_PIXELFORMAT_RGBA32

#define CHIPPY_DISPLAY_TEXTURE_FLAGS SDL_TEXTUREACCESS_STREAMING
#define CHIPPY_DISPLAY_SCALE_MODE SDL_SCALEMODE_NEAREST

// RGBA32
uint32_t g_DisplayBuffer[CHIPPY_DISPLAY_HEIGHT][CHIPPY_DISPLAY_WIDTH];
SDL_Texture* g_DisplayTexture = NULL;
SDL_Color g_DisplayColors[2] =
{
    { 0, 0, 0, 255 }, // off color
    { 255, 30, 30, 255 } // on color
};

// App Time
#define CHIPPY_TEXT_SIZE 8
#define CHIPPY_FIXED_STEP (1.0 / 60.0)
#define CHIPPY_START_SCREEN_DELAY 2.0

#define SECONDS(t) ((double)(t) * 0.001)
#define MILLISECONDS(t) (uint64_t)(t * 1000.0)

double g_TimeStep = CHIPPY_FIXED_STEP;
uint64_t g_DeltaTime = 0;
uint64_t g_CurrentTime = 0;

// Rom Time
bool g_GamePaused = false;
uint8_t g_DelayTimer = 0;
uint8_t g_SoundTimer = 0;
double g_GameTimer = 0;
double g_CycleTimer = 0;

// Rom Memory
#define CHIPPY_ROM_MEM_SIZE 4096
#define CHIPPY_STARTING_PROGRAM_COUNTER 0x200

uint16_t g_ProgramCounter = CHIPPY_STARTING_PROGRAM_COUNTER;
uint8_t g_VariableRegs[16];
uint16_t g_IndexReg = 0;
Cstack* g_AddressStack = NULL;
uint8_t* g_RomMemory = NULL;
uint8_t g_RomSize = 0;

// Rom Instructions
#define CHIPPY_CYCLES_PER_SEC 700 // Instruction Limit
#define CHIPPY_SEC_PER_CYCLE (1.0 / CHIPPY_CYCLES_PER_SEC)

#define NNN(x) (x & 0x0FFF)
#define NN(x) (x & 0x00FF)
#define N(x) (x & 0x000F)
#define X(x) ((x & 0x0F00) >> 8)
#define Y(x) ((x & 0x00F0) >> 4)

typedef void (*CHIPPY_FPtr)(uint16_t);

// Rom Fonts
const uint16_t g_Font[16][5] =
{
    { 0xF0, 0x90, 0x90, 0x90, 0xF0 }, // 0
    { 0x20, 0x60, 0x20, 0x20, 0x70 }, // 1
    { 0xF0, 0x10, 0xF0, 0x80, 0xF0 }, // 2
    { 0xF0, 0x10, 0xF0, 0x10, 0xF0 }, // 3
    { 0x90, 0x90, 0xF0, 0x10, 0x10 }, // 4
    { 0xF0, 0x80, 0xF0, 0x10, 0xF0 }, // 5
    { 0xF0, 0x80, 0xF0, 0x90, 0xF0 }, // 6
    { 0xF0, 0x10, 0x20, 0x40, 0x40 }, // 7
    { 0xF0, 0x90, 0xF0, 0x90, 0xF0 }, // 8
    { 0xF0, 0x90, 0xF0, 0x10, 0xF0 }, // 9
    { 0xF0, 0x90, 0xF0, 0x90, 0x90 }, // A
    { 0xE0, 0x90, 0xE0, 0x90, 0xE0 }, // B
    { 0xF0, 0x80, 0x80, 0x80, 0xF0 }, // C
    { 0xE0, 0x90, 0x90, 0x90, 0xE0 }, // D
    { 0xF0, 0x80, 0xF0, 0x80, 0xF0 }, // E
    { 0xF0, 0x80, 0xF0, 0x80, 0x80 }  // F
};
const uint32_t g_FontStartAddress = 0x50;
const uint32_t g_FontHeight = 5;

// Rom Inputs
const uint64_t g_InputBitMask = 1ull << SDL_SCANCODE_1 |
                                       1ull << SDL_SCANCODE_2 |
                                       1ull << SDL_SCANCODE_3 |
                                       1ull << SDL_SCANCODE_4 |
                                       1ull << SDL_SCANCODE_Q |
                                       1ull << SDL_SCANCODE_W |
                                       1ull << SDL_SCANCODE_E |
                                       1ull << SDL_SCANCODE_R |
                                       1ull << SDL_SCANCODE_A |
                                       1ull << SDL_SCANCODE_S |
                                       1ull << SDL_SCANCODE_D |
                                       1ull << SDL_SCANCODE_F |
                                       1ull << SDL_SCANCODE_Z |
                                       1ull << SDL_SCANCODE_X |
                                       1ull << SDL_SCANCODE_C |
                                       1ull << SDL_SCANCODE_V;
uint64_t g_InputBits = 0;

#define GET_INPUT(code) (g_InputBits >> code)
// (!!isDown) is a trick to ensure this value is 0 or 1 and nothing else
#define SET_INPUT(code, isDown) g_InputBits = (g_InputBits & ~(1ull << code)) | ((uint64_t)(!!isDown) << code);
#define IS_VALID_INPUT(code) (g_InputBitMask & (1ull << code))

// Program Functions
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

void CHIPPY_InitVariableRegister()
{
    for (int i = 0; i < 16; i++)
    {
        g_VariableRegs[i] = 0;
    }
}

void CHIPPY_ClearDisplayBuffer()
{
    for (int i = 0; i < CHIPPY_DISPLAY_HEIGHT; ++i)
    {
        for (int j = 0; j < CHIPPY_DISPLAY_WIDTH; ++j)
        g_DisplayBuffer[i][j] = 0;
    }
}

void CHIPPY_SetColorAt(uint32_t* idx, uint8_t r, uint8_t g, uint8_t b)
{
    *idx = 0;
    *idx = (uint32_t)r << 24 | (uint32_t)g << 16 | (uint32_t)b << 8;
}

int CHIPPY_LoadRom()
{
    float secondsPerCycle = 1.0 / CHIPPY_CYCLES_PER_SEC;

    g_ProgramCounter = CHIPPY_STARTING_PROGRAM_COUNTER;

    FILE* rom = fopen(CHIPPY_ROM_PATH, "rb");
    if (!rom)
    {
        fprintf(stderr, "Couldn't read rom at path %s\n", CHIPPY_ROM_PATH);
        fclose(rom);
        return 1;
    }
    
    // Get rom size
    fseek(rom, 0, SEEK_END);
    g_RomSize = ftell(rom);
    rewind(rom);

    // Allocate buffer
    g_RomMemory = malloc(CHIPPY_ROM_MEM_SIZE);
    if (!g_RomMemory) {
        perror("Failed to allocate rom buffer");
        fclose(rom);
        return 1;
    }

    // CHIP-8 expect program mem to start at addr 200 (512 in base-10)
    uint8_t* romMemData = g_RomMemory + g_ProgramCounter;

    // Read file into buffer
    size_t read = fread(romMemData, 1, g_RomSize, rom);
    if (read != g_RomSize) {
        perror("Failed to read entire rom");
        fclose(rom);
        return 1;
    }

    fclose(rom);
};

/** Emulator Functions **/
uint16_t CHIPPY_Fetch()
{
    // Read instruction PC is pointing at from mem - two bytes combined into a 16 bit instruction
    // Increment the PC as we access the bytes, shuffle the PC to the next instruction for next fetch
    return (((uint16_t)g_RomMemory[g_ProgramCounter++] << 8) ^ ((uint16_t)g_RomMemory[g_ProgramCounter++]));
};

// split input into two parts:
// first half_byte = instruction & the rest = data

// if else or switch for triggering instructions? na that's gross
// functions pointers + map/indexing instant access

/*
X: The second nibble. Used to look up one of the 16 registers (VX) from V0 through VF. (always used to look up the values in registers)
Y: The third nibble. Also used to look up one of the 16 registers (VY) from V0 through VF. (always used to look up the values in registers)
N: The fourth nibble. A 4-bit number.
NN: The second byte (third and fourth nibbles). An 8-bit immediate number.
NNN: The second, third and fourth nibbles. A 12-bit immediate memory address.

00E0 (clear screen)
00EE (Subroutine at NNN - pop NNN from stack, set PC to NNN)
0NNN (machine language routine - SKIP)

1NNN (jump - set PC to NNN)

2NNN (Subroutine at NNN - push NNN to stack, set PC to NNN)

3XNN (PC += 2 if (VX == NN))
4XNN (PC += 2 if (VX != NN))
5XY0 (PC += 2 if (VX == VY))
6XNN (set register VX to NN)
7XNN (add value to register VX)
9XY0 (PC += 2 if (VX != VY))
ANNN (set index register I)
DXYN (display/draw)

3XNN
4XNN
5XY0
9XY0

BNNN
CXNN

8XY0
8XY1
8XY2
8XY3
8XY4
8XY5
8XY7
8XY6
8XYE

EX9E
EXA1

FX07
FX15
FX18

FX1E
FX0A
FX29
FX33

FX55
FX65
*/
void CHIPPY_OpClearScreen()
{
    CHIPPY_ClearDisplayBuffer();
};

void CHIPPY_OpPushSubroutine(uint16_t instruction)
{
    Cstack_Push(g_AddressStack, g_ProgramCounter);
    g_ProgramCounter = NNN(instruction);
}

void CHIPPY_OpPopSubroutine()
{
    g_ProgramCounter = Cstack_Pop(g_AddressStack);
};

void CHIPPY_LookUp0(uint16_t instruction)
{
    switch (instruction)
    {
    case 0x00E0:
        CHIPPY_OpClearScreen();
        break;
    case 0x00EE:
        CHIPPY_OpPopSubroutine();
        break;
    default:
        break;
    }
};

void CHIPPY_OpJumpPC(uint16_t instruction)
{
    g_ProgramCounter = NNN(instruction);
};

void CHIPPY_OpSetVX(uint16_t instruction)
{
    g_VariableRegs[X(instruction)] = NN(instruction);
};

void CHIPPY_OpVXAdd(uint16_t instruction)
{
    g_VariableRegs[X(instruction)] += NN(instruction);
};

void CHIPPY_OpSetIdxReg(uint16_t instruction)
{
    g_IndexReg = NNN(instruction);
}; 

void CHIPPY_OpSetPixel(uint16_t instruction)
{
    // This is totally wrong
    SDL_Color color = g_DisplayColors[!!N(instruction)];
    CHIPPY_SetColorAt(g_DisplayBuffer[X(instruction)][Y(instruction)],
        color.r,
        color.g,
        color.b
        );
}; 

CHIPPY_FPtr g_OperationMap[16] =
{
    CHIPPY_LookUp0, // 0XXX Search
    CHIPPY_OpJumpPC, // 1XXX Jump
    CHIPPY_OpPushSubroutine, // 2XXX Subroutine
    NULL, // 3XXX
    NULL, // 4XXX
    NULL, // 5XXX
    CHIPPY_OpSetVX, // 6XXX Set VX to NN
    CHIPPY_OpVXAdd, // 7XXX Add NN to VX
    NULL, // 8XXX
    NULL, // 9XXX
    CHIPPY_OpSetIdxReg, // AXXX Set Index Reg I to NNN
    NULL, // BXXX
    NULL, // CXXX
    CHIPPY_OpSetPixel, // DXXX Set Pixel XY to N
    NULL, // EXXX
    NULL  // FXXX
};


void CHIPPY_Execute(uint16_t op, uint16_t instruction)
{

};

void CHIPPY_Update()
{
    if (g_GamePaused) return;
    
    // Using Fixed Timestep
    g_CycleTimer += SECONDS(g_DeltaTime);
    g_GameTimer += SECONDS(g_DeltaTime);

    // Timers need to be decremented by 1 every second
    const uint8_t timerDecrement = (uint8_t)(floor(g_GameTimer));

    g_DelayTimer = g_DelayTimer - timerDecrement;
    g_SoundTimer = g_SoundTimer - timerDecrement;
    g_GameTimer -= timerDecrement;

    // Ensure no missed instructions
    const int cyclesToRun = (int)(g_CycleTimer / CHIPPY_SEC_PER_CYCLE);
    for (int i = 0; i < cyclesToRun; ++i)
    {
        const uint16_t instruction = CHIPPY_Fetch();
        const uint16_t opcode = (instruction >> 12);
        CHIPPY_Execute(opcode, instruction);
    }

    // Only subtract time used, to ensure no lost time between updates.
    g_CycleTimer -= cyclesToRun * CHIPPY_SEC_PER_CYCLE;
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    g_CurrentTime = SDL_GetTicks();


    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("CHIPPY-08", 800, 600, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MAXIMIZED, &g_Window, &g_Renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    CHIPPY_InitVariableRegister();
    CHIPPY_ClearDisplayBuffer();

    g_AddressStack = malloc(sizeof(Cstack));

    g_DisplayTexture = SDL_CreateTexture(g_Renderer, CHIPPY_DISPLAY_FORMAT, CHIPPY_DISPLAY_TEXTURE_FLAGS, CHIPPY_DISPLAY_WIDTH, CHIPPY_DISPLAY_HEIGHT);
    SDL_SetTextureScaleMode(g_DisplayTexture, CHIPPY_DISPLAY_SCALE_MODE);

    if (CHIPPY_LoadRom() != 0)
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
};

SDL_AppResult handle_key_event_(SDL_Scancode key_code, int IsDown)
{
    // Program Input
    switch (key_code) 
    {
    case SDL_SCANCODE_ESCAPE: /* Quit. */
        return SDL_APP_SUCCESS;
    default:
        break;
    }

    // Game Input
    if (IS_VALID_INPUT(key_code))
    {
        SET_INPUT(key_code, IsDown);
    }

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        return handle_key_event_(event->key.scancode, 1);
    case SDL_EVENT_KEY_UP:
        return handle_key_event_(event->key.scancode, 0);
    }
    return SDL_APP_CONTINUE;
}

void CHIPPY_WelcomeMsg()
{
    const char* message = "CHIPPY-08";
    int w = 0, h = 0;
    float x, y;
    const float scale = 4.0f;

    /* Center the message and scale it up */
    SDL_GetRenderOutputSize(g_Renderer, &w, &h);
    SDL_SetRenderScale(g_Renderer, scale, scale);

    x = ((w / scale) - CHIPPY_TEXT_SIZE * SDL_strlen(message)) / 2;
    y = ((h / scale) - CHIPPY_TEXT_SIZE) / 2;

    /* Draw the message */
    SDL_SetRenderDrawColor(g_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_Renderer);
    SDL_SetRenderDrawColor(g_Renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(g_Renderer, x, y, message);
    SDL_RenderPresent(g_Renderer);
};

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    uint64_t time = SDL_GetTicks();
    g_CurrentTime = time;
    
    // fixed timestep
    while (g_CurrentTime < time + MILLISECONDS(g_TimeStep))
    {
        g_CurrentTime = SDL_GetTicks();
    }

    g_DeltaTime = (g_CurrentTime - time);

    if (SECONDS(g_CurrentTime) < CHIPPY_START_SCREEN_DELAY) 
    {
        CHIPPY_WelcomeMsg();
    }
    else
    {
        CHIPPY_Update();

        // Draw

        // debug colors
        for (int i = 0; i < CHIPPY_DISPLAY_WIDTH; i++)
            g_DisplayBuffer[0][i] = g_CurrentTime * 0xffffffff;

        SDL_UpdateTexture(g_DisplayTexture, NULL, &g_DisplayBuffer, sizeof(g_DisplayBuffer[0][0]) * CHIPPY_DISPLAY_WIDTH);
        
        SDL_SetRenderDrawColor(g_Renderer, g_DisplayColors[0].r, g_DisplayColors[0].g, g_DisplayColors[0].b, g_DisplayColors[0].a);
        SDL_RenderClear(g_Renderer);
        
        SDL_SetRenderDrawColor(g_Renderer, g_DisplayColors[1].r, g_DisplayColors[1].g, g_DisplayColors[1].b, g_DisplayColors[1].a);
        SDL_RenderTexture(g_Renderer, g_DisplayTexture, NULL, NULL);     

        SDL_RenderPresent(g_Renderer);
    }


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    Cstack_Clean(g_AddressStack);
    free(g_AddressStack);
    free(g_RomMemory);
    SDL_DestroyTexture(g_DisplayTexture);
    SDL_DestroyRenderer(g_Renderer);
    SDL_DestroyWindow(g_Window);
}

