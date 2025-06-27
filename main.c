#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

#include <stdio.h>
#include <stdlib.h>

#define CHIPPY_ROM_PATH "roms\\1-ibm-logo.ch8"
#define CHIPPY_CYCLES_PER_SEC 700

#define CHIPPY_TEXT_SIZE 8
#define CHIPPY_FIXED_STEP 1 / 60.0

#define CHIPPY_ROM_MEM_SIZE 4096
#define CHIPPY_STARTING_PROGRAM_COUNTER 0x200

#define In_Seconds(t) (t * 0.001)

uint64_t g_TimeStep = CHIPPY_FIXED_STEP;
uint64_t g_DeltaTime = 0;
uint64_t g_CurrentTime = 0;

static uint16_t g_ProgramCounter = CHIPPY_STARTING_PROGRAM_COUNTER;
static uint16_t g_IndexReg = 0;
static uint8_t* g_RomMemory = 0;
static uint8_t g_RomSize = 0;
static uint8_t g_DelayTimer = 0;
static uint8_t g_SoundTimer = 0;

static const uint16_t g_Font[16][5] =
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
static const uint32_t g_FontStartAddress = 0x50;
static const uint32_t g_FontHeight = 5;

static const uint64_t g_InputBitMask = 1ull << SDL_SCANCODE_1 |
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
static uint64_t g_InputBits = 0;

#define GET_INPUT(code) (g_InputBits >> code)
// (!!isDown) is a trick to ensure this value is 0 or 1 and nothing else
#define SET_INPUT(code, isDown) g_InputBits = (g_InputBits & ~(1ull << code)) | ((uint64_t)(!!isDown) << code);
#define IS_VALID_INPUT(code) g_InputBitMask & (1ull << code)

static int LoadRom()
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
// cap at ~700 instructions per sec probably
static void Fetch()
{
    // Read instruction PC is pointing at from mem
    // two bytes combined into a 16 bit instruction
    // Increment the PC by two after to recieve next OpCode
    // (can be done in execute but probably don't)
    // 
};

static void DecodeAndExecute()
{
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
    */

    /*
    00E0 (clear screen)
    1NNN (jump)
    6XNN (set register VX)
    7XNN (add value to register VX)
    ANNN (set index register I)
    DXYN (display/draw)
    */
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    g_CurrentTime = SDL_GetTicks();

    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("CHIPPY-08", 800, 600, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MAXIMIZED, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (LoadRom() != 0)
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
};

static SDL_AppResult handle_key_event_(SDL_Scancode key_code, int IsDown)
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
    SDL_GetRenderOutputSize(renderer, &w, &h);
    SDL_SetRenderScale(renderer, scale, scale);

    x = ((w / scale) - CHIPPY_TEXT_SIZE * SDL_strlen(message)) / 2;
    y = ((h / scale) - CHIPPY_TEXT_SIZE) / 2;

    /* Draw the message */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, message);
    SDL_RenderPresent(renderer);
};

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    uint64_t time = SDL_GetTicks();
    g_CurrentTime = time;
    g_DeltaTime = (time - g_CurrentTime);
    
    // fixed timestep
    while (g_CurrentTime > time + g_TimeStep)
    {
        g_CurrentTime = SDL_GetTicks();
        g_DeltaTime = (time - g_CurrentTime);
    }

    if (In_Seconds(g_CurrentTime) < 4.0f) 
    {
        CHIPPY_WelcomeMsg();
    }
    else
    {


        
        
        
        
        
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }


    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    free(g_RomMemory);
}

