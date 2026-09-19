#ifndef VIEWPORT_H
#define VIEWPORT_H
#include <iosfwd>
#include "big.h"
struct ViewportBounds{
    Big xi;
    Big xf;
    Big yi;
    Big yf;
};
class Viewport{
public:
    Viewport(int cssWidth, int cssHeight);
    void setDeviceSize(int width, int height);
    void beginZoom(double deviceX, double deviceY);
    ViewportBounds completeZoom(double secondDeviceX, double secondDeviceY);
    ViewportBounds planAutoZoom(const Big& centerX, const Big& centerY, double divisor);
    ViewportBounds scaledBounds(double factor);
    ViewportBounds initialBounds() const;
    void setBounds(const ViewportBounds& bounds);
    ViewportBounds getBounds() const;
    Big getXi() const;
    Big getXf() const;
    Big getYi() const;
    Big getYf() const;
    Big planeFromDeviceX(double deviceX) const;
    Big planeFromDeviceY(double deviceY) const;
    Big planeAtUnitX(double u) const;
    Big planeAtUnitY(double v) const;
    double spanX() const;
    double spanY() const;
    void log(std::ostream& stream) const;
private:
    void initializeBounds();
    void ensurePrecision(double hintSpan);
    int cssWidth;
    int cssHeight;
    int deviceWidth;
    int deviceHeight;
    double aspectRatio;
    Big boundXi;
    Big boundXf;
    Big boundYi;
    Big boundYf;
    bool selectionPending;
    Big pendingPlaneX;
    Big pendingPlaneY;
};
#endif
