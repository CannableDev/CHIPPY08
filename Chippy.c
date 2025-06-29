#include "Chippy.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "cstack.h"

// Rom Display
// RGBA32
uint32_t g_DisplayBuffer[CHIPPY_DISPLAY_HEIGHT][CHIPPY_DISPLAY_WIDTH];
SDL_Texture* g_DisplayTexture = NULL;
const SDL_Color g_DisplayColors[2] =
{
    { 10, 24, 41, 255 }, // off color
    { 93, 232, 165, 255 } // on color
};

// Rom Time
double g_TimeStep = CHIPPY_FIXED_STEP;
uint64_t g_DeltaTime = 0;
uint64_t g_CurrentTime = 0;

bool g_GamePaused = false;
uint8_t g_DelayTimer = 0;
uint8_t g_SoundTimer = 0;
double g_GameTimer = 0;
double g_CycleTimer = 0;

// Rom Memory
uint16_t g_ProgramCounter = CHIPPY_STARTING_PROGRAM_COUNTER;
uint8_t g_VariableRegisters[16];
uint16_t g_IndexRegister = 0;
Cstack* g_AddressStack = NULL;
uint8_t* g_RomMemory = NULL;
size_t g_RomSize = 0;

// Rom Fonts
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

// Rom Inputs

static const uint8_t g_InputHexTable[16] =
{
    SDL_SCANCODE_1, // 0x0
    SDL_SCANCODE_2, // 0x1
    SDL_SCANCODE_3, // 0x2
    SDL_SCANCODE_Q, // 0x3
    SDL_SCANCODE_W, // 0x4
    SDL_SCANCODE_E, // 0x5
    SDL_SCANCODE_A, // 0x6
    SDL_SCANCODE_S, // 0x7
    SDL_SCANCODE_D, // 0x8
    SDL_SCANCODE_X, // 0x9
    SDL_SCANCODE_Z, // 0xA
    SDL_SCANCODE_C, // 0xB
    SDL_SCANCODE_4, // 0xC
    SDL_SCANCODE_R, // 0xD
    SDL_SCANCODE_F, // 0xE
    SDL_SCANCODE_V  // 0xF
};
/*
    CHIP8-Hex            PC-Scancode
    1 2 3 C              1 2 3 4
    4 5 6 D      ->      Q W E R
    7 8 9 E              A S D F
    A 0 B F              Z X C V
*/
// Set size large enough to cover all scancodes used, SCANCODE_4 == 33 
static const uint8_t g_InputScanTable[34] = {
    [SDL_SCANCODE_1] = 0x0,
    [SDL_SCANCODE_2] = 0x1,
    [SDL_SCANCODE_3] = 0x2,
    [SDL_SCANCODE_Q] = 0x3,
    [SDL_SCANCODE_W] = 0x4,
    [SDL_SCANCODE_E] = 0x5,
    [SDL_SCANCODE_A] = 0x6,
    [SDL_SCANCODE_S] = 0x7,
    [SDL_SCANCODE_D] = 0x8,
    [SDL_SCANCODE_X] = 0x9,
    [SDL_SCANCODE_Z] = 0xA,
    [SDL_SCANCODE_C] = 0xB,
    [SDL_SCANCODE_4] = 0xC,
    [SDL_SCANCODE_R] = 0xD,
    [SDL_SCANCODE_F] = 0xE,
    [SDL_SCANCODE_V] = 0xF
    // All other values default to 0
};

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
uint64_t g_InputBitMap = 0;
uint8_t g_LastInput = 0;

void CHIPPY_InitVariableRegister()
{
    for (int i = 0; i < 16; i++)
    {
        g_VariableRegisters[i] = 0;
    }
}

inline uint32_t CHIPPY_SDLColor_To_Uint32(SDL_Color* color)
{
    return (uint32_t)color->r << 24 | (uint32_t)color->g << 16 | (uint32_t)color->b << 8 | (uint32_t)color->a;
}

