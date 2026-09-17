#include <SDL3/SDL.h>
#include <iostream>
#include <new>
#include "application.h"
int main(int argc, char** argv){
    (void)argc;
    (void)argv;
    if(!SDL_Init(SDL_INIT_VIDEO)){
        std::cerr<<"SDL_Init failed: "<<SDL_GetError()<<std::endl;
        return 1;
    }
    SDL_Rect bounds;
    if(!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds)){
        std::cerr<<"SDL_GetDisplayBounds failed: "<<SDL_GetError()<<std::endl;
        SDL_Quit();
        return 1;
    }
    try{
        Application application(bounds.w, bounds.h);
        if(!application.initialize()){
            SDL_Quit();
            return 1;
        }
        application.run();
    }
    catch(const std::bad_alloc&){
        std::cerr<<"Not enough memory for the "<<bounds.w<<"x"<<bounds.h<<" display buffer"<<std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Quit();
    return 0;
}
