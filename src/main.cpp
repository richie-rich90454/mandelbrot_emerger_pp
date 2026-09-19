#include <SDL3/SDL.h>
#include <iostream>
#include <new>
#include "application.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
int main(int argc, char** argv){
    (void)argc;
    (void)argv;
    if(!SDL_Init(SDL_INIT_VIDEO)){
        std::cerr<<"SDL_Init failed: "<<SDL_GetError()<<std::endl;
        return 1;
    }
    SDL_Rect bounds;
#ifdef __EMSCRIPTEN__
    // the canvas backing store is the page viewport, so pointer coordinates, the renderer and the letterbox rect share one space
    bounds.w=EM_ASM_INT({ return Math.round(window.innerWidth); });
    bounds.h=EM_ASM_INT({ return Math.round(window.innerHeight); });
    if(bounds.w<=0 || bounds.h<=0){
        bounds.w=1280;
        bounds.h=720;
    }
#else
    if(!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds)){
        std::cerr<<"SDL_GetDisplayBounds failed: "<<SDL_GetError()<<std::endl;
        SDL_Quit();
        return 1;
    }
#endif
    try{
#ifdef __EMSCRIPTEN__
        // the web main loop never returns, and emscripten unwinds the stack when it starts, so the application must live on the heap
        Application* application=new Application(bounds.w, bounds.h);
#else
        Application stackApplication(bounds.w, bounds.h);
        Application* application=&stackApplication;
#endif
        if(!application->initialize()){
            SDL_Quit();
            return 1;
        }
        application->run();
    }
    catch(const std::bad_alloc&){
        std::cerr<<"Not enough memory for the "<<bounds.w<<"x"<<bounds.h<<" display buffer"<<std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Quit();
    return 0;
}
