#ifndef SIMULATION_H
#define SIMULATION_H
#include <vector>
#include "pixel_buffer.h"
class Viewport;
class ColorScheme;
class Simulation{
public:
    Simulation(int deviceWidth, int deviceHeight);
    void reframe(const Viewport& viewport);
    void step(long long frameIndex, const ColorScheme* scheme);
    const PixelBuffer& getBuffer() const;
private:
    void renderRows(int startRow, int endRow, long long frameIndex, const ColorScheme* scheme);
    int width;
    int height;
    PixelBuffer buffer;
    std::vector<double> container;
    std::vector<double> coords;
    std::vector<double> diverge;
};
#endif
