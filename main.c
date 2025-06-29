#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_Window *g_Window = NULL;
SDL_Renderer *g_Renderer = NULL;

#include "Chippy.h"

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer(CHIPPY_WINDOW_NAME, CHIPPY_WINDOW_WIDTH, CHIPPY_WINDOW_HEIGHT, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MAXIMIZED, &g_Window, &g_Renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return CHIPPY_Init(g_Renderer);
};

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        return CHIPPY_InputEvent(event->key.scancode, 1);
    case SDL_EVENT_KEY_UP:
        return CHIPPY_InputEvent(event->key.scancode, 0);
    }
    return SDL_APP_CONTINUE;
}

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
        CHIPPY_WelcomeMsg(g_Renderer);
    }
    else
    {
        CHIPPY_Update();

        SDL_UpdateTexture(CHIPPY_GetDisplayTexture(), NULL, CHIPPY_GetDisplayBuffer(), sizeof(uint32_t) * CHIPPY_DISPLAY_WIDTH);
        
        SDL_SetRenderDrawColor(g_Renderer, g_DisplayColors[0].r, g_DisplayColors[0].g, g_DisplayColors[0].b, g_DisplayColors[0].a);
        SDL_RenderClear(g_Renderer);
        
        SDL_SetRenderDrawColor(g_Renderer, g_DisplayColors[1].r, g_DisplayColors[1].g, g_DisplayColors[1].b, g_DisplayColors[1].a);
        SDL_RenderTexture(g_Renderer, CHIPPY_GetDisplayTexture(), NULL, NULL);

        SDL_RenderPresent(g_Renderer);
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    CHIPPY_Shutdown();
    SDL_DestroyRenderer(g_Renderer);
    SDL_DestroyWindow(g_Window);
}

