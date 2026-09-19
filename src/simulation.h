#ifndef SIMULATION_H
#define SIMULATION_H
#include <random>
#include <vector>
#include "big.h"
class Viewport;
class ColorScheme;
struct ProbeResult{
    bool hasStructure=false;
    double targetU=0.5;
    double targetV=0.5;
    long long targetIterations=0;
    bool hasSlowest=false;
    double slowestU=0.5;
    double slowestV=0.5;
    long long slowestIterations=0;
    bool hasSurvivor=false;
    double survivorU=0.5;
    double survivorV=0.5;
};
class Simulation{
public:
    Simulation(int deviceWidth, int deviceHeight);
    void reframe(const Viewport& viewport, double referenceU=0.5, double referenceV=0.5);
    void step(int passCount, const ColorScheme* scheme, unsigned char* pixels, int pitch);
    long long passesSinceReframe() const;
    double escapeCountAt(int x, int y) const;
    bool referenceEscaped() const;
    bool bestReferenceOffset(double& u, double& v) const;
    ProbeResult probeStructure(int samples, int horizon, long long targetBudget, long long minTargetIterations, std::mt19937& randomEngine);
private:
    void renderRows(int startRow, int endRow, int passCount, long long baseIndex, double toneScale, const ColorScheme* scheme, unsigned char* pixels, int pitch);
    void extendReference(long long count);
    void appendReferenceEntry();
    int width;
    int height;
    std::vector<double> w;
    std::vector<unsigned int> refIndex;
    std::vector<double> diverge;
    std::vector<double> dcX;
    std::vector<double> dcY;
    std::vector<double> toneTable;
    double spanX;
    double spanY;
    double refU;
    double refV;
    Big cRefRe;
    Big cRefIm;
    Big orbitRe;
    Big orbitIm;
    std::vector<double> refZr;
    std::vector<double> refZi;
    std::vector<double> refD;
    long long frameIndex;
    long long reframeIndex;
    bool refEscaped;
};
#endif
