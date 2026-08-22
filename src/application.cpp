#include "application.h"
#include "color_scheme.h"
#include "png_writer.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <ctime>
#include <iostream>
Application::Application(int cssWidth, int cssHeight):window(nullptr),renderer(nullptr),texture(nullptr),viewport(cssWidth, cssHeight),simulation(cssWidth*RES, cssHeight*RES),schemes{nullptr, nullptr, nullptr},activeScheme(0),clicker(false),running(false),fullscreen(true),startTicks(0),windowWidth(cssWidth),windowHeight(cssHeight),dstX(0.0f),dstY(0.0f),dstW(0.0f),dstH(0.0f),scale(1.0f){
    schemes[0]=new GrayscaleScheme();
    schemes[1]=new ThermalScheme();
    schemes[2]=new AlphaScheme();
}
Application::~Application(){
    for(int i=0; i<3; i++){
        delete schemes[i];
    }
    if(texture!=nullptr){
        SDL_DestroyTexture(texture);
    }
    if(renderer!=nullptr){
        SDL_DestroyRenderer(renderer);
    }
    if(window!=nullptr){
        SDL_DestroyWindow(window);
    }
}
bool Application::initialize(){
    window=SDL_CreateWindow("Mandelbrot Emerger", windowWidth, windowHeight, SDL_WINDOW_FULLSCREEN|SDL_WINDOW_BORDERLESS);
    if(window==nullptr){
        std::cerr<<"SDL_CreateWindow failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    renderer=SDL_CreateRenderer(window, nullptr);
    if(renderer==nullptr){
        std::cerr<<"SDL_CreateRenderer failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);
    texture=SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, simulation.getBuffer().getWidth(), simulation.getBuffer().getHeight());
    if(texture==nullptr){
        std::cerr<<"SDL_CreateTexture failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    return true;
}
void Application::run(){
    running=true;
    startTicks=SDL_GetTicks();
    simulation.reframe(viewport);
    viewport.log(std::cout);
    while(running){
        processEvents();
        render();
    }
}
void Application::processEvents(){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_EVENT_QUIT:
                running=false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                onMouseButtonDown(event);
                break;
            case SDL_EVENT_KEY_DOWN:
                onKeyDown(event);
                break;
            default:
                break;
        }
    }
}
void Application::render(){
    computeDestinationRect();
    // the fade clock runs at a virtual 60Hz like p5's frameRate(60) so escape brightness keeps its browser wall-clock timeline while iteration stays uncapped
    long long frameIndex=static_cast<long long>((SDL_GetTicks()-startTicks))*60LL/1000LL;
    if(frameIndex<1){
        frameIndex=1;
    }
    simulation.step(frameIndex, schemes[activeScheme]);
    SDL_UpdateTexture(texture, nullptr, simulation.getBuffer().data(), simulation.getBuffer().getWidth()*4);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_FRect destination={dstX, dstY, dstW, dstH};
    SDL_RenderTexture(renderer, texture, nullptr, &destination);
    SDL_RenderPresent(renderer);
}
void Application::onMouseButtonDown(const SDL_Event& event){
    float mouseX=event.button.x;
    float mouseY=event.button.y;
    double deviceX=static_cast<double>((mouseX-dstX)/scale);
    double deviceY=static_cast<double>((mouseY-dstY)/scale);
    if(!clicker){
        viewport.beginZoom(deviceX, deviceY);
        clicker=true;
    }
    else{
        viewport.completeZoom(deviceX, deviceY);
        clicker=false;
        simulation.reframe(viewport);
        viewport.log(std::cout);
    }
}
void Application::onKeyDown(const SDL_Event& event){
    switch(event.key.key){
        case SDLK_RETURN:
            saveScreenshot();
            break;
        case SDLK_C:
            cycleColorScheme();
            break;
        case SDLK_F11:
            toggleFullscreen();
            break;
        case SDLK_ESCAPE:
            running=false;
            break;
        default:
            break;
    }
}
void Application::cycleColorScheme(){
    activeScheme=(activeScheme+1)%3;
}
void Application::toggleFullscreen(){
    fullscreen=!fullscreen;
    SDL_SetWindowFullscreen(window, fullscreen);
    if(!fullscreen){
        SDL_SetWindowSize(window, 1280, 720);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}
void Application::saveScreenshot(){
    std::time_t now=std::time(nullptr);
    std::tm localTime;
#ifdef _MSC_VER
    localtime_s(&localTime, &now);
#else
    localTime=*std::localtime(&now);
#endif
    char name[64];
    std::strftime(name, sizeof(name), "mandelbrot_%Y%m%d_%H%M%S.png", &localTime);
    PngWriter writer;
    if(writer.save(simulation.getBuffer(), name)){
        std::cout<<"Saved "<<name<<std::endl;
    }
    else{
        std::cerr<<"Failed to save "<<name<<std::endl;
    }
}
void Application::computeDestinationRect(){
    int windowW=0;
    int windowH=0;
    SDL_GetWindowSize(window, &windowW, &windowH);
    float bufferW=static_cast<float>(simulation.getBuffer().getWidth());
    float bufferH=static_cast<float>(simulation.getBuffer().getHeight());
    scale=(static_cast<float>(windowW)/bufferW<static_cast<float>(windowH)/bufferH)?static_cast<float>(windowW)/bufferW:static_cast<float>(windowH)/bufferH;
    dstW=bufferW*scale;
    dstH=bufferH*scale;
    dstX=(static_cast<float>(windowW)-dstW)*0.5f;
    dstY=(static_cast<float>(windowH)-dstH)*0.5f;
}
