#ifndef VIEWPORT_H
#define VIEWPORT_H
#include <iosfwd>
const int RES=2;
class Viewport{
public:
    Viewport(int cssWidth, int cssHeight);
    void beginZoom(double deviceX, double deviceY);
    void completeZoom(double secondDeviceX, double secondDeviceY);
    void autoZoom(double centerX, double centerY, double divisor);
    void resetToInitial();
    double getXi() const;
    double getXf() const;
    double getYi() const;
    double getYf() const;
    double planeFromDeviceX(double deviceX) const;
    double planeFromDeviceY(double deviceY) const;
    void log(std::ostream& stream) const;
private:
    void initializeBounds();
    int cssWidth;
    int cssHeight;
    double aspectRatio;
    double boundXi;
    double boundXf;
    double boundYi;
    double boundYf;
    bool selectionPending;
    double pendingPlaneX;
    double pendingPlaneY;
};
#endif
