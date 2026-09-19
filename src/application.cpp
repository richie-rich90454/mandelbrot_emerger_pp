#include "application.h"
#include "color_scheme.h"
#include "png_writer.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iostream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
namespace{
    const unsigned long long AUTO_ZOOM_INTERVAL_MS=6000ull;
    const int PROBE_SAMPLES=256;
    const int PROBE_MAX_ITER=512;
    const int PROBE_MIN_TARGET_ITER=16;
    const double AUTO_ZOOM_DIVISOR=3.0;
    const double AUTO_ZOOM_MIN_SPAN=1e-11;
    const double ZOOM_OUT_FACTOR=2.0;
    const unsigned long long MINIMIZED_POLL_MS=50ull;
    const unsigned long long ANIMATION_MS=2500ull;
    const double CROSSFADE_START=0.75;
}
Application::Application(int cssWidth, int cssHeight):window(nullptr),renderer(nullptr),texture(nullptr),flightTexture(nullptr),viewport(cssWidth, cssHeight),bufferWidth(cssWidth*RES),bufferHeight(cssHeight*RES),simulation(bufferWidth, bufferHeight),schemes{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},activeScheme(0),clicker(false),running(false),fullscreen(true),autoZoomEnabled(true),animating(false),flightCapturePending(false),screenshotRequested(false),animationMode(0),pendingApplied(false),lastReframeTicks(0),animationStartTicks(0),animFrom(ViewportBounds{0.0, 0.0, 0.0, 0.0}),pendingTarget(ViewportBounds{0.0, 0.0, 0.0, 0.0}),randomEngine(std::random_device{}()),windowWidth(cssWidth),windowHeight(cssHeight),dstX(0.0f),dstY(0.0f),dstW(0.0f),dstH(0.0f),scale(1.0f){
    schemes[0]=new GrayscaleScheme();
    schemes[1]=new ThermalScheme();
    schemes[2]=new AlphaScheme();
    schemes[3]=new RainbowScheme();
    schemes[4]=new FireScheme();
    schemes[5]=new IceScheme();
}
const int Application::SCHEME_COUNT;
Application::~Application(){
    for(int i=0; i<SCHEME_COUNT; i++){
        delete schemes[i];
    }
    if(texture!=nullptr){
        SDL_DestroyTexture(texture);
    }
    if(flightTexture!=nullptr){
        SDL_DestroyTexture(flightTexture);
    }
    if(renderer!=nullptr){
        SDL_DestroyRenderer(renderer);
    }
    if(window!=nullptr){
        SDL_DestroyWindow(window);
    }
}
bool Application::initialize(){
#ifdef __EMSCRIPTEN__
    const SDL_WindowFlags windowFlags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    const SDL_WindowFlags windowFlags=SDL_WINDOW_FULLSCREEN|SDL_WINDOW_BORDERLESS;
    // direct3d11 allocates a full-frame staging texture on every lock and opengl's upload spikes past the frame budget; direct3d12 keeps the desktop at a stable 60fps
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d12");
#endif
    window=SDL_CreateWindow("Mandelbrot Emerger", windowWidth, windowHeight, windowFlags);
    if(window==nullptr){
        std::cerr<<"SDL_CreateWindow failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
#ifdef __EMSCRIPTEN__
    renderer=SDL_CreateRenderer(window, nullptr);
#else
    renderer=SDL_CreateRenderer(window, nullptr);
    if(renderer==nullptr){
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        renderer=SDL_CreateRenderer(window, nullptr);
    }
    if(renderer==nullptr){
        SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
        renderer=SDL_CreateRenderer(window, nullptr);
    }
#endif
    if(renderer==nullptr){
        std::cerr<<"SDL_CreateRenderer failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);
    fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
    texture=SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, bufferWidth, bufferHeight);
    if(texture==nullptr){
        std::cerr<<"SDL_CreateTexture failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    flightTexture=SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, bufferWidth, bufferHeight);
    if(flightTexture==nullptr){
        std::cerr<<"SDL_CreateTexture(flight) failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    return true;
}
void Application::run(){
    running=true;
    lastReframeTicks=SDL_GetTicks();
    simulation.reframe(viewport);
    viewport.log(std::cout);
    std::cout<<"[auto] engaged (press A to toggle, click to take manual control)"<<std::endl;
#ifdef __EMSCRIPTEN__
    // one tick per animation frame; the loop ends through emscripten_cancel_main_loop in tick()
    emscripten_set_main_loop_arg([](void* self){ static_cast<Application*>(self)->tick(); }, this, 0, 1);
#else
    while(running){
        tick();
    }
#endif
}
void Application::tick(){
    processEvents();
#ifdef __EMSCRIPTEN__
    if(!running){
        emscripten_cancel_main_loop();
        return;
    }
#endif
    render();
    maybeAutoZoom();
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
    if((SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)!=0){
        if(flightCapturePending){
            void* lockedPixels=nullptr;
            int lockedPitch=0;
            if(SDL_LockTexture(texture, nullptr, &lockedPixels, &lockedPitch)){
                SDL_UpdateTexture(flightTexture, nullptr, lockedPixels, lockedPitch);
                flightCapturePending=false;
            }
        }
        SDL_Delay(MINIMIZED_POLL_MS);
        return;
    }
    // the simulation writes straight into the locked streaming texture, so a batch never costs an extra frame copy
    void* lockedPixels=nullptr;
    int lockedPitch=0;
    if(SDL_LockTexture(texture, nullptr, &lockedPixels, &lockedPitch)){
        unsigned char* pixels=static_cast<unsigned char*>(lockedPixels);
        if(flightCapturePending){
            SDL_UpdateTexture(flightTexture, nullptr, pixels, lockedPitch);
            flightCapturePending=false;
        }
        const int passes=1;
        simulation.step(passes, schemes[activeScheme], pixels, lockedPitch);
        if(screenshotRequested){
            captureScreenshot(pixels, lockedPitch);
            screenshotRequested=false;
        }
        SDL_UnlockTexture(texture);
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_FRect destination={dstX, dstY, dstW, dstH};
    if(animating && animationMode==1){
        drawDive(destination);
    }
    else if(animating && animationMode==2){
        drawFade(destination);
    }
    else{
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
    }
    SDL_RenderPresent(renderer);
    if(!screenshotPixels.empty()){
        saveScreenshot();
    }
}
bool Application::contains(const ViewportBounds& outer, const ViewportBounds& inner){
    return inner.xi>=outer.xi && inner.xf<=outer.xf && inner.yi>=outer.yi && inner.yf<=outer.yf;
}
bool Application::validBounds(const ViewportBounds& bounds){
    return std::isfinite(bounds.xi) && std::isfinite(bounds.xf) && std::isfinite(bounds.yi) && std::isfinite(bounds.yf) && bounds.xf>bounds.xi && bounds.yf>bounds.yi;
}
void Application::onMouseButtonDown(const SDL_Event& event){
    if(event.button.button!=SDL_BUTTON_LEFT){
        return;
    }
    SDL_Event renderEvent=event;
    SDL_ConvertEventToRenderCoordinates(renderer, &renderEvent);
    finishAnimation();
    if(autoZoomEnabled){
        autoZoomEnabled=false;
        std::cout<<"[auto] disengaged (manual control)"<<std::endl;
    }
    float mouseX=renderEvent.button.x;
    float mouseY=renderEvent.button.y;
    double deviceX=static_cast<double>((mouseX-dstX)/scale);
    double deviceY=static_cast<double>((mouseY-dstY)/scale);
    if(!clicker){
        viewport.beginZoom(deviceX, deviceY);
        clicker=true;
    }
    else{
        clicker=false;
        beginTransition(viewport.completeZoom(deviceX, deviceY));
    }
}
void Application::onKeyDown(const SDL_Event& event){
    if(event.key.repeat){
        return;
    }
    switch(event.key.key){
        case SDLK_RETURN:
            screenshotRequested=true;
            break;
        case SDLK_C:
            cycleColorScheme();
            break;
        case SDLK_F11:
            toggleFullscreen();
            break;
        case SDLK_A:
            toggleAutoZoom();
            break;
        case SDLK_ESCAPE:
            running=false;
            break;
        default:
            break;
    }
}
void Application::cycleColorScheme(){
    activeScheme=(activeScheme+1)%SCHEME_COUNT;
    std::cout<<"[color] "<<schemes[activeScheme]->name()<<std::endl;
}
void Application::toggleAutoZoom(){
    autoZoomEnabled=!autoZoomEnabled;
    clicker=false;
    finishAnimation();
    lastReframeTicks=SDL_GetTicks();
    std::cout<<"[auto] "<<(autoZoomEnabled?"engaged":"disengaged")<<std::endl;
}
// probes random points in the current bounds and steers toward slow escapers, which hug the filament structure; the precision floor restarts full view so generation cycles forever
void Application::maybeAutoZoom(){
    if(!autoZoomEnabled || clicker || animating){
        return;
    }
    if(SDL_GetTicks()-lastReframeTicks<AUTO_ZOOM_INTERVAL_MS){
        return;
    }
    performAutoZoom();
}
void Application::performAutoZoom(){
    double bestScore=-1.0;
    double bestX=(viewport.getXi()+viewport.getXf())*0.5;
    double bestY=(viewport.getYi()+viewport.getYf())*0.5;
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for(int s=0; s<PROBE_SAMPLES; s++){
        double u=unit(randomEngine);
        double v=unit(randomEngine);
        double cx=viewport.getXi()+u*(viewport.getXf()-viewport.getXi());
        double cy=viewport.getYi()+v*(viewport.getYf()-viewport.getYi());
        double zr=cx;
        double zi=cy;
        int iterations=0;
        while(iterations<PROBE_MAX_ITER && zr*zr+zi*zi<=4.0){
            double nr=zr*zr-zi*zi+cx;
            zi=2.0*zr*zi+cy;
            zr=nr;
            iterations++;
        }
        if(iterations<PROBE_MAX_ITER && static_cast<double>(iterations)>bestScore){
            bestScore=static_cast<double>(iterations);
            bestX=cx;
            bestY=cy;
        }
    }
    double ySpan=std::fabs(viewport.getYf()-viewport.getYi());
    double outerLimit=(2.0*static_cast<double>(windowHeight)/static_cast<double>(windowWidth))*64.0;
    ViewportBounds target=viewport.getBounds();
    if(!(ySpan>=AUTO_ZOOM_MIN_SPAN) || ySpan/AUTO_ZOOM_DIVISOR<AUTO_ZOOM_MIN_SPAN){
        target.xi=-2.0;
        target.xf=2.0;
        target.yi=-2.0*static_cast<double>(windowHeight)/static_cast<double>(windowWidth);
        target.yf=2.0*static_cast<double>(windowHeight)/static_cast<double>(windowWidth);
        std::cout<<"[auto] precision floor reached - restarting cycle"<<std::endl;
    }
    else if(bestScore>=PROBE_MIN_TARGET_ITER){
        target=viewport.planAutoZoom(bestX, bestY, AUTO_ZOOM_DIVISOR);
    }
    else{
        // no candidate escaped slowly enough to promise structure: pull back out instead of diving into interior or exterior dead zones
        if(ySpan>=outerLimit){
            target.xi=-2.0;
            target.xf=2.0;
            target.yi=-2.0*static_cast<double>(windowHeight)/static_cast<double>(windowWidth);
            target.yf=2.0*static_cast<double>(windowHeight)/static_cast<double>(windowWidth);
            std::cout<<"[auto] empty region - restarting cycle"<<std::endl;
        }
        else{
            target=viewport.planAutoZoom((viewport.getXi()+viewport.getXf())*0.5, (viewport.getYi()+viewport.getYf())*0.5, 1.0/ZOOM_OUT_FACTOR);
            std::cout<<"[auto] no structure nearby - zooming out"<<std::endl;
        }
    }
    beginTransition(target);
}
// dives glide a magnifying crop across the pre-zoom frame while the live field resolves at the destination, with one reframe total and no simulation resets mid-flight; pull-backs and targets the frame cannot cover cross through black since nothing beyond the current view is computable
void Application::applyPendingTarget(){
    viewport.setBounds(pendingTarget);
    simulation.reframe(viewport);
    viewport.log(std::cout);
    lastReframeTicks=SDL_GetTicks();
}
void Application::finishAnimation(){
    if(!animating){
        return;
    }
    if(animationMode==2 && !pendingApplied){
        applyPendingTarget();
        pendingApplied=true;
    }
    animating=false;
    SDL_SetTextureAlphaMod(texture, 255);
}
void Application::beginTransition(const ViewportBounds& target){
    finishAnimation();
    ViewportBounds from=viewport.getBounds();
    // a degenerate selection (a double click, for example) has nothing to show and must not reset the running field
    if(!validBounds(from) || !validBounds(target)){
        return;
    }
    animationStartTicks=SDL_GetTicks();
    pendingTarget=target;
    pendingApplied=false;
    // every target the captured frame contains glides - zooms magnify its crop and equal-span selections pan it - while anything wider or offset crosses through black
    if(contains(from, target)){
        flightCapturePending=true;
        animFrom=from;
        animationMode=1;
        animating=true;
        viewport.setBounds(target);
        simulation.reframe(viewport);
        viewport.log(std::cout);
        lastReframeTicks=SDL_GetTicks();
    }
    else{
        animationMode=2;
        animating=true;
    }
}
void Application::drawDive(const SDL_FRect& destination){
    double t=(SDL_GetTicks()-animationStartTicks)/(double)ANIMATION_MS;
    if(t>=1.0 || !animating){
        animating=false;
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        return;
    }
    double e=t*t*(3.0-2.0*t);
    double fromSpan=animFrom.yf-animFrom.yi;
    double fromWidth=animFrom.xf-animFrom.xi;
    double fromCx=(animFrom.xi+animFrom.xf)*0.5;
    double fromCy=(animFrom.yi+animFrom.yf)*0.5;
    ViewportBounds to=viewport.getBounds();
    double toSpan=to.yf-to.yi;
    double span=std::exp((1.0-e)*std::log(fromSpan)+e*std::log(toSpan));
    double cx=(1.0-e)*fromCx+e*(to.xi+(to.xf-to.xi)*0.5);
    double cy=(1.0-e)*fromCy+e*(to.yi+(to.yf-to.yi)*0.5);
    double w=span*(fromWidth/fromSpan);
    float texW=static_cast<float>(bufferWidth);
    float texH=static_cast<float>(bufferHeight);
    SDL_FRect src;
    src.x=texW*static_cast<float>((cx-w*0.5-animFrom.xi)/fromWidth);
    src.y=texH*static_cast<float>((animFrom.yf-(cy+span*0.5))/fromSpan);
    src.w=texW*static_cast<float>(w/fromWidth);
    src.h=texH*static_cast<float>(span/fromSpan);
    if(src.x<0.0f){
        src.x=0.0f;
    }
    if(src.y<0.0f){
        src.y=0.0f;
    }
    if(src.x+src.w>texW){
        src.w=texW-src.x;
    }
    if(src.y+src.h>texH){
        src.h=texH-src.y;
    }
    SDL_RenderTexture(renderer, flightTexture, &src, &destination);
    if(e>CROSSFADE_START){
        double fade=(e-CROSSFADE_START)/(1.0-CROSSFADE_START);
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(fade*255.0));
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
    }
}
void Application::drawFade(const SDL_FRect& destination){
    double t=(SDL_GetTicks()-animationStartTicks)/(double)ANIMATION_MS;
    if(!pendingApplied && t>=0.5){
        applyPendingTarget();
        pendingApplied=true;
    }
    if(t>=1.0 || !animating){
        animating=false;
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        return;
    }
    double alpha=(t<0.5)?(1.0-t/0.5):((t-0.5)/0.5);
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha*255.0));
    SDL_RenderTexture(renderer, texture, nullptr, &destination);
}
void Application::toggleFullscreen(){
    fullscreen=!fullscreen;
    SDL_SetWindowFullscreen(window, fullscreen);
    if(!fullscreen){
        SDL_SetWindowSize(window, 1280, 720);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}
void Application::captureScreenshot(const unsigned char* pixels, int pitch){
    const std::size_t rowBytes=static_cast<std::size_t>(bufferWidth)*4u;
    screenshotPixels.resize(rowBytes*static_cast<std::size_t>(bufferHeight));
    for(int y=0; y<bufferHeight; y++){
        std::memcpy(screenshotPixels.data()+static_cast<std::size_t>(y)*rowBytes, pixels+static_cast<std::size_t>(y)*static_cast<std::size_t>(pitch), rowBytes);
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
    if(writer.save(screenshotPixels.data(), bufferWidth, bufferHeight, name)){
        std::cout<<"Saved "<<name<<std::endl;
#ifdef __EMSCRIPTEN__
        // the browser has no user-visible filesystem: hand the PNG now living in MEMFS to the page as a download
        EM_ASM({
            var bytes=FS.readFile(UTF8ToString($0));
            var url=URL.createObjectURL(new Blob([bytes], {type: "image/png"}));
            var link=document.createElement("a");
            link.href=url;
            link.download=UTF8ToString($0);
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            URL.revokeObjectURL(url);
        }, name);
#endif
    }
    else{
        std::cerr<<"Failed to save "<<name<<std::endl;
    }
    screenshotPixels.clear();
}
void Application::computeDestinationRect(){
    int windowW=0;
    int windowH=0;
    SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
    float bufferW=static_cast<float>(bufferWidth);
    float bufferH=static_cast<float>(bufferHeight);
    scale=(static_cast<float>(windowW)/bufferW<static_cast<float>(windowH)/bufferH)?static_cast<float>(windowW)/bufferW:static_cast<float>(windowH)/bufferH;
    dstW=bufferW*scale;
    dstH=bufferH*scale;
    dstX=(static_cast<float>(windowW)-dstW)*0.5f;
    dstY=(static_cast<float>(windowH)-dstH)*0.5f;
}
