
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_main.h>

#include "math.h"
#include "engine_types.h"
#include "zray.h"
#include "tracer.h"

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
uint64_t t1 = 0;
SDL_Texture* g_pixels = 0;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow("zray", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	g_pixels = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    t1 = SDL_GetTicks();

    init_world();

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}


void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    deinit_world();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    /* SDL will clean up the window/renderer for us. */
}



SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            handle_key_event_down(event->key.scancode);
            break;
        case SDL_EVENT_KEY_UP:
            handle_key_event_up(event->key.scancode);
            break;
    }
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppIterate(void* appstate)
{
    uint64_t t2 = SDL_GetTicks();
    float delta = static_cast<float>(t2 - t1);

    void* data;
    int pitch;
    if (delta > SCREEN_TICKS_PER_FRAME) {
        bool ok = SDL_LockTexture(g_pixels, NULL, &data, &pitch);
        {
            // We paint every pixel so we don't need to blank the buffer.
            //SDL_memset4(data, 0, SCREEN_WIDTH * SCREEN_HEIGHT);
            main_loop((Colour<BYTE>*)data);
            t1 = t2;

        }
        SDL_UnlockTexture(g_pixels);

        // copy to window
        SDL_RenderTexture(renderer, g_pixels, NULL, NULL);

        render_text_overlay(renderer);

        SDL_RenderPresent(renderer);
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}



