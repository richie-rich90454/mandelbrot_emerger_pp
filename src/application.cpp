#include "application.h"
#include "color_scheme.h"
#include "png_writer.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iostream>
#include <thread>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
namespace{
    const unsigned long long AUTO_ZOOM_INTERVAL_MS=6000ull;
    const long long AUTO_ZOOM_MIN_PASSES=600;
    const long long AUTO_ZOOM_MAX_PASSES=1088;
    const int PROBE_SAMPLES=256;
    const int PROBE_HORIZON=4096;
    const long long PROBE_TARGET_BUDGET=1024;
    const long long PROBE_MIN_TARGET_ITER=16;
    const double AUTO_ZOOM_DIVISOR=3.0;
    const double ZOOM_OUT_FACTOR=2.0;
    const unsigned long long MINIMIZED_POLL_MS=50ull;
    const unsigned long long ANIMATION_MS=2500ull;
    const double CROSSFADE_START=0.75;
    const long long REFERENCE_PASS_LIMIT=1ll<<20;
    // supersampling factor relative to the drawn pixels: as large as a core-count-scaled pixel budget allows, so bigger displays and faster machines get more samples and giant displays still run
    double bufferScale(int cssWidth, int cssHeight){
        unsigned int cores=std::thread::hardware_concurrency();
        if(cores==0){
            cores=1;
        }
#ifdef __EMSCRIPTEN__
        const double pixelRatio=EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; });
        const double outputPixels=static_cast<double>(cssWidth)*static_cast<double>(cssHeight)*pixelRatio*pixelRatio;
        const double budgetPixels=static_cast<double>(cores)*300000.0;
#else
        const double outputPixels=static_cast<double>(cssWidth)*static_cast<double>(cssHeight);
        const double budgetPixels=static_cast<double>(cores)*1000000.0;
#endif
        double scale=std::sqrt(budgetPixels/outputPixels);
        scale=(scale>=1.0)?(std::floor(scale*10.0+0.5)/10.0):(std::floor(scale*20.0)/20.0);
        if(scale<0.25){
            scale=0.25;
        }
        if(scale>4.0){
            scale=4.0;
        }
        return scale;
    }
    double outputPixelRatio(){
#ifdef __EMSCRIPTEN__
        return EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; });
#else
        return 1.0;