void CHIPPY_ClearDisplayBuffer()
{
    const uint32_t clearColor = CHIPPY_SDLColor_To_Uint32(&g_DisplayColors[0]);
    for (int i = 0; i < CHIPPY_DISPLAY_HEIGHT; ++i)
    {
        for (int j = 0; j < CHIPPY_DISPLAY_WIDTH; ++j)
            g_DisplayBuffer[i][j] = clearColor;
    }
}

inline uint8_t CHIPPY_CheckColorPresent(uint32_t* pixel)
{
    return (uint8_t)(*pixel ^ CHIPPY_SDLColor_To_Uint32(&g_DisplayColors[1]));
}

inline void CHIPPY_SetColorAt(uint32_t* idx, SDL_Color* color)
{
    *idx = CHIPPY_SDLColor_To_Uint32(color);
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

    memcpy(&g_RomMemory[g_FontStartAddress], &g_Font, sizeof(g_Font));
    return 0;
};

/** Emulator Functions **/
uint16_t CHIPPY_Fetch()
{
    // Read instruction PC is pointing at from mem - two bytes combined into a 16 bit instruction
    // Increment the PC as we access the bytes, shuffle the PC to the next instruction for next fetch
    uint16_t instruction = ((uint16_t)(g_RomMemory[g_ProgramCounter]) << 8) ^ (uint16_t)(g_RomMemory[g_ProgramCounter + 1]);
    g_ProgramCounter += 2;
    return instruction;
};

inline void CHIPPY_Op_ClearScreen()
{
    CHIPPY_ClearDisplayBuffer();
};

void CHIPPY_Op_PushSubroutine(uint16_t instruction)
{
    Cstack_Push(g_AddressStack, g_ProgramCounter);
    g_ProgramCounter = NNN(instruction);
}

inline void CHIPPY_Op_PopSubroutine()
{
    g_ProgramCounter = Cstack_Pop(g_AddressStack);
};

void CHIPPY_LookUp_Op0(uint16_t instruction)
{
    // 0--- Op Codes don't require masking
    switch (instruction)
    {
    case 0x00E0:
        CHIPPY_Op_ClearScreen();
        break;
    case 0x00EE:
        CHIPPY_Op_PopSubroutine();
        break;
    default: // 0NNN (machine language routine - SKIP)
        break;
    }
};

inline void CHIPPY_OpSet_VXVY(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] = g_VariableRegisters[Y(instruction)];
};

inline void CHIPPY_OpBitwiseOr_VXVY(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] |= g_VariableRegisters[Y(instruction)];
};

inline void CHIPPY_OpBitwiseAnd_VXVY(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] &= g_VariableRegisters[Y(instruction)];
};

inline void CHIPPY_OpBitwiseXOR_VXVY(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] &= g_VariableRegisters[Y(instruction)];
};

void CHIPPY_OpCarryAdd_VXVY(uint16_t instruction)
{
    const uint8_t xIdx = X(instruction);
    const uint16_t result = g_VariableRegisters[xIdx] + g_VariableRegisters[Y(instruction)];
    // If the result overflows, set the carry flag in VF to 1, else set 0
    g_VariableRegisters[0xF] = (uint8_t)(result > 0xFF);
    g_VariableRegisters[xIdx] = (uint8_t)result;
};

void CHIPPY_OpSubtract_VXVY(uint16_t instruction)
{
    const uint8_t xIdx = X(instruction);
    const uint8_t x = g_VariableRegisters[xIdx];
    const uint8_t y = g_VariableRegisters[Y(instruction)];
    const uint16_t result = x - y;
    // If a > b, set the carry flag in VF to 1, else set 0
    g_VariableRegisters[0xF] = (uint8_t)(x > y);
    g_VariableRegisters[xIdx] = (uint8_t)result;
};

