#ifndef APPLICATION_H
#define APPLICATION_H
#include <random>
#include <vector>
#include "viewport.h"
#include "simulation.h"
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_FRect;
union SDL_Event;
class ColorScheme;
class Application{
public:
    Application(int cssWidth, int cssHeight);
    ~Application();
    bool initialize();
    void run();
private:
    void processEvents();
    void render();
    void onMouseButtonDown(const SDL_Event& event);
    void onKeyDown(const SDL_Event& event);
    void cycleColorScheme();
    void toggleFullscreen();
    void toggleAutoZoom();
    void maybeAutoZoom();
    void performAutoZoom();
    void beginTransition(const ViewportBounds& target);
    void drawDive(const SDL_FRect& destination);
    void drawFade(const SDL_FRect& destination);
    void saveScreenshot();
    void computeDestinationRect();
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Texture* flightTexture;
    Viewport viewport;
    Simulation simulation;
    ColorScheme* schemes[3];
    int activeScheme;
    bool clicker;
    bool running;
    bool fullscreen;
    bool autoZoomEnabled;
    bool animating;
    int animationMode;
    bool pendingApplied;
    unsigned long long startTicks;
    unsigned long long lastReframeTicks;
    unsigned long long animationStartTicks;
    long long consumedTicks;
    ViewportBounds animFrom;
    ViewportBounds pendingTarget;
    std::vector<unsigned char> flightBuffer;
    std::mt19937 randomEngine;
    int windowWidth;
    int windowHeight;
    float dstX;
    float dstY;
    float dstW;
    float dstH;
    float scale;
};
#endif
