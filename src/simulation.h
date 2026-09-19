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
    long long passesSinceReframe() const;
private:
    void renderRows(int startRow, int endRow, int passCount, long long baseIndex, double toneScale, const ColorScheme* scheme, unsigned char* pixels, int pitch);
    int width;
    int height;
    std::vector<double> z;
    std::vector<double> diverge;
    std::vector<double> planeX;
    std::vector<double> planeY;
    std::vector<double> toneTable;
    long long frameIndex;
    long long reframeIndex;
};
#endif