void CHIPPY_OpSubtract_VYVX(uint16_t instruction)
{
    const uint8_t xIdx = X(instruction);
    const uint8_t x = g_VariableRegisters[xIdx];
    const uint8_t y = g_VariableRegisters[Y(instruction)];
    const uint16_t result = y - x;
    // If a > b, set the carry flag in VF to 1, else set 0
    g_VariableRegisters[0xF] = (uint8_t)(y > x);
    g_VariableRegisters[xIdx] = (uint8_t)result;
};

void CHIPPY_OpShiftRight_VYVX(uint16_t instruction)
{
    const uint8_t xIdx = X(instruction);
    g_VariableRegisters[xIdx] = g_VariableRegisters[Y(instruction)];

    const uint8_t vx = g_VariableRegisters[xIdx];
    // Set carry flag in VF to match LSB
    g_VariableRegisters[0xF] = vx & 1;
    g_VariableRegisters[xIdx] >>= 1;
};

void CHIPPY_OpShiftLeft_VYVX(uint16_t instruction)
{
    const uint8_t xIdx = X(instruction);
    g_VariableRegisters[xIdx] = g_VariableRegisters[Y(instruction)];

    const uint8_t vx = g_VariableRegisters[xIdx];
    // Set carry flag in VF to match LSB
    g_VariableRegisters[0xF] = vx & 1;
    g_VariableRegisters[xIdx] <<= 1;
};

void CHIPPY_LookUp_Op8(uint16_t instruction)
{
    const uint16_t opCode = instruction & 0xF00F;

    switch (opCode)
    {
    case 0x8000:
        CHIPPY_OpSet_VXVY(instruction);
        break;
    case 0x8001:
        CHIPPY_OpBitwiseOr_VXVY(instruction);
        break;
    case 0x8002:
        CHIPPY_OpBitwiseAnd_VXVY(instruction);
        break;
    case 0x8003:
        CHIPPY_OpBitwiseXOR_VXVY(instruction);
        break;
    case 0x8004:
        CHIPPY_OpCarryAdd_VXVY(instruction);
        break;
    case 0x8005:
        CHIPPY_OpSubtract_VXVY(instruction);
        break;
    case 0x8007:
        CHIPPY_OpSubtract_VYVX(instruction);
        break;
    case 0x8006:
        CHIPPY_OpShiftRight_VYVX(instruction);
        break;
    case 0x800E:
        CHIPPY_OpShiftLeft_VYVX(instruction);
        break;
    default:
        break;
    }
};

inline void CHIPPY_OpSkip_KeyVXDown(uint16_t instruction)
{
    g_ProgramCounter += 2 * GET_INPUT_FROM_HEX(g_VariableRegisters[X(instruction)]);
};

inline void CHIPPY_OpSkip_KeyVXUp(uint16_t instruction)
{
    g_ProgramCounter += 2 * !GET_INPUT_FROM_HEX(g_VariableRegisters[X(instruction)]);
};

void CHIPPY_LookUp_OpE(uint16_t instruction)
{
    const uint16_t opCode = instruction & 0xF0FF;

    switch (opCode)
    {
    case 0xE09E:
        CHIPPY_OpSkip_KeyVXDown(instruction);
        break;
    case 0xE0A1:
        CHIPPY_OpSkip_KeyVXUp(instruction);
        break;
    default:
        break;
    }
};

inline void CHIPPY_OpTimer_CacheDelayVX(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] = g_DelayTimer;
};

inline void CHIPPY_OpTimer_SetDelayVX(uint16_t instruction)
{
    g_DelayTimer = g_VariableRegisters[X(instruction)];
};

inline void CHIPPY_OpTimer_SetSoundVX(uint16_t instruction)
{
    g_SoundTimer = g_VariableRegisters[X(instruction)];
};

inline void CHIPPY_OpAdd_IdxReg(uint16_t instruction)
{
    g_IndexRegister += g_VariableRegisters[X(instruction)];
};