#endif
    }
    int bufferWidthFor(int cssWidth, int cssHeight){
        return static_cast<int>(std::lround(static_cast<double>(cssWidth)*outputPixelRatio()*bufferScale(cssWidth, cssHeight)));
    }
    int bufferHeightFor(int cssWidth, int cssHeight){
        return static_cast<int>(std::lround(static_cast<double>(cssHeight)*outputPixelRatio()*bufferScale(cssWidth, cssHeight)));
    }
}
Application::Application(int cssWidth, int cssHeight):window(nullptr),renderer(nullptr),texture(nullptr),flightTexture(nullptr),bufferWidth(bufferWidthFor(cssWidth, cssHeight)),bufferHeight(bufferHeightFor(cssWidth, cssHeight)),viewport(cssWidth, cssHeight),simulation(bufferWidth, bufferHeight),schemes{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},activeScheme(6),clicker(false),running(false),fullscreen(true),autoZoomEnabled(true),animating(false),flightCapturePending(false),screenshotRequested(false),animationMode(0),pendingApplied(false),lastReframeTicks(0),animationStartTicks(0),animFrom(),pendingTarget(),pendingRefU(0.5),pendingRefV(0.5),lastRefU(0.5),lastRefV(0.5),nextZoomPasses(AUTO_ZOOM_MIN_PASSES),referenceRebases(0),randomEngine(std::random_device{}()),windowWidth(cssWidth),windowHeight(cssHeight),dstX(0.0f),dstY(0.0f),dstW(0.0f),dstH(0.0f),scale(1.0f){
    viewport.setDeviceSize(bufferWidth, bufferHeight);
    schemes[0]=new GrayscaleScheme();
    schemes[1]=new ThermalScheme();
    schemes[2]=new AlphaScheme();
    schemes[3]=new RainbowScheme();
    schemes[4]=new FireScheme();
    schemes[5]=new IceScheme();
    schemes[6]=new AmberScheme();
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
    flightTexture=SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, bufferWidth, bufferHeight);
    if(flightTexture==nullptr){
        std::cerr<<"SDL_CreateTexture(flight) failed: "<<SDL_GetError()<<std::endl;
        return false;
    }
    return true;
}
void Application::run(){
    running=true;
    lastReframeTicks=SDL_GetTicks();
    simulation.reframe(viewport, lastRefU, lastRefV);
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
        SDL_Delay(MINIMIZED_POLL_MS);
        return;
    }
    if(flightCapturePending){
        // copy the frame the gpu is showing into the flight texture: locking a streaming texture hands back a fresh staging buffer on direct3d, so its pixels are undefined
        SDL_SetRenderTarget(renderer, flightTexture);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_SetRenderTarget(renderer, nullptr);
        flightCapturePending=false;
    }
    // the simulation writes straight into the locked streaming texture, so a batch never costs an extra frame copy
    void* lockedPixels=nullptr;
    int lockedPitch=0;
    if(SDL_LockTexture(texture, nullptr, &lockedPixels, &lockedPitch)){
        unsigned char* pixels=static_cast<unsigned char*>(lockedPixels);
        simulation.step(1, schemes[activeScheme], pixels, lockedPitch);
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
    if(simulation.referenceEscaped()){
        handleReferenceEscape();
    }
    else if(simulation.passesSinceReframe()>REFERENCE_PASS_LIMIT){
        simulation.reframe(viewport, lastRefU, lastRefV);
        lastReframeTicks=SDL_GetTicks();
        std::cout<<"[ref] reference pass limit reached - reference orbit reset"<<std::endl;
    }
}
bool Application::contains(const ViewportBounds& outer, const ViewportBounds& inner){
    return inner.xi>=outer.xi && inner.xf<=outer.xf && inner.yi>=outer.yi && inner.yf<=outer.yf;
}
bool Application::validBounds(const ViewportBounds& bounds){
    return bounds.xf>bounds.xi && bounds.yf>bounds.yi;
}
ViewportBounds Application::clampToInitial(const ViewportBounds& bounds) const{
    ViewportBounds initial=viewport.initialBounds();
    if(bounds.xi<initial.xi || bounds.xf>initial.xf || bounds.yi<initial.yi || bounds.yf>initial.yf){
        return initial;
    }
    return bounds;
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
void Application::maybeAutoZoom(){
    if(!autoZoomEnabled || clicker || animating){
        return;
    }
    // a zoom only starts once the view has resolved enough passes to show its structure, never on a half-drawn frame;
    // the bar rises with depth so slow-escaped detail is visible before the camera moves on
    if(simulation.passesSinceReframe()<nextZoomPasses){
        return;
    }
    if(SDL_GetTicks()-lastReframeTicks<AUTO_ZOOM_INTERVAL_MS){
        return;
    }
    performAutoZoom();
}
// probes read survival off the reference orbit in double, so they resolve structure at any depth. The camera
// follows the slowest escaper inside the budget (visible before the epoch ends) while the reference orbit is
// anchored to a full-horizon survivor, or failing that the slowest escaper of all: the wait never outlives that
// point's own lifetime, so the perturbation stays valid for the whole epoch
void Application::performAutoZoom(){
    ProbeResult probe=simulation.probeStructure(PROBE_SAMPLES, PROBE_HORIZON, PROBE_TARGET_BUDGET, PROBE_MIN_TARGET_ITER, randomEngine);
    ViewportBounds target=viewport.getBounds();
    double referenceU=0.5;
    double referenceV=0.5;
    long long requiredPasses=AUTO_ZOOM_MIN_PASSES;
    double targetU=0.5;
    double targetV=0.5;
    bool haveTarget=false;
    if(probe.hasStructure){
        haveTarget=true;
        targetU=probe.targetU;
        targetV=probe.targetV;
    }
    else if(probe.hasSlowest){
        haveTarget=true;
        targetU=probe.slowestU;
        targetV=probe.slowestV;
    }
    else if(probe.hasSurvivor){
        haveTarget=true;
        targetU=probe.survivorU;
        targetV=probe.survivorV;
    }
    if(!haveTarget){
        // no slow escaper promises structure: pull back, bounded by the full view, instead of diving into a dead zone
        target=clampToInitial(viewport.scaledBounds(ZOOM_OUT_FACTOR));
        nextZoomPasses=requiredPasses;
        std::cout<<"[auto] no structure nearby - zooming out"<<std::endl;
        beginTransition(target, referenceU, referenceV);
        return;
    }
    if(probe.hasSurvivor){
        requiredPasses=AUTO_ZOOM_MAX_PASSES;
    }
    else{
        requiredPasses=std::min<long long>(AUTO_ZOOM_MAX_PASSES, std::max<long long>(64, probe.slowestIterations-16));
    }
    Big centerX=viewport.planeAtUnitX(targetU);
    Big centerY=viewport.planeAtUnitY(targetV);
    target=viewport.planAutoZoom(centerX, centerY, AUTO_ZOOM_DIVISOR);
    double referencePointU=probe.hasSurvivor?probe.survivorU:probe.slowestU;
    double referencePointV=probe.hasSurvivor?probe.survivorV:probe.slowestV;
    Big referenceX=viewport.planeAtUnitX(referencePointU);
    Big referenceY=viewport.planeAtUnitY(referencePointV);
    double width=Big::sub(target.xf, target.xi).toDouble();
    double height=Big::sub(target.yf, target.yi).toDouble();
    double dx=Big::sub(referenceX, target.xi).toDouble();
    double dy=Big::sub(referenceY, target.yi).toDouble();
    if(width>0.0 && height>0.0 && std::isfinite(dx) && std::isfinite(dy)){
        referenceU=dx/width;
        referenceV=dy/height;
    }
    nextZoomPasses=requiredPasses;
    beginTransition(target, referenceU, referenceV);
}
// a reference orbit that escapes mid-epoch invalidates the perturbation for still-active pixels; rebase onto the
// pixel closest to the set, and if that keeps dying, widen the view so fresh territory enters the probe
void Application::handleReferenceEscape(){
    if(!simulation.referenceEscaped()){
        return;
    }
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double referenceU=0.5;
    double referenceV=0.5;
    if(referenceRebases==0){
        if(!simulation.bestReferenceOffset(referenceU, referenceV)){
            referenceU=unit(randomEngine);
            referenceV=unit(randomEngine);
        }
        simulation.reframe(viewport, referenceU, referenceV);
        std::cout<<"[ref] reference orbit escaped - rebasing onto a longer-lived point"<<std::endl;
        referenceRebases=1;
    }
    else{
        viewport.setBounds(clampToInitial(viewport.scaledBounds(ZOOM_OUT_FACTOR)));
        referenceU=unit(randomEngine);
        referenceV=unit(randomEngine);
        simulation.reframe(viewport, referenceU, referenceV);
        std::cout<<"[ref] reference orbit escaped again - widening view and rebasing"<<std::endl;
        referenceRebases=0;
    }
    lastRefU=referenceU;
    lastRefV=referenceV;
    viewport.log(std::cout);
    lastReframeTicks=SDL_GetTicks();
}
// dives glide a magnifying crop across the pre-zoom frame while the live field resolves at the destination, with one reframe total and no simulation resets mid-flight; pull-backs and targets the frame cannot cover cross through black since nothing beyond the current view is computable
void Application::applyPendingTarget(){
    viewport.setBounds(pendingTarget);
    simulation.reframe(viewport, pendingRefU, pendingRefV);
    lastRefU=pendingRefU;
    lastRefV=pendingRefV;
    referenceRebases=0;
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
void Application::beginTransition(const ViewportBounds& target, double referenceU, double referenceV){
    finishAnimation();
    ViewportBounds from=viewport.getBounds();
    // a degenerate selection (a double click, for example) has nothing to show and must not reset the running field
    if(!validBounds(from) || !validBounds(target)){
        return;
    }
    animationStartTicks=SDL_GetTicks();
    pendingTarget=target;
    pendingRefU=referenceU;
    pendingRefV=referenceV;
    pendingApplied=false;
    referenceRebases=0;
    // every target the captured frame contains glides - zooms magnify its crop and equal-span selections pan it - while anything wider or offset crosses through black
    if(contains(from, target)){
        flightCapturePending=true;
        animFrom=from;
        animationMode=1;
        animating=true;
        viewport.setBounds(target);
        simulation.reframe(viewport, referenceU, referenceV);
        lastRefU=referenceU;
        lastRefV=referenceV;
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
    ViewportBounds to=viewport.getBounds();
    double fromWidth=Big::sub(animFrom.xf, animFrom.xi).toDouble();
    double fromHeight=Big::sub(animFrom.yf, animFrom.yi).toDouble();
    double toHeight=Big::sub(to.yf, to.yi).toDouble();
    // underflowed spans cannot be cropped as doubles; land on the fresh field directly instead of mis-cropping
    if(!(fromWidth>0.0) || !(fromHeight>0.0) || !(toHeight>0.0)){
        animating=false;
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        return;
    }
    double toWidth=Big::sub(to.xf, to.xi).toDouble();
    if(!(toWidth>0.0)){
        animating=false;
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderTexture(renderer, texture, nullptr, &destination);
        return;
    }
    double span=std::exp((1.0-e)*std::log(fromHeight)+e*std::log(toHeight));
    Big fromCenterX=Big::mul(Big::add(animFrom.xi, animFrom.xf), 0.5);
    Big fromCenterY=Big::mul(Big::add(animFrom.yi, animFrom.yf), 0.5);
    Big toCenterX=Big::mul(Big::add(to.xi, to.xf), 0.5);
    Big toCenterY=Big::mul(Big::add(to.yi, to.yf), 0.5);
    Big centerX=Big::add(fromCenterX, Big::mul(Big::sub(toCenterX, fromCenterX), e));
    Big centerY=Big::add(fromCenterY, Big::mul(Big::sub(toCenterY, fromCenterY), e));
    double w=span*(fromWidth/fromHeight);
    double centerXFrac=Big::sub(centerX, animFrom.xi).toDouble()/fromWidth;
    double centerYFrac=Big::sub(animFrom.yf, centerY).toDouble()/fromHeight;
    float texW=static_cast<float>(bufferWidth);
    float texH=static_cast<float>(bufferHeight);
    SDL_FRect src;
    src.x=texW*static_cast<float>(centerXFrac-w*0.5/fromWidth);
    src.y=texH*static_cast<float>(centerYFrac-span*0.5/fromHeight);
    src.w=texW*static_cast<float>(w/fromWidth);
    src.h=texH*static_cast<float>(span/fromHeight);
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
