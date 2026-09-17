#ifndef SIMULATION_H
#define SIMULATION_H
#include <vector>
class Viewport;
class ColorScheme;
class Simulation{
public:
    Simulation(int deviceWidth, int deviceHeight);
    void reframe(const Viewport& viewport);
    void step(int passCount, const ColorScheme* scheme, unsigned char* pixels, int pitch);
private:
    void renderRows(int startRow, int endRow, int passCount, long long baseIndex, long long divisor, const ColorScheme* scheme, unsigned char* pixels, int pitch);
    int width;
    int height;
    std::vector<double> z;
    std::vector<double> diverge;
    std::vector<double> planeX;
    std::vector<double> planeY;
    long long frameIndex;
};
#endif