void CHIPPY_OpInput_GetKey(uint16_t instruction)
{
    // This opcode blocks until a key is pressed, but as we already incremented the program counter in the Fetch step-
    // we decrement it here first to cause a loop
    g_ProgramCounter -= 2;

    if (GET_ANY_INPUT_DOWN)
    {
        g_ProgramCounter += 2;
        g_VariableRegisters[X(instruction)] = g_InputScanTable[g_LastInput];
    }
};

void CHIPPY_OpFont_SetCharacter(uint16_t instruction)
{
    const uint8_t fontIndex = g_VariableRegisters[X(instruction)] * g_FontHeight;
    g_IndexRegister = g_RomMemory[g_FontStartAddress + fontIndex];
};

void CHIPPY_OpFont_VXToDecimal(uint16_t instruction)
{
    // Takes the number in vx and converts it to three decimal digits and stores them in the index register memory
    uint8_t input = g_VariableRegisters[X(instruction)];
    uint8_t memoryIdx = g_IndexRegister;

    while (input > 0)
    {
        g_RomMemory[memoryIdx] = input % 10;

        ++memoryIdx;
        input *= 0.1;
    }
};

void CHIPPY_OpMemory_Store(uint16_t instruction)
{
    for (int i = 0; i <= X(instruction); ++i)
    {
        g_RomMemory[g_IndexRegister + i] = g_VariableRegisters[i];
    }
};

void CHIPPY_OpMemory_Load(uint16_t instruction)
{
    for (int i = 0; i <= X(instruction); ++i)
    {
        g_VariableRegisters[i] = g_RomMemory[g_IndexRegister + i];
    }
};

void CHIPPY_LookUp_OpF(uint16_t instruction)
{
    const uint16_t opCode = instruction & 0xF0FF;

    switch (opCode)
    {
    case 0xF007:
        CHIPPY_OpTimer_CacheDelayVX(instruction);
        break;
    case 0xF015:
        CHIPPY_OpTimer_SetDelayVX(instruction);
        break;
    case 0xF018:
        CHIPPY_OpTimer_SetDelayVX(instruction);
        break;
    case 0xF01E:
        CHIPPY_OpAdd_IdxReg(instruction);
        break;
    case 0xF00A:
        CHIPPY_OpInput_GetKey(instruction);
        break;
    case 0xF029:
        CHIPPY_OpFont_SetCharacter(instruction);
        break;
    case 0xF033:
        CHIPPY_OpFont_VXToDecimal(instruction);
        break;
    case 0xF055:
        CHIPPY_OpMemory_Store(instruction);
        break;
    case 0xF065:
        CHIPPY_OpMemory_Load(instruction);
        break;
    default:
        break;
    }
};

inline void CHIPPY_OpJump_PC(uint16_t instruction)
{
    g_ProgramCounter = NNN(instruction);
};

inline void CHIPPY_OpJump_V0PC(uint16_t instruction)
{
    g_ProgramCounter = NNN(instruction) + g_VariableRegisters[0];
};

inline void CHIPPY_OpIf_VXNN(uint16_t instruction)
{
    // if VX == NN, PC += 2
    g_ProgramCounter += (uint8_t)(g_VariableRegisters[X(instruction)] == NN(instruction)) * 2;
};

inline void CHIPPY_OpIfNot_VXNN(uint16_t instruction)
{
    // if VX == NN, PC += 2
    g_ProgramCounter += (uint8_t)(g_VariableRegisters[X(instruction)] != NN(instruction)) * 2;
};

inline void CHIPPY_OpIf_VXVY(uint16_t instruction)
{
    // if VX == VY, PC += 2
    g_ProgramCounter += (uint8_t)(g_VariableRegisters[X(instruction)] == g_VariableRegisters[Y(instruction)]) * 2;
};

inline void CHIPPY_OpIfNot_VXVY(uint16_t instruction)
{
    // if VX != VY, PC += 2
    g_ProgramCounter += (uint8_t)(g_VariableRegisters[X(instruction)] != g_VariableRegisters[Y(instruction)]) * 2;
};

inline void CHIPPY_OpSet_VX(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] = NN(instruction);
};

inline void CHIPPY_OpAdd_VX(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] += NN(instruction);
};

inline void CHIPPY_OpSet_IdxReg(uint16_t instruction)
{
    g_IndexRegister = NNN(instruction);
};

inline void CHIPPY_OpRand_VX(uint16_t instruction)
{
    g_VariableRegisters[X(instruction)] = rand() & NN(instruction);
};

void CHIPPY_Op_DrawSprite(uint16_t instruction)
{
    const uint8_t x = g_VariableRegisters[X(instruction)] % CHIPPY_DISPLAY_WIDTH;
    const uint8_t y = g_VariableRegisters[Y(instruction)] % CHIPPY_DISPLAY_HEIGHT;
    uint8_t numPixelsH = N(instruction);

    // Prevent drawing out of buffer range
    numPixelsH = y + numPixelsH > CHIPPY_DISPLAY_HEIGHT - 1 ?
        CHIPPY_DISPLAY_HEIGHT - y - 1 : numPixelsH;
    uint8_t numPixelsW = x + 8 > CHIPPY_DISPLAY_WIDTH - 1 ?
        CHIPPY_DISPLAY_WIDTH - x - 1 : 8;

    for (uint8_t row = 0; row < numPixelsH; ++row)
    {
        for (uint8_t col = 0; col < numPixelsW; ++col)
        {
            // get pixel from msb -> lsb 
            const uint8_t spritePixelOn = !!(g_RomMemory[g_IndexRegister + row] & (0x80 >> col));
            // get pixel to draw to from color buffer
            uint32_t* bufferPixelPtr = &g_DisplayBuffer[y + row][x + col];
            const uint8_t bufferPixelOn = CHIPPY_CheckColorPresent(bufferPixelPtr);

            // if sprite and pixel are on, negated 
            const uint8_t bPixelNegation = (uint8_t)(spritePixelOn & bufferPixelOn);

            g_VariableRegisters[0xF] = bPixelNegation;

            CHIPPY_SetColorAt(bufferPixelPtr, &g_DisplayColors[bufferPixelOn ^ spritePixelOn]);
        }
    }
};

/*
X: The second nibble. Used to look up one of the 16 registers (VX) from V0 through VF. (always used to look up the values in registers)
Y: The third nibble. Also used to look up one of the 16 registers (VY) from V0 through VF. (always used to look up the values in registers)
N: The fourth nibble. A 4-bit number.
NN: The second byte (third and fourth nibbles). An 8-bit immediate number.
NNN: The second, third and fourth nibbles. A 12-bit immediate memory address.*/
CHIPPY_FPtr g_OperationMap[16] =
{
    CHIPPY_LookUp_Op0, // 0--- Multiple Instructions
    CHIPPY_OpJump_PC, // 1--- Jump PC to NNN
    CHIPPY_Op_PushSubroutine, // 2--- Subroutine
    CHIPPY_OpIf_VXNN, // 3--- if (vx == nn) skip
    CHIPPY_OpIfNot_VXNN, // 4--- if (vx != nn) skip
    CHIPPY_OpIf_VXVY, // 5--- if (vx == vy) skip
    CHIPPY_OpSet_VX, // 6--- Set VX to NN
    CHIPPY_OpAdd_VX, // 7--- Add NN to VX
    CHIPPY_LookUp_Op8, // 8--- Multiple Instructions
    CHIPPY_OpIfNot_VXVY, // 9--- if (vx != vy) skip
    CHIPPY_OpSet_IdxReg, // A--- Set Index Reg I to NNN
    CHIPPY_OpJump_V0PC, // B--- Jump PC to NNN + V0
    CHIPPY_OpRand_VX, // C--- Generate Random int into VX
    CHIPPY_Op_DrawSprite, // D--- Draw sprite at (VX, VY) by N*8
    CHIPPY_LookUp_OpE, // E--- Multiple Instructions
    CHIPPY_LookUp_OpF  // F--- Multiple Instructions
};


inline void CHIPPY_Execute(uint16_t instruction)
{
    (*g_OperationMap[OP(instruction)])(instruction);
};

uint32_t* CHIPPY_GetDisplayBuffer()
{
    return &g_DisplayBuffer[0][0];
};

SDL_Texture* CHIPPY_GetDisplayTexture()
{
    return g_DisplayTexture;
};

void CHIPPY_Update()
{
    if (g_GamePaused) return;

    // Using Fixed Timestep
    g_CycleTimer += SECONDS(g_DeltaTime);
    g_GameTimer += SECONDS(g_DeltaTime);

    // Timers need to be decremented by 1 every second
    const uint8_t timerDecrement = (uint8_t)(floor(g_GameTimer));

    g_DelayTimer = g_DelayTimer > 0 ? g_DelayTimer - timerDecrement : g_DelayTimer;
    g_SoundTimer = g_SoundTimer > 0 ? g_SoundTimer - timerDecrement : g_SoundTimer;
    g_GameTimer -= timerDecrement;

    // Ensure no missed instructions
    const int cyclesToRun = (int)(g_CycleTimer / CHIPPY_SEC_PER_CYCLE);
    for (int i = 0; i < cyclesToRun; ++i)
    {
        const uint16_t instruction = CHIPPY_Fetch();
        CHIPPY_Execute(instruction);
    }

    // Only subtract time used, to ensure no lost time between updates.
    g_CycleTimer -= cyclesToRun * CHIPPY_SEC_PER_CYCLE;
};

SDL_AppResult CHIPPY_InputEvent(SDL_Scancode key_code, int IsDown)
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
        g_LastInput = IsDown ? key_code : 0;
        SET_INPUT(key_code, IsDown);
    }

    return SDL_APP_CONTINUE;
}

void CHIPPY_WelcomeMsg(SDL_Renderer* renderer)
{
    const char* message = CHIPPY_WELCOME_MSG;
    int w = 0, h = 0;
    float x, y;
    const float scale = 4.0f;

    /* Center the message and scale it up */
    SDL_GetRenderOutputSize(renderer, &w, &h);
    SDL_SetRenderScale(renderer, scale, scale);

    x = ((w / scale) - CHIPPY_START_SCREEN_TEXT_SIZE * SDL_strlen(message)) / 2;
    y = ((h / scale) - CHIPPY_START_SCREEN_TEXT_SIZE) / 2;

    /* Draw the message */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, message);
    SDL_RenderPresent(renderer);
};

void CHIPPY_Shutdown()
{
    Cstack_Clean(g_AddressStack);
    free(g_AddressStack);
    free(g_RomMemory);
    SDL_DestroyTexture(g_DisplayTexture);
};

SDL_AppResult CHIPPY_Init(SDL_Renderer* renderer) {
    g_CurrentTime = SDL_GetTicks();
    srand(time(NULL));

    CHIPPY_InitVariableRegister();
    CHIPPY_ClearDisplayBuffer();

    g_AddressStack = Cstack_Init();

    g_DisplayTexture = SDL_CreateTexture(renderer, CHIPPY_DISPLAY_FORMAT, CHIPPY_DISPLAY_TEXTURE_FLAGS, CHIPPY_DISPLAY_WIDTH, CHIPPY_DISPLAY_HEIGHT);
    SDL_SetTextureScaleMode(g_DisplayTexture, CHIPPY_DISPLAY_SCALE_MODE);

    if (CHIPPY_LoadRom() != 0)
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}